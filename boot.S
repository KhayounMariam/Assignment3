/* boot.S — RV32IM startup + interrupt glue for DTEK-V labs
 *
 * Provides:
 *   - _start:   C runtime bring-up, sets mtvec, calls main, loops
 *   - enable_interrupt(): set MIE.MEIE and mstatus.MIE
 *   - trap_entry: machine trap handler → calls handle_interrupt(cause)
 *
 * Expects linker symbols:
 *   _stack_top, __bss_start, __bss_end, __global_pointer$
 */

    .option norvc
    .section .text.init
    .globl _start
_start:
    /* Set up stack */
    la      sp, _stack_top

    /* Set global pointer (optional, but nice for -fpic/-msmall-data) */
    .option push
    .option norelax
    la      gp, __global_pointer$
    .option pop

    /* Zero .bss */
    la      t0, __bss_start
    la      t1, __bss_end
1:
    beq     t0, t1, 2f
    sw      x0, 0(t0)
    addi    t0, t0, 4
    blt     t0, t1, 1b
2:

    /* Install machine trap-vector */
    la      t0, trap_entry
    csrw    mtvec, t0

    /* Jump to C main() */
    call    main

/* If main returns, park the core */
3:  wfi
    j       3b


/* --------------------------------------------------------------- */
/* Machine trap handler: save context → call handle_interrupt → mret
 * We save all integer regs (except x0) to be ABI-safe for C.
 */
    .align  4
    .globl  trap_entry
trap_entry:
    /* Make room for 31 regs (x1..x31) = 31*4 = 124 bytes */
    addi    sp, sp, -(31*4)

    /* Save x1..x31 (skip x0) in order, at 0..120(sp) */
    sw      ra,   0(sp)     /* x1  */
    sw      gp,   4(sp)     /* x3  */
    sw      tp,   8(sp)     /* x4  */
    sw      t0,  12(sp)     /* x5  */
    sw      t1,  16(sp)     /* x6  */
    sw      t2,  20(sp)     /* x7  */
    sw      s0,  24(sp)     /* x8  */
    sw      s1,  28(sp)     /* x9  */
    sw      a0,  32(sp)     /* x10 */
    sw      a1,  36(sp)     /* x11 */
    sw      a2,  40(sp)     /* x12 */
    sw      a3,  44(sp)     /* x13 */
    sw      a4,  48(sp)     /* x14 */
    sw      a5,  52(sp)     /* x15 */
    sw      a6,  56(sp)     /* x16 */
    sw      a7,  60(sp)     /* x17 */
    sw      s2,  64(sp)     /* x18 */
    sw      s3,  68(sp)     /* x19 */
    sw      s4,  72(sp)     /* x20 */
    sw      s5,  76(sp)     /* x21 */
    sw      s6,  80(sp)     /* x22 */
    sw      s7,  84(sp)     /* x23 */
    sw      s8,  88(sp)     /* x24 */
    sw      s9,  92(sp)     /* x25 */
    sw      s10, 96(sp)     /* x26 */
    sw      s11, 100(sp)    /* x27 */
    sw      t3,  104(sp)    /* x28 */
    sw      t4,  108(sp)    /* x29 */
    sw      t5,  112(sp)    /* x30 */
    sw      t6,  116(sp)    /* x31 */

    /* Read mcause and pass a "cause" to C as a0.
       Your C currently ignores it, but we pass it anyway. */
    csrr    t0, mcause
    mv      a0, t0

    /* Call C ISR glue: void handle_interrupt(unsigned cause); */
    call    handle_interrupt

    /* Restore registers (reverse order not required) */
    lw      ra,   0(sp)
    lw      gp,   4(sp)
    lw      tp,   8(sp)
    lw      t0,  12(sp)
    lw      t1,  16(sp)
    lw      t2,  20(sp)
    lw      s0,  24(sp)
    lw      s1,  28(sp)
    lw      a0,  32(sp)
    lw      a1,  36(sp)
    lw      a2,  40(sp)
    lw      a3,  44(sp)
    lw      a4,  48(sp)
    lw      a5,  52(sp)
    lw      a6,  56(sp)
    lw      a7,  60(sp)
    lw      s2,  64(sp)
    lw      s3,  68(sp)
    lw      s4,  72(sp)
    lw      s5,  76(sp)
    lw      s6,  80(sp)
    lw      s7,  84(sp)
    lw      s8,  88(sp)
    lw      s9,  92(sp)
    lw      s10, 96(sp)
    lw      s11, 100(sp)
    lw      t3,  104(sp)
    lw      t4,  108(sp)
    lw      t5,  112(sp)
    lw      t6,  116(sp)

    addi    sp, sp, (31*4)

    /* Return from trap/interrupt */
    mret


/* --------------------------------------------------------------- */
/* void enable_interrupt(void)
 * - enable machine external interrupts (MIE.MEIE)
 * - set global MIE (mstatus.MIE)
 */
    .section .text
    .globl  enable_interrupt
enable_interrupt:
    /* Enable Machine External Interrupts (MEIE) */
    csrr    t0, mie
    li      t1, (1 << 11)          /* MEIE bit */
    or      t0, t0, t1
    csrw    mie, t0

    /* Enable global machine interrupts (MIE in mstatus) */
    csrr    t0, mstatus
    li      t1, (1 << 3)           /* MIE bit */
    or      t0, t0, t1
    csrw    mstatus, t0
    ret


/* --------------------------------------------------------------- */
/* Weak fallback if handle_interrupt is missing (safety).
 * Your project defines it in C; this is just to avoid link errors. */
    .weak   handle_interrupt
handle_interrupt:
    ret
