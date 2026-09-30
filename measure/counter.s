# counter.s -- count down from ITERS to 0 (no memory access)
# retired instructions = 2 * ITERS + 4   (for ITERS = 1000000)

    .equ ITERS, 1000000

    .text
    li   t0, ITERS          # t0 = counter
loop:
    addi t0, t0, -1         # counter--
    bnez t0, loop           # repeat until counter == 0

    li   a7, 10             # exit
    ecall
