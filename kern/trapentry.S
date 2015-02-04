/* See COPYRIGHT for copyright information. */

#include <inc/mmu.h>
#include <inc/memlayout.h>
#include <inc/trap.h>
#include <kern/macro.h>

#include <kern/picirq.h>


###################################################################
# exceptions/interrupts
###################################################################

/* TRAPHANDLER defines a globally-visible function for handling a trap.
 * It pushes a trap number onto the stack, then jumps to _alltraps.
 * Use TRAPHANDLER for traps where the CPU automatically pushes an error code.
 *
 * You shouldn't call a TRAPHANDLER function from C, but you may
 * need to _declare_ one in C (for instance, to get a function pointer
 * during IDT setup).  You can declare the function with
 *   void NAME();
 * where NAME is the argument passed to TRAPHANDLER.
 */
#define TRAPHANDLER(name, num)						\
	.globl name;		/* define global symbol for 'name' */	\
	.type name, @function;	/* symbol type is function */		\
	.align 2;		/* align function definition */		\
	name:			/* function starts here */		\
	pushq $(num);							\
	jmp _alltraps

/* Use TRAPHANDLER_NOEC for traps where the CPU doesn't push an error code.
 * It pushes a 0 in place of the error code, so the trap frame has the same
 * format in either case.
 */
#define TRAPHANDLER_NOEC(name, num)					\
	.globl name;							\
	.type name, @function;						\
	.align 2;							\
	name:								\
	pushq $0;							\
	pushq $(num);							\
	jmp _alltraps

.text

/*
 * Lab 3: Your code here for generating entry points for the different traps.
   Find the PUSHA,POPA macros in kern/macro.h. Since amd64 doesn't support
   pusha,popa so you will find these macros handy.
 */

TRAPHANDLER_NOEC(TRAPH_divide, T_DIVIDE)
TRAPHANDLER_NOEC(TRAPH_debug, T_DEBUG)
TRAPHANDLER_NOEC(TRAPH_nmi, T_NMI)
TRAPHANDLER_NOEC(TRAPH_breakpoint, T_BRKPT)
TRAPHANDLER_NOEC(TRAPH_overflow, T_OFLOW)
TRAPHANDLER_NOEC(TRAPH_boundcheck, T_BOUND)
TRAPHANDLER_NOEC(TRAPH_illegalop, T_ILLOP)
TRAPHANDLER_NOEC(TRAPH_devicena, T_DEVICE)
TRAPHANDLER(TRAPH_doublefault, T_DBLFLT)
TRAPHANDLER(TRAPH_invalidtss, T_TSS)
TRAPHANDLER(TRAPH_segmentnotpresent, T_SEGNP)
TRAPHANDLER(TRAPH_stackexception, T_STACK)
TRAPHANDLER(TRAPH_generalprotectionfault, T_GPFLT)
TRAPHANDLER(TRAPH_pagefault, T_PGFLT)
TRAPHANDLER_NOEC(TRAPH_syscall, T_SYSCALL)
TRAPHANDLER_NOEC(TRAPH_floatingpointerror, T_FPERR)
TRAPHANDLER_NOEC(TRAPH_alignmentcheck, T_ALIGN)
TRAPHANDLER_NOEC(TRAPH_machinecheck, T_MCHK)
TRAPHANDLER_NOEC(TRAPH_simderror, T_SIMDERR)
TRAPHANDLER_NOEC(TRAPH_timer, IRQ_OFFSET+IRQ_TIMER)
TRAPHANDLER_NOEC(TRAPH_keyboard, IRQ_OFFSET+IRQ_KBD)
TRAPHANDLER_NOEC(TRAPH_serial, IRQ_OFFSET+IRQ_SERIAL)
TRAPHANDLER_NOEC(TRAPH_spurious,IRQ_OFFSET+IRQ_SPURIOUS)
TRAPHANDLER_NOEC(TRAPH_ide,IRQ_OFFSET+IRQ_IDE)
TRAPHANDLER_NOEC(TRAPH_err,IRQ_OFFSET+IRQ_ERROR)


/*
 * Lab 3: Your code here for _alltraps
 *
 * Hint: Be sure to review the x64 calling convention from lab1
 *       for how to pass a pointer to the trapframe.
 */

_alltraps:
     
	subq $8,%rsp
	movw %ds, (%rsp)
	subq $8,%rsp
	movw %es, (%rsp)
	PUSHA
	movq $(GD_KD), %rax
	movw %ax, %ds
	movw %ax, %es
	movq %rsp, %rdi
	call trap
	POPA_
	movw (%rsp), %es
	add $8, %rsp
	movw (%rsp), %ds
	add $8, %rsp
	iret
