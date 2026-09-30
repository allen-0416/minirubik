# check_no_m.s -- must FAIL to assemble: proves Ripes rejects M-extension
# instructions when no --isaexts is given (expected: "Unknown opcode 'mul'").

    .text
    li   t0, 6
    li   t1, 7
    mul  a0, t0, t1
    li   a7, 10
    ecall
