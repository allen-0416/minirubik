# store_sw.s -- write N_BYTES bytes with sw, one new address per store
# retired instructions = 3 * N_BYTES / 4 + 6   (N_BYTES a multiple of 4096)
# The region starts at BASE and is not declared in .data, so it holds
# no guest bytes until this loop writes them.

    .equ N_BYTES, 1048576
    .equ BASE,    0x10100000

    .text
    li   t0, BASE           # t0 = current address
    li   t1, N_BYTES
    add  t1, t0, t1         # t1 = end address
    li   t2, 1              # t2 = value to store
loop:
    sw   t2, 0(t0)          # store
    addi t0, t0, 4          # next address
    bltu t0, t1, loop

    li   a7, 10             # exit
    ecall
