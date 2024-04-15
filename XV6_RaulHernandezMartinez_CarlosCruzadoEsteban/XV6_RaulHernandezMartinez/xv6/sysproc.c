#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"

int
sys_fork(void)
{
  return fork();
}

// También hay que adaptar las correspondientes funciones de implementación
// de las llamadas dentro del núcleo, sys_exit() y sys_wait() para aceptar,
// respectivamente, un entero (el estado de salida del proceso) y un puntero a
// enteros (obsérvese el uso de argptr() dentro del núcleo)

int
sys_exit(void)
{
  int estado;

  if(argint(0, &estado) < 0){
    return -1;
  }

  // Desplazamos el estado 8 bytes para cumplir con los macros que se proporcionan en las diapositivas.
  // Si no se añade da "failure" en el test.
  exit(estado << 8);
  return 0;  // not reached
}

int
sys_wait(void)
{
  int * estado;

  if(argptr(0, (void **)&estado, sizeof(int)) < 0){
    
    return -1;
  }

  return wait(estado);
}

int
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

int
sys_getpid(void)
{
  return myproc()->pid;
}

// Elimina la reserva de páginas de la llamada al sistema sbrk() (implementada
// a través de la función sys_sbrk() en sysproc.c)

int
sys_sbrk(void)
{
  int addr;
  int n; // -> número de bytes

  if(argint(0, &n) < 0)
    return -1;

  addr = myproc()->sz;

  // La nueva función debe incrementar el tamaño del proceso (proc->sz) y devolver
  // el tamaño antiguo pero no debe reservar memoria

  if(n >= 0){
    myproc()->sz = myproc()->sz + n;
  }
  // No se debe llamar a growproc() en caso de que el proceso crezca
  else{
    if(growproc(n) < 0){
      return -1;
    }
  }
  /*if(growproc(n) < 0)
    return -1;*/

  return addr;
}

int
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

// return how many clock tick interrupts have occurred
// since start.
int
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

int
sys_date(void)
{
  // Recoger el parámetro struct rtcdate* de la primera posición de la pila.
  struct rtcdate *r;

  // Comprobar todos los errores y retornar -1 en caso de error.
  if(argptr(0, (void ** ) &r, sizeof(struct rtcdate)) < 0){
    return -1;
  }

  // Llamar a la función cmostime() con ese puntero para obtener la fecha.
  cmostime(r);
  return 0;
}

int
sys_getprio(void)
{
  int pid;
  if(argint(0,&pid)<0)
    return -1;
  return getprio(pid);
}

int 
sys_setprio(void)
{
  int pid;
  int prio;
  if(argint(0,&pid)<0 || argint(1,&prio)<0)
    return -1;
  return setprio(pid,prio);
}
