#Dtek-board
.macro  PUSH reg
    addi sp, sp, -4
    sw   \reg, 0(sp) 
.endm

.macro  POP reg
    lw   \reg, 0(sp)
    addi sp, sp, 4
.endm
#Convenience shorthands for saving/restoring a register on the stack. Different assemblers.
#############################################################

    .data
    .align  2
mytime: .word   0x5957              # starts at 59:57
timstr: .asciz  "text more text lots of text\0"

         

    .text
    .globl tick, time2string, hexasc, delay, display_string
   # .global main handle_interrupt, timetemplate, main,



# main: program entry, just jump to timetemplate, do not return
#main:
 #   tail timetemplate

# handle_interrupt: stub , 
# handle_interrupt:
   # ret
#some board require an interrupt handler symbol even if it's a stub. Exporting symbols helps the linker.
#############################################################
# Optional RARS syscall print (won�t work on board itself)  #
#############################################################
display_string:
    li  a7, 4           # print string at address in a0
    ecall
    li  a0, 10          # newline
    li  a7, 11
    ecall
    ret  #printing with ecall won't work in hardware.

#############################################################
# Main loop                                                 #
#############################################################
timetemplate:
    la  a0, timstr
    jal display_string                  # prints timstr + newline

    # wait a little
    li  a0, 2                        # ms 2   #we need longer requested delay to achieve human-visible seconds
    jal delay

    # tick the packed-BCD time at mytime
    la  a0, mytime
    jal tick
    
   label2:
    la  a0, timstr
    la  t0, mytime
    lw  a1, 0(t0)
    jal time2string

    # loop forever
    j   timetemplate

#############################################################
# Tick: increment packed BCD time                           #
#############################################################
tick:
    lw  t0, 0(a0)        # get current time
    addi t0, t0, 1       # +1 second

    andi t1, t0, 0xF     # lowest digit
    sltiu t2, t1, 0xA    # if < 10, ok
    bnez t2, tiend
    addi t0, t0, 0x6     # adjust lowest digit

    andi t1, t0, 0xF0    # next digit
    sltiu t2, t1, 0x60   # if < 6, ok
    bnez t2, tiend
    addi t0, t0, 0xA0    # adjust digit

    li   t3, 0xF
    slli t3, t3, 8
    and  t1, t0, t3      # minute ones
    addi t3, x0, 0xA
    slli t3, t3, 8
    slt  t2, t1, t3      # if < 10, ok
    bnez t2, tiend
    addi t0, t0, 0x600   # adjust digit

    li   t3, 0xF
    slli t3, t3, 12
    and  t1, t0, t3      # minute tens
    addi t3, x0, 0x6
    slli t3, t3, 12
    slt  t2, t1, t3      # if < 6, ok
    bnez t2, tiend

    li   t3, 0xA
    slli t3, t3, 12
    add  t0, t0, t3      # adjust last digit

tiend:
    sw  t0, 0(a0)        # store updated time
    ret

#########################################################
# time2string                                           #
#########################################################
time2string:
    addi    sp, sp, -16
    sw      ra, 12(sp)
    sw      s0,  8(sp)

    mv      s0, a0      # s0 = out pointer
    mv      t1, a1      # t1 = time copy
    
   
    # Minutes tens (bits 12..15)
    mv      t2, t1
    srli    t2, t2, 12
    andi    a0, t2, 0xF
    jal     hexasc
    sb      a0, 0(s0)
    addi    s0, s0, 1

    # Minutes ones (bits 8..11)
    mv      t2, t1
    srli    t2, t2, 8
    andi    a0, t2, 0xF
    jal     hexasc
    sb      a0, 0(s0)
    addi    s0, s0, 1

    # Colon
    li      a0, 0x3A
    sb      a0, 0(s0)
    addi    s0, s0, 1

    # Seconds tens (bits 4..7)
    mv      t2, t1
    srli    t2, t2, 4
    andi    a0, t2, 0xF
    jal     hexasc
    sb      a0, 0(s0)
    addi    s0, s0, 1

    # Seconds ones (bits 0..3)
    andi    a0, t1, 0xF
    jal     hexasc
    sb      a0, 0(s0)
    addi    s0, s0, 1
    j      skip

  
 skip:
 
    li a0, 0
    sb a0,0(s0)
    

    lw      s0,  8(sp)
    lw      ra, 12(sp)
    addi    sp, sp, 16
    ret

#########################################################
# hexasc                                                #
#########################################################
hexasc:
    andi    a0, a0, 0xF      # keep low 4 bits
    li      t0, 10
    blt     a0, t0, label

    addi    a0, a0, -10      # 10..15 -> 0..5
    addi    a0, a0, 'A'      # 'A'..'F'
    ret

 label:
    addi    a0, a0, '0'      # '0'..'9'
    ret  #DTEk uses ret consistently

#########################################################
# delay(ms)                                             #
#########################################################
delay:
    mv   t0, a0              # t0 = ms

outer:
    beq t0, x0, done        # if ms == 0 -> done
    blt  t0, x0, done        # if ms < 0  -> done   blez function funkar inte i dtek
    addi t0, t0, -1

    li   t1, 1884400        # adjust this constant per board speed  4711

inner:
    addi t1, t1, -1
    bnez t1, inner

    j    outer

done:
    ret
