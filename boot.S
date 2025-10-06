    .data
    .align 2
welcome_msg:
    .asciz "================================================\n===== RISC-V Boot-Up Process Now Complete ======\n================================================\n"

    .section .text
    .align 2
    .globl _start
    .globl enable_interrupt

_isr_handler:
    j _isr_routine
    j _start

_isr_routine:
    addi sp, sp, -4*32

    sw x1,  0(sp)
    sw x3,  8(sp)
    sw x4, 12(sp)
    sw x5, 16(sp)
    sw x6, 20(sp)
    sw x7, 24(sp)
    sw x8, 28(sp)
    sw x9, 32(sp)
    sw x10,36(sp)
    sw x11,40(sp)
    sw x12,44(sp)
    sw x13,48(sp)
    sw x14,52(sp)
    sw x15,56(sp)
    sw x16,60(sp)
    sw x17,64(sp)
    sw x18,68(sp)
    sw x19,72(sp)
    sw x20,76(sp)
    sw x21,80(sp)
    sw x22,84(sp)
    sw x23,88(sp)
    sw x24,92(sp)
    sw x25,96(sp)
    sw x26,100(sp)
    sw x27,104(sp)
    sw x28,108(sp)
    sw x29,112(sp)
    sw x30,116(sp)
    sw x31,120(sp)

    csrr t0, mcause
    li   t1, 0x80000000
    bgeu t0, t1, external_irq   # treat mcause >= 0x80000000 as interrupt

    add  a6, t0, zero
    li   t1, 11
    beq  t0, t1, skip_init_args
    csrr a0, mepc
skip_init_args:
    jal  handle_exception

    csrr t0, mepc
    addi t0, t0, 4
    csrw mepc, t0
    j restore

external_irq:
    li   t0, 0x7fffffff
    csrr t1, mcause
    and  a0, t1, t0
    jal  handle_interrupt

restore:
    lw x1,  0(sp)
    lw x3,  8(sp)
    lw x4, 12(sp)
    lw x5, 16(sp)
    lw x6, 20(sp)
    lw x7, 24(sp)
    lw x8, 28(sp)
    lw x9, 32(sp)
    lw x10,36(sp)
    lw x11,40(sp)
    lw x12,44(sp)
    lw x13,48(sp)
    lw x14,52(sp)
    lw x15,56(sp)
    lw x16,60(sp)
    lw x17,64(sp)
    lw x18,68(sp)
    lw x19,72(sp)
    lw x20,76(sp)
    lw x21,80(sp)
    lw x22,84(sp)
    lw x23,88(sp)
    lw x24,92(sp)
    lw x25,96(sp)
    lw x26,100(sp)
    lw x27,104(sp)
    lw x28,108(sp)
    lw x29,112(sp)
    lw x30,116(sp)
    lw x31,120(sp)

    addi sp, sp, 4*32
    mret

_start:
    csrw mie, x0

    # >>> important: set trap vector <<<
    la   t0, _isr_handler
    csrw mtvec, t0
    # <<<

    la   sp, _stack_end
    la   gp, __global_pointer
    la   a0, welcome_msg
    li   a7, 4
    ecall
    jal  main

loop:
    j loop

enable_interrupt:
    
    
    csrsi mie, 16
	csrsi mstatus,3 
	
    ret
