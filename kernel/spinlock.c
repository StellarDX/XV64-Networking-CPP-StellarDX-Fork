// Mutual exclusion spin locks.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "x86.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "spinlock.h"

void initlock(struct spinlock* lk, char* name){
	lk->name = name;
	lk->locked = 0;
	lk->cpu = 0;
}


void acquire(struct spinlock* lk){
	uint8 success = sacquire(lk, UINT32_MAX);
	if(!success){
		panic("acquire: failed to acquire lock");
	}
}

// Acquire the lock.
// Loops (spins) until the lock is acquired.
// Holding a lock for a long time may cause
// other CPUs to waste time spinning to acquire it.
uint8 sacquire(struct spinlock* lk, uint32 wait){
	pushcli(); // disable interrupts to avoid deadlock.
	if (holding(lk)) {
		int i;
		cprintf("lock '%s':\n", lk->name);
		for (i = 0; i < 10; i++)
			cprintf(" %p", lk->pcs[i]);
		cprintf("\n");
		panic("acquire: already holding lock");
	}

	// The xchg is atomic.
	// It also serializes, so that reads after acquire are not
	// reordered before it.
	uint32 startticks = ticks;
	while (xchg(&lk->locked, 1) != 0) {
		uint32 delta = ticks - startticks;
		if(delta > wait){
			return 0;
		}
	}

	// Record info about lock acquisition for debugging.
	lk->cpu = cpu;
	getcallerpcs(&lk, lk->pcs);
	return 1;
}

// Release the lock.
void release(struct spinlock* lk){
	if (!holding(lk))
		panic("release");

	lk->pcs[0] = 0;
	lk->cpu = 0;

	// The xchg serializes, so that reads before release are
	// not reordered after it.  The 1996 PentiumPro manual (Volume 3,
	// 7.2) says reads can be carried out speculatively and in
	// any order, which implies we need to serialize here.
	// But the 2007 Intel 64 Architecture Memory Ordering White
	// Paper says that Intel 64 and IA-32 will not move a load
	// after a store. So lock->locked = 0 would work here.
	// The xchg being asm volatile ensures gcc emits it after
	// the above assignments (and after the critical section).
	xchg(&lk->locked, 0);

	popcli();
}

// Record the current call stack in pcs[] by following the %ebp chain.
void getcallerpcs(void* v, uintp pcs[]){
	uintp* ebp;
	//https://stackoverflow.com/questions/41912684/what-is-the-purpose-of-the-rbp-register-in-x86-64-assembler
	asm volatile ("mov %%rbp, %0" : "=r" (ebp));
	getstackpcs(ebp, pcs);
}

void getstackpcs(uintp* ebp, uintp pcs[]){
	int i;

	for (i = 0; i < 10; i++) {
		if (ebp == 0 || ebp < (uintp*)KERNBASE || ebp == (uintp*)0xffffffff)
			break;
		pcs[i] = ebp[1]; // saved %eip
		ebp = (uintp*)ebp[0]; // saved %ebp
	}
	for (; i < 10; i++)
		pcs[i] = 0;
}

// Check whether this cpu is holding the lock.
int holding(struct spinlock* lock){
	return lock->locked && lock->cpu == cpu;
}

// Pushcli/popcli are like cli/sti except that they are matched:
// it takes two popcli to undo two pushcli.  Also, if interrupts
// are off, then pushcli, popcli leaves them off.

void pushcli(void){
	int eflags;

	eflags = readeflags();
	cli();
	if (cpu->ncli++ == 0)
		cpu->intena = eflags & FL_IF;
}

void popcli(void){
	if (readeflags() & FL_IF)
		panic("popcli - interruptible");
	if (--cpu->ncli < 0)
		panic("popcli");
	if (cpu->ncli == 0 && cpu->intena)
		sti();
}
