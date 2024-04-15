#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "traps.h"
#include "spinlock.h"

// Necesitarás llamar a mappages(). Para ello borra la declaración de función
// estática en vm.c y declara mappages() en trap.c
extern int mappages(pde_t *pgdir, void *va, uint size, uint pa, int perm);

// Interrupt descriptor table (shared by all CPUs).
struct gatedesc idt[256];
extern uint vectors[];  // in vectors.S: array of 256 entry pointers
struct spinlock tickslock;
uint ticks;

void
tvinit(void)
{
  int i;

  for(i = 0; i < 256; i++)
    SETGATE(idt[i], 0, SEG_KCODE<<3, vectors[i], 0);
  SETGATE(idt[T_SYSCALL], 1, SEG_KCODE<<3, vectors[T_SYSCALL], DPL_USER);

  initlock(&tickslock, "time");
}

void
idtinit(void)
{
  lidt(idt, sizeof(idt));
}

//PAGEBREAK: 41
void
trap(struct trapframe *tf)
{
  if(tf->trapno == T_SYSCALL){
    if(myproc()->killed)
      // Si no se pone un valor dentro de exit da error de compilacion
      exit(-1);
    myproc()->tf = tf;
    syscall();
    if(myproc()->killed)
      // Si no se pone un valor dentro de exit da error de compilacion
      exit(-1);
    return;
  }

  switch(tf->trapno){
  case T_IRQ0 + IRQ_TIMER:
    if(cpuid() == 0){
      acquire(&tickslock);
      ticks++;
      wakeup(&ticks);
      release(&tickslock);
    }
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_IDE:
    ideintr();
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_IDE+1:
    // Bochs generates spurious IDE1 interrupts.
    break;
  case T_IRQ0 + IRQ_KBD:
    kbdintr();
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_COM1:
    uartintr();
    lapiceoi();
    break;
  case T_IRQ0 + 7:
  case T_IRQ0 + IRQ_SPURIOUS:
    cprintf("cpu%d: spurious interrupt at %x:%x\n",
            cpuid(), tf->cs, tf->eip);
    lapiceoi();
    break;
  // Responder a un fallo de página en el espacio
  // de usuario mapeando una nueva página física en la dirección que generó el
  // fallo, regresando después al espacio de usuario para que el proceso continúe
  case T_PGFLT:
    cprintf("Entra al case T_PGFLT\n");

    // Extraemos la dirección de fallo
    uint direccion_fallo = rcr2(); 

    // La direccion de fallo no tiene que sobrepasar el tamaño que ya incrementamos en la llamada a sbrk()
    if(direccion_fallo >= myproc()->sz){ 
      myproc()->killed = 1;
      break;
    }

    // Usa PGROUNDDOWN(va) para redondear la dirección virtual a límite de página
    // Hay que comprobar de que el fallo de página no es por la página de guarda.
    if (PGROUNDDOWN(rcr2()) == myproc()->pagina_de_guarda){			
      myproc()->killed = 1;
      break;
    }        

    // Redondeamos la dirección virtual al límite de página
    uint pagina_fallo = PGROUNDDOWN(direccion_fallo);  

    // Asignamos una nueva página física desde la memoria libre    
    char* p = kalloc(); 

    if(p == 0){
      // Comprobamos que tengamos memoria libre disponible, y sino salimos del proceso.
      exit(-1);
    } 

    // Limpiamos la página asignada
    memset(p, 0, PGSIZE);

    // Mapeamos las nuevas páginas del proceso
    // "V2p" nos traduce la dirección virtual a física
    if(mappages(myproc()->pgdir, (char*)pagina_fallo, PGSIZE, V2P(p), PTE_W|PTE_U) < 0){ 
      // Si falla el mapeo, liberar la página asignada, marcar el proceso como terminado y salir del ciclo
      kfree(p);
      myproc()->killed = 1; 
      break;
    }
    
    break;
  //PAGEBREAK: 13
  default:
    if(myproc() == 0 || (tf->cs&3) == 0){
      // In kernel, it must be our mistake.
      cprintf("unexpected trap %d from cpu %d eip %x (cr2=0x%x)\n",
              tf->trapno, cpuid(), tf->eip, rcr2());
      panic("trap");
    }
    // In user space, assume process misbehaved. ( MUESTRA ESTE CPRINTF - el de "echo hola")
    cprintf("pid %d %s: trap %d err %d on cpu %d "
            "eip 0x%x addr 0x%x--kill proc\n",
            myproc()->pid, myproc()->name, tf->trapno,
            tf->err, cpuid(), tf->eip, rcr2());
    myproc()->killed = 1;
  }

  // Force process exit if it has been killed and is in user space.
  // (If it is still executing in the kernel, let it keep running
  // until it gets to the regular system call return.)

  // Sumamos +1 al trap
  if(myproc() && myproc()->killed && (tf->cs&3) == DPL_USER)
    exit(tf->trapno + 1);

  // Force process to give up CPU on clock tick.
  // If interrupts were on while locks held, would need to check nlock.
  if(myproc() && myproc()->state == RUNNING &&
     tf->trapno == T_IRQ0+IRQ_TIMER)
    yield();

  // Check if the process has been killed since we yielded

  // Sumamos +1 al trap
  if(myproc() && myproc()->killed && (tf->cs&3) == DPL_USER)
    exit(tf->trapno + 1);
}
