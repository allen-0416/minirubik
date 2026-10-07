# solver.s -- optimal 2x2x2 solver in hand-written RV32I
#
#   make run STATE=21345671111111      CLI build, measured with --iret
#   make gui STATE=76352141113232      GUI build with the LED renderer
#
# Input: the 14-character string `input` (input.s). Output: the solution,
# e.g. "R B' D2 ...". Exit code 0 = solution verified by replay, 1 = not.
# Tables come from ../stage3/tables.s: perm_move .half [3][5040],
# ori_move .half [3][729], pdb_p .byte [5040], pdb_o .byte [729].
#
# Registers in the search:
#   s0 perm_move  s1 ori_move  s2 pdb_p  s3 pdb_o  s4 path  s5 bound
#   s6 fr (frame pointer)  s7 g  s8 perm_row  s9 ori_row
#   a0, a1 root permutation and orientation ranks (never changed)
#   t0 face  t1 turn  t2 child np  t3 child no  t4 h  t5 h_o  t6 address
# After the search (replay and print): s8 len, s9 path cursor,
#   s10 &path[len], s11 names.
#
# Frame (12 bytes, word-aligned):
#   0 p   2 o   4 np   6 no   8 face   9 turn   10 last_face   11 pad

    .equ CUBIES, 7
    .equ FRAME, 12

# RENDER = 1 adds the LED renderer (make gui); RENDER = 0, the default,
# leaves it out entirely (make run), so --iret and .text are unaffected.
    .ifndef RENDER
    .equ RENDER, 0
    .endif

    .section .rodata
names:                          # move m's name at names + 2m, ' '-padded
    .ascii "R R2R'B B2B'D D2D'"
weight:                         # (6 - i)!: Lehmer weight of position i
    .half 720, 120, 24, 6, 2, 1, 1
perm_row:                       # face * 5040 * 2: perm_move row offsets
    .word 0, 10080, 20160
ori_row:                        # face * 729 * 2: ori_move row offsets
    .word 0, 1458, 2916

    .bss
cubie:  .zero 8                 # cubie at positions 1..7, 0-based
twist:  .zero 8                 # twist at positions 1..7, 0..2

    .balign 4
stack:  .zero 132               # 11 frames: a pushed node has g <= 10
path:   .zero 12                # moves 0..8

    .text
    .globl _start
_start:
# ---------------------------------------------------------------------------
# Part 1: parse the input, then rank it: a0 = permutation, a1 = orientation.
# ---------------------------------------------------------------------------
    la   t0, input              # read pointer
    la   t1, cubie              # write pointer
    li   t2, 7                  # count down
parse_cubie:
    lbu  t3, 0(t0)
    addi t3, t3, -'1'           # '1'..'7' -> 0..6
    sb   t3, 0(t1)
    addi t0, t0, 1
    addi t1, t1, 1
    addi t2, t2, -1
    bnez t2, parse_cubie

    la   t1, twist              # t0 already points at input[7]
    li   t2, 7
parse_twist:
    lbu  t3, 0(t0)
    addi t3, t3, -'1'           # '1'..'3' -> 0..2
    sb   t3, 0(t1)
    addi t0, t0, 1
    addi t1, t1, 1
    addi t2, t2, -1
    bnez t2, parse_twist

    .if RENDER
    jal  ra, render_state       # draw the input state (before a0/a1 are set)
    .endif

    # Permutation rank: every inversion p[j] < p[i], j > i, adds weight[i].
    # t0 &p[i]  t1 &weight[i]  t2 p[i]  t3 weight[i]  t4 &p[j]  t5 p[j]
    li   a0, 0
    la   t0, cubie
    la   t1, weight
    addi t6, t0, 7              # end
perm_i:
    lbu  t2, 0(t0)
    lhu  t3, 0(t1)
    addi t4, t0, 1              # j = i + 1
perm_j:
    bgeu t4, t6, perm_j_done
    lbu  t5, 0(t4)
    bgeu t5, t2, perm_skip      # not an inversion
    add  a0, a0, t3
perm_skip:
    addi t4, t4, 1
    j    perm_j
perm_j_done:
    addi t0, t0, 1
    addi t1, t1, 2              # .half
    bltu t0, t6, perm_i

    # Orientation rank: base 3 over twist[0..5]; rank * 3 = (rank << 1) + rank.
    li   a1, 0
    la   t0, twist
    addi t1, t0, 6              # twist[6] follows from the others
ori_i:
    lbu  t2, 0(t0)
    slli t3, a1, 1
    add  a1, a1, t3
    add  a1, a1, t2
    addi t0, t0, 1
    bltu t0, t1, ori_i

# ---------------------------------------------------------------------------
# Part 2: search constants and the first bound s5 = h(root).
# ---------------------------------------------------------------------------
    la   s0, perm_move
    la   s1, ori_move
    la   s2, pdb_p
    la   s3, pdb_o
    la   s4, path
    la   s8, perm_row
    la   s9, ori_row

    or   t0, a0, a1
    beqz t0, solved_root        # already solved: empty solution

    add  t0, s2, a0
    lbu  t1, 0(t0)              # pdb_p[p]
    add  t0, s3, a1
    lbu  t2, 0(t0)              # pdb_o[o]
    addi s5, t1, 0              # s5 = max(t1, t2)
    bgeu t1, t2, h_done
    addi s5, t2, 0
h_done:

# ---------------------------------------------------------------------------
# Part 3: IDA* with an explicit stack (stage3/solver_ida.c, my_search).
# ---------------------------------------------------------------------------
bound_loop:
    li   t0, 11
    bgtu s5, t0, no_solution    # the diameter is 11
    la   s6, stack              # root frame
    li   s7, 0
    sh   a0, 0(s6)              # p
    sh   a1, 2(s6)              # o
    sh   a0, 4(s6)              # np = p
    sh   a1, 6(s6)              # no = o
    sb   zero, 8(s6)            # face
    sb   zero, 9(s6)            # turn
    li   t0, 3
    sb   t0, 10(s6)             # last_face = 3: none at the root

search_loop:
    lbu  t0, 8(s6)              # face
    li   t6, 3
    bne  t0, t6, not_popped
    beqz s7, next_bound         # root exhausted: this bound fails
    addi s6, s6, -12            # pop
    addi s7, s7, -1
    j    search_loop

not_popped:
    lbu  t1, 9(s6)              # turn
    beq  t1, t6, next_face      # three turns tried
    lbu  t5, 10(s6)
    beq  t0, t5, next_face      # same face as the last move: skip

    # Child: one more quarter turn of face on (np, no).
    slli t4, t0, 2
    add  t6, s8, t4
    lw   t6, 0(t6)              # face * 10080
    add  t6, s0, t6             # &perm_move[face][0]
    lhu  t2, 4(s6)
    slli t4, t2, 1
    add  t6, t6, t4
    lhu  t2, 0(t6)              # np = perm_move[face][np]
    sh   t2, 4(s6)

    slli t4, t0, 2
    add  t6, s9, t4
    lw   t6, 0(t6)              # face * 1458
    add  t6, s1, t6             # &ori_move[face][0]
    lhu  t3, 6(s6)
    slli t4, t3, 1
    add  t6, t6, t4
    lhu  t3, 0(t6)              # no = ori_move[face][no]
    sh   t3, 6(s6)

    slli t4, t0, 1              # path[g] = face * 3 + turn
    add  t4, t4, t0
    add  t4, t4, t1
    add  t6, s4, s7
    sb   t4, 0(t6)

    addi t1, t1, 1              # ++turn
    sb   t1, 9(s6)

    or   t4, t2, t3
    beqz t4, found              # child solved

    add  t6, s2, t2             # h = max(pdb_p[np], pdb_o[no])
    lbu  t4, 0(t6)
    add  t6, s3, t3
    lbu  t5, 0(t6)
    bgeu t4, t5, child_h_done
    addi t4, t5, 0
child_h_done:
    add  t4, t4, s7
    addi t4, t4, 1
    bgtu t4, s5, search_loop    # prune: g + 1 + h > bound

    sh   t2, 12(s6)             # push: next frame at 12(s6)
    sh   t3, 14(s6)
    sh   t2, 16(s6)
    sh   t3, 18(s6)
    sb   zero, 20(s6)
    sb   zero, 21(s6)
    sb   t0, 22(s6)             # next.last_face = face
    addi s6, s6, 12
    addi s7, s7, 1
    j    search_loop

next_face:
    addi t0, t0, 1
    sb   t0, 8(s6)
    sb   zero, 9(s6)
    lhu  t2, 0(s6)              # (np, no) = (p, o)
    sh   t2, 4(s6)
    lhu  t3, 2(s6)
    sh   t3, 6(s6)
    j    search_loop

next_bound:
    addi s5, s5, 1
    j    bound_loop

# ---------------------------------------------------------------------------
# Part 4: replay the path from the root (T5), then print it.
# ---------------------------------------------------------------------------
found:
    addi s8, s7, 1              # len
    mv   t2, a0
    mv   t3, a1
    mv   s9, s4
    add  s10, s9, s8            # &path[len]

replay_move:
    bgeu s9, s10, replay_check
    lbu  t0, 0(s9)              # m = face * 3 + turn: find the face
    mv   t4, s0                 # row bases, R first
    mv   t5, s1
    li   t6, 3
    bltu t0, t6, replay_turns
    addi t0, t0, -3             # B
    li   t1, 10080
    add  t4, t4, t1
    li   t1, 1458
    add  t5, t5, t1
    bltu t0, t6, replay_turns
    addi t0, t0, -3             # D
    li   t1, 10080
    add  t4, t4, t1
    li   t1, 1458
    add  t5, t5, t1
replay_turns:
    addi t0, t0, 1              # turn + 1 quarter turns
replay_turn:
    slli t6, t2, 1
    add  t6, t4, t6
    lhu  t2, 0(t6)
    slli t6, t3, 1
    add  t6, t5, t6
    lhu  t3, 0(t6)
    addi t0, t0, -1
    bnez t0, replay_turn
    addi s9, s9, 1
    j    replay_move

replay_check:
    or   t6, t2, t3
    bnez t6, replay_fail        # not back at (0, 0)
    la   s11, names
    mv   s9, s4

print_move:
    bgeu s9, s10, print_end
    beq  s9, s4, print_name     # no space before the first move
    li   a0, ' '
    li   a7, 11                 # print character
    ecall
print_name:
    lbu  t0, 0(s9)
    slli t0, t0, 1
    add  t0, s11, t0            # &names[2m]
    lbu  a0, 0(t0)
    li   a7, 11
    ecall
    lbu  a0, 1(t0)
    li   t1, ' '
    beq  a0, t1, print_next     # one-letter name
    li   a7, 11
    ecall
print_next:
    .if RENDER
    lbu  a0, 0(s9)              # redraw after every move
    jal  ra, apply_move_state
    jal  ra, render_state
    .endif
    addi s9, s9, 1
    j    print_move

print_end:
solved_root:
    li   a0, '\n'
    li   a7, 11
    ecall
    li   a0, 0                  # exit 0: verified
    li   a7, 93
    ecall

replay_fail:
no_solution:
    li   a0, 1                  # exit 1
    li   a7, 93
    ecall

    .if RENDER
    .include "render.s"
    .endif
