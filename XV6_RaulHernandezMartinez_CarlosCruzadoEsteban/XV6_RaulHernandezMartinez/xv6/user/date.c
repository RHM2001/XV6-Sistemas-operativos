#include "types.h"
#include "user.h"
#include "date.h"

int main(int argc, char *argv[]){
    struct rtcdate r; // del "date.h"
    if(date (&r)){
        printf(2, "date failed\n");
        exit (0) ;
    }

    printf(1, "Day:%d, Month:%d, Year:%d - Hour:%d, Min:%d, Sec:%d\n", r.day, r.month, r.year, r.hour, r.minute, r.second);
    exit (0) ;
}