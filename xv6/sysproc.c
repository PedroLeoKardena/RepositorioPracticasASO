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
  myproc()->exitstatus = (status & 0xff) << 8;  // Se pone el estado del proceso en los 8 bits mas significativos. (Porque los menos significativos se usan para el trap).
  exit();
  return 0;  // not reached
}

int
sys_wait(void)
{
  int *status;
  int arg;
  if(argint(0, &arg) < 0)
    return -1;
  status = (int *)arg;
  if(status!=0 && argptr(0, (void**)&status, sizeof(int)) < 0)
    return -1;  
  return wait(status);
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
  int n; //size a crecer el nuevo proceso

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  //sz proceso = addr. Devolvemos el size del proceso por que indica la direccion de memoria que apunta al comienzo de nuestro espacio en memoria (tras hacer
		       //malloc).
  //growproc hace crecer el proceso en n bytes. Ejercicio 1 nos obliga a no usarlo.
  //if(growproc(n) < 0)
  //  return -1;
  //Como no lo usamos, si hacemos solo myproc()->sz+=n esto aumenta sz pero no aparecera en la tabla de paginas. Lo que debemos hacer es forzar el error de tabla de paginas.
  myproc()->sz+=n;
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
