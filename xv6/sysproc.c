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

int
sys_exit(void)
{
  int status;
  if(argint(0, &status) < 0)
    return -1;
  
  //Nosotros lo que tenemos que hacer es modificar esta llamada exit para que reciba el parametro status. Esta llamada es invocada por sys_exit (salida correcta) y por trap 
  //(exit "incorrecto") funcion en trap.c, y dependiendo de quien llame lo tratamos de una manera u otra. Hay que usar las macros definidas en user/user.h.
  //Una vez que hayamos tratado todo esto entonces podremos trabajar con sys_wait.
  exit();
  return 0;  // not reached
}

int
sys_wait(void)
{
  int *status;
  
  if(argptr(0, (void**)&status, sizeof(int)) < 0)
    return -1;

  return wait();
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

int
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  //if(growproc(n) < 0)
    //return -1;
  myproc()->sz += n;
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
  struct rtcdate *d;
  
  if (argptr(0, (void **) &d, sizeof(struct rtcdate)) < 0)
    return -1;
  
  cmostime(d);
  return 0;  
}
