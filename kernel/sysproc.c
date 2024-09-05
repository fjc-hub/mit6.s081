#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)
{
  int n;
  if(argint(0, &n) < 0)
    return -1;
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  if(argaddr(0, &p) < 0)
    return -1;
  return wait(p);
}

uint64
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
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

  backtrace();
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

uint64 sys_sigalarm(void) {
  int ntick;
  uint64 fn;
  struct proc* p = myproc();

  if(argint(0, &ntick) < 0) return -1;
  if(argaddr(1, &fn) < 0) return -1;

  if (ntick <= 0 && fn == 0) {
    p->fnable = 0; // stop alarm
    return 0;
  }

  p->ntick = ntick;
  p->alarm_fn = fn;

  p->consumeTicks = 0;
  if (p->fnable == 0) // except that alarm function running
    p->fnable = 1;

  return 0;
}

uint64 sys_sigreturn(void) {
  struct proc* p = myproc();

  if (p->fnable == 2) {
    p->fnable = 1;
    p->consumeTicks = 0;
    // resume all register (representing A history Process-Status)
    // go back a history status of the process
    p->trapframe->epc = p->alarmresumeregister->epc;
    p->trapframe->ra = p->alarmresumeregister->ra;
    p->trapframe->sp = p->alarmresumeregister->sp;
    p->trapframe->gp = p->alarmresumeregister->gp;
    p->trapframe->tp = p->alarmresumeregister->tp;
    p->trapframe->t0 = p->alarmresumeregister->t0;
    p->trapframe->t1 = p->alarmresumeregister->t1;
    p->trapframe->t2 = p->alarmresumeregister->t2;
    p->trapframe->s0 = p->alarmresumeregister->s0;
    p->trapframe->s1 = p->alarmresumeregister->s1;
    p->trapframe->a0 = p->alarmresumeregister->a0;
    p->trapframe->a1 = p->alarmresumeregister->a1;
    p->trapframe->a2 = p->alarmresumeregister->a2;
    p->trapframe->a3 = p->alarmresumeregister->a3;
    p->trapframe->a4 = p->alarmresumeregister->a4;
    p->trapframe->a5 = p->alarmresumeregister->a5;
    p->trapframe->a6 = p->alarmresumeregister->a6;
    p->trapframe->a7 = p->alarmresumeregister->a7;
    p->trapframe->s2 = p->alarmresumeregister->s2;
    p->trapframe->s3 = p->alarmresumeregister->s3;
    p->trapframe->s4 = p->alarmresumeregister->s4;
    p->trapframe->s5 = p->alarmresumeregister->s5;
    p->trapframe->s6 = p->alarmresumeregister->s6;
    p->trapframe->s7 = p->alarmresumeregister->s7;
    p->trapframe->s8 = p->alarmresumeregister->s8;
    p->trapframe->s9 = p->alarmresumeregister->s9;
    p->trapframe->s10 = p->alarmresumeregister->s10;
    p->trapframe->s11 = p->alarmresumeregister->s11;
    p->trapframe->t3 = p->alarmresumeregister->t3;
    p->trapframe->t4 = p->alarmresumeregister->t4;
    p->trapframe->t5 = p->alarmresumeregister->t5;
    p->trapframe->t6 = p->alarmresumeregister->t6;
  }
  
  return 0;
}