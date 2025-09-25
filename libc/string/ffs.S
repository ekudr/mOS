
    .text
    .global ffs

ffsll:
    beqz a0, zero_in

    .word 0x60151593 # ctz a1, a0    Zbb extention
#    ctz a1, a0
    addi a0, a1, 1
    ret
zero_in:
    li a0, 0
    ret    

