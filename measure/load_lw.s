# load_lw.s -- read N_BYTES bytes with lw, one new address per load
# retired instructions = 3 * N_BYTES / 4 + 5   (N_BYTES a multiple of 4096)
# The region starts at BASE and is not declared in .data, so it holds
# no guest bytes; this loop only reads it and never writes.

    .equ N_BYTES, 1048576
    .equ BASE,    0x10100000

    .text
    li   t0, BASE           # t0 = current address
    li   t1, N_BYTES
    add  t1, t0, t1         # t1 = end address
loop:
    lw   t2, 0(t0)          # load
    addi t0, t0, 4          # next address
    bltu t0, t1, loop

    li   a7, 10             # exit
    ecall
