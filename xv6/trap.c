#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "traps.h"
#include "spinlock.h"

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
      exit();
    myproc()->tf = tf;
    syscall();
    if(myproc()->killed)
      exit();
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
  case T_PGFLT:
    //Aqui que es cuando salta la excepcion por falta de memoria debemos dar la memoria (tratar el error de paginacion) 

    uint va = rcr2(); // Dirección virtual que causó el fallo
    //Explicación Ejercicio 2:
    //En el ejercicio 2 tenemos que asegurarnos de hacer ciertas validaciones de seguridad:
    //Primero: El proceso debe existir.
    //Segundo: va < p->sz: La dirección debe estar dentro del tamaño declarado del proceso.
    //Tercero: va >= PGROUNDDOWN(tf->esp): Protección de la página de guarda.

    //Si la dirección está DEBAJO de la pila, es un desbordamiento de pila, no memoria heap válida.
    //Comprobamos myproc() == 0, que la direccion virtual no supere el tamaño declarado del proceso 
    //y por ultimo nos aseguramos que el stack pointer (myproc()->tf->esp). Esta ultima comprobación soluciona el problema:
    //Comprobar y manejar los casos de fallos en los que la página inválida está debajo de la pila.
    
    //Estas comprobacioens basicamente lo que nos permiten es asegurarnos de que: la direccion virtual no supere la memoria
    //que el proceso a pedido a través del sys_sbrk y que no se vaya por debajo de la pila (recordar que la pila crece hacia arriba)
    if(myproc() == 0 || va >= myproc()->sz || va < PGROUNDDOWN(myproc()->tf->esp)){
      cprintf("pid %d %s: segfault lazy alloc failed va=0x%x ip=0x%x\n",
              myproc()->pid, myproc()->name, va, tf->eip);
      myproc()->killed = 1; //Matamos el proceso
      //Falla en PG_FAULT -> debemos asignarle código de salida de pg_fault.
      myproc()->exitstatus = tf->trapno+1;
      break;
    }

    //Nos basamos en codigo allocuvm
    //char *mem = kalloc(); Preguntar si es necesario
    //Ahora podemos intentar reservar memoria fisica:
    char* mem = kalloc();
    //Comprobamos de que no nos quedamos sin memoria
    if(mem == 0){
      cprintf("lazy alloc: out of memory\n");
      myproc()->killed = 1;
      myproc()->exitstatus = tf->trapno+1;
      break;
    }
    //Limpiamos la página.
    memset(mem, 0, PGSIZE);

    //Ahora si que si, mapeamos con mappages.
    //mappages en el que pasamos el procesos rcr2() = 4004
    //mappages(myproc()->pgdir,(char *)PGROUNDDOWN(rcr2()), PGSIZE, V2P(mem), PTE_W|PTE_U);
    //Tambien vamos a comprobar que mappages no falle. 
    if(mappages(myproc()->pgdir, (char *)PGROUNDDOWN(va), PGSIZE, V2P(mem), PTE_W|PTE_U) < 0){
      cprintf("lazy alloc: mappages failed\n");
      kfree(mem);
      myproc()->killed = 1;
      myproc()->exitstatus = tf->trapno+1;
      break;
    }
    
    break;
  //PAGEBREAK: 13
  default:
    //myproc() == 0 es modo kernel. Si salta un trap en modo kernel -> panic = matar todos los procesos.
    if(myproc() == 0 || (tf->cs&3) == 0){
      // In kernel, it must be our mistake.
      cprintf("unexpected trap %d from cpu %d eip %x (cr2=0x%x)\n",
              tf->trapno, cpuid(), tf->eip, rcr2());
      panic("trap");
    }
    // In user space, assume process misbehaved.
    cprintf("pid %d %s: trap %d err %d on cpu %d "
            "eip 0x%x addr 0x%x--kill proc\n",
            myproc()->pid, myproc()->name, tf->trapno,
            tf->err, cpuid(), tf->eip, rcr2());
    myproc()->killed = 1;
    myproc()->exitstatus = ((tf->trapno + 1) & 0x7f);   // Codigo de salida por trap+1 con los 7 menos significativos
  }

  // Force process exit if it has been killed and is in user space.
  // (If it is still executing in the kernel, let it keep running
  // until it gets to the regular system call return.)
  if(myproc() && myproc()->killed && (tf->cs&3) == DPL_USER)
    exit();
    
  // Force process to give up CPU on clock tick.
  // If interrupts were on while locks held, would need to check nlock.
  if(myproc() && myproc()->state == RUNNING &&
     tf->trapno == T_IRQ0+IRQ_TIMER)
    yield();

  // Check if the process has been killed since we yielded
  if(myproc() && myproc()->killed && (tf->cs&3) == DPL_USER)
    exit();
}
