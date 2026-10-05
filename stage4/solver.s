# solver.s -- optimal 2x2x2 solver, hand-written RV32I (Stage 4)
#
# Build and run (see Makefile):  make run STATE=21345671111111
#
# Register plan for the search (Stage 4 design):
#   s0 perm_move base    s1 ori_move base    s2 pdb_p base    s3 pdb_o base
#   s4 path base         s5 bound            s6 fr            s7 g
#   t0 face  t1 turn  t2 child np  t3 child no  t4 h_p / h  t5 h_o  t6 address
#   a0 root p, a1 root o: set in part 1, never changed during the search.
#   part 4: s8 len   s9 path cursor   s10 &path[len]   s11 names base
#           t2/t3 replayed p/o   t4/t5 row bases of the move's face
#
# Frame layout (12 bytes, word-aligned):
#   0 p   2 o   4 np   6 no   8 face   9 turn   10 last_face   11 pad
#
# Tables come from ../stage3/tables.s (generated, .rodata):
#   perm_move: .half [3][5040]   ori_move: .half [3][729]
#   pdb_p:     .byte [5040]      pdb_o:    .byte [729]
#
# Status:
#   part 1  parse + ranks          done, verified on 5 states
#   part 2  constants + root h     done, verified on 5 states
#   part 3  IDA* search            done, same paths as solver_ida.c
#   part 4  replay check + output  done; exit 0 verified, 1 replay failed
#
# Exit code: 0 = solution printed and verified by replay (T5),
#            1 = replay failed or no solution within 11 moves.

    .equ CUBIES, 7
    .equ FRAME, 12

    .section .rodata
# move m's name at names + 2*m; a one-letter name is padded with ' '.
names:
    .ascii "R R2R'B B2B'D D2D'"
# (6 - i)!: Lehmer weight of position i (exercise 1).
weight:
    .half 720, 120, 24, 6, 2, 1, 1

# face * (row bytes): perm_move rows are 5040 halfwords, ori_move rows 729.
perm_row:
    .word 0, 10080, 20160
ori_row:
    .word 0, 1458, 2916

    .bss
cubie:  .zero 8                 # p[0..6], 0-based, from input[0..6]
twist:  .zero 8                 # o[0..6], 0-based, from input[7..13]

    .balign 4
stack:  .zero 132               # 11 frames x 12 bytes (Q3: g <= 10)
path:   .zero 12                # moves 0..8, at most 11

    .text
    .globl _start
_start:
# ===========================================================================
# Part 1: parse the input and rank it.               a0 = perm rank, a1 = ori rank
#
# input (input.s) is a 14-character string, e.g. "21345671111111":
#   input[0..6]  cubie digits '1'..'7'  ->  cubie[i] = input[i] - '1'
#   input[7..13] twist digits '1'..'3'  ->  twist[i] = input[7 + i] - '1'
#
# Expected ranks (checked against solver.c):
#   12345671111111 ->    0   0       54721631111111 -> 3343   0
#   21345671111111 ->  720   0       76543213233233 -> 5039 644
#   76352141113232 -> 4988  23
# ===========================================================================

    # ---- parse: cubie[i] = input[i] - '1', twist[i] = input[7+i] - '1' ----
    la   t0, input              # t0 = 讀取指標，指向 input[0]
    la   t1, cubie              # t1 = 寫入指標，指向 cubie[0]
    li   t2, 7                  # t2 = 剩下幾個要處理（倒數計數）
parse_cubie:
    lbu  t3, 0(t0)              # t3 = 目前的字元
    addi t3, t3, -'1'           # 轉成 0-based
    sb   t3, 0(t1)              # 寫進 cubie[]
    addi t0, t0, 1              # 讀取指標往下一格
    addi t1, t1, 1              # 寫入指標往下一格
    addi t2, t2, -1             # 計數減 1
    bnez t2, parse_cubie        # 還沒做完就繼續

    # t0 現在指向 input[7]，不用重設
    la   t1, twist              # 寫入指標換成 twist[0]
    li   t2, 7
parse_twist:
    lbu  t3, 0(t0)              # t3 = 目前的字元
    addi t3, t3, -'1'           # 轉成 0-based
    sb   t3, 0(t1)              # 寫進 twist[]
    addi t0, t0, 1              # 讀取指標往下一格
    addi t1, t1, 1              # 寫入指標往下一格
    addi t2, t2, -1             # 計數減 1
    bnez t2, parse_twist        # 還沒做完就繼續

    # ---- a0 = permutation rank (exercise 1) ----
    # t0=&p[i] t1=&weight[i] t2=p[i] t3=weight[i] t4=&p[j] t5=p[j] t6=end
    li   a0, 0                  # rank = 0
    la   t0, cubie              # t0 = &cubie[i]，i = 0
    la   t1, weight             # t1 = &weight[i]
    addi t6, t0, 7              # t6 = &cubie[7]，結束位址
perm_i:
    lbu  t2, 0(t0)              # t2 = p[i]
    lhu  t3, 0(t1)              # t3 = weight[i]（.half 用 lhu）
    addi t4, t0, 1              # t4 = &cubie[i + 1]，內層起點

perm_j:
    bgeu t4, t6, perm_j_done    # j 走到結尾：內層結束
    lbu  t5, 0(t4)              # t5 = p[j]
    bgeu t5, t2, perm_skip      # p[j] >= p[i]：不是逆序對，跳過加法
    add  a0, a0, t3             # rank += weight[i]

perm_skip:
    addi t4, t4, 1              # j++
    j    perm_j

perm_j_done:
    addi t0, t0, 1              # i++
    addi t1, t1, 2              # weight 指標 +2
    bltu t0, t6, perm_i         # i < 7 就繼續外層

    # ---- a1 = orientation rank (exercise 1): base 3 over twist[0..5] ----
    li   a1, 0                  # rank = 0
    la   t0, twist              # t0 = &twist[0]
    addi t1, t0, 6              # t1 = &twist[6]，結束位址（只看前 6 個）

ori_i:
    lbu  t2, 0(t0)              # t2 = o[i]
    slli t3, a1, 1              # t3 = rank << 1
    add  a1, a1, t3             # rank = (rank << 1) + rank
    add  a1, a1, t2             # rank += o[i]
    addi t0, t0, 1              # i++
    bltu t0, t1, ori_i          # i < 6 就繼續

# ===========================================================================
# Part 2: search constants and the first bound.            s5 = h(root)
#
# Expected h (pdb_p, pdb_o):
#   21345671111111 -> 7 (7, 0)       54721631111111 -> 6 (6, 0)
#   76352141113232 -> 3 (3, 3)       76543213233233 -> 6 (6, 5)
# ===========================================================================
    la   s0, perm_move          # perm_move
    la   s1, ori_move           # ori_move
    la   s2, pdb_p              # pdb_p
    la   s3, pdb_o              # pdb_o
    la   s4, path               # path
    la   s8, perm_row           # perm_row
    la   s9, ori_row            # ori_row

    or   t0, a0, a1             # t0 = p | o
    beqz t0, solved_root        # 都是 0：解長度 0

    add  t0, s2, a0             # t0 = &pdb_p[p]（byte 表：基底 + p）
    lbu  t1, 0(t0)              # t1 = pdb_p[p]
    add  t0, s3, a1             # t0 = &pdb_o[o]
    lbu  t2, 0(t0)              # t2 = pdb_o[o]

    # s5 = max(t1, t2)：先假設 t1 最大，t1 < t2 才改成 t2
    addi s5, t1, 0
    bgeu t1, t2, h_done
    addi s5, t2, 0
h_done:

# ===========================================================================
# Part 3: IDA* search (exercise 5's my_search, with the frame layout above).
#
# Frame layout (12 bytes, word-aligned):
#   0 p   2 o   4 np   6 no   8 face   9 turn   10 last_face   11 pad
#
# Expected solutions (identical to ../stage3/solver_ida):
#   12345671111111 -> (empty line)
#   76352141113232 -> R2 B2 R'
#   21345671111111 -> R B' D2 R' B R' B' R D2 R B
#   54721631111111 -> B2 R' B R' D' B R2 B R B D'
# ===========================================================================

    # ---- bound_loop: my_solve's while (bound <= 11); root frame = stack[0] ----
bound_loop:
    li   t0, 11
    bgtu s5, t0, no_solution    # bound > 11：不可能（直徑是 11）
    la   s6, stack              # fr = &stack[0]
    li   s7, 0                  # g = 0
    sh   a0, 0(s6)              # p
    sh   a1, 2(s6)              # o
    sh   a0, 4(s6)              # np = p
    sh   a1, 6(s6)              # no = o
    sb   zero, 8(s6)            # face = 0
    sb   zero, 9(s6)            # turn = 0
    li   t0, 3
    sb   t0, 10(s6)             # last_face = 3（根節點沒有上一個面）

    # ---- search_loop: one small step per iteration ----
search_loop:
    lbu  t0, 8(s6)              # t0 = face
    li   t6, 3
    bne  t0, t6, not_popped     # face != 3：這一層還沒做完，繼續檢查
    # pop
    beqz s7, next_bound         # g == 0：這一輪 bound 失敗
    addi s6, s6, -12            # fr 回到上一層
    addi s7, s7, -1             # g - 1
    j    search_loop

not_popped:
    lbu  t1, 9(s6)              # t1 = turn
    beq  t1, t6, next_face      # turn == 3：這個面的 3 次都試完了
    lbu  t5, 10(s6)             # t5 = last_face
    beq  t0, t5, next_face      # face == last_face：同面，跳過

    # ---- child: np = perm_move[face][np] → t2 ----
    slli t4, t0, 2              # t4 = face * 4（.word 表的位元組偏移量）
    add  t6, s8, t4
    lw   t6, 0(t6)              # t6 = face * 10080
    add  t6, s0, t6             # t6 = &perm_move[face][0]
    lhu  t2, 4(s6)              # t2 = 目前的 np
    slli t4, t2, 1              # t4 = np * 2（.half）
    add  t6, t6, t4
    lhu  t2, 0(t6)              # t2 = 新的 np
    sh   t2, 4(s6)              # 存回 frame

    # ---- child: no = ori_move[face][no] → t3 ----
    slli t4, t0, 2              # t4 = face * 4（.word 表的位元組偏移量）
    add  t6, s9, t4
    lw   t6, 0(t6)              # t6 = face * 1458
    add  t6, s1, t6             # t6 = &ori_move[face][0]
    lhu  t3, 6(s6)              # t3 = 目前的 no
    slli t4, t3, 1              # t4 = no * 2（.half）
    add  t6, t6, t4
    lhu  t3, 0(t6)              # t3 = 新的 no
    sh   t3, 6(s6)              # 存回 frame

    # ---- path[g] = face * 3 + turn ----
    slli t4, t0, 1              # face << 1
    add  t4, t4, t0             # + face
    add  t4, t4, t1             # + turn
    add  t6, s4, s7             # t6 = &path[g]
    sb   t4, 0(t6)

    # ---- ++turn ----
    addi t1, t1, 1
    sb   t1, 9(s6)

    # ---- solved? ----
    or   t4, t2, t3
    beqz t4, found

    # ---- h = max(pdb_p[np], pdb_o[no]) → t4 ----
    add  t6, s2, t2
    lbu  t4, 0(t6)              # t4 = pdb_p[np]
    add  t6, s3, t3
    lbu  t5, 0(t6)              # t5 = pdb_o[no]
    bgeu t4, t5, child_h_done
    addi t4, t5, 0
child_h_done:

    # ---- prune: g + 1 + h > bound → search_loop ----
    add  t4, t4, s7             # h + g
    addi t4, t4, 1              # h + g + 1
    bgtu t4, s5, search_loop

    # ---- push: next frame starts at 12(s6) ----
    sh   t2, 12(s6)             # next.p  = np
    sh   t3, 14(s6)             # next.o  = no
    sh   t2, 16(s6)             # next.np = np
    sh   t3, 18(s6)             # next.no = no
    sb   zero, 20(s6)           # next.face = 0
    sb   zero, 21(s6)           # next.turn = 0
    sb   t0, 22(s6)             # next.last_face = 這一層的 face
    addi s6, s6, 12
    addi s7, s7, 1
    j    search_loop

    # ---- next_face: face + 1, turn = 0, (np, no) = (p, o) ----
next_face:
    addi t0, t0, 1              # face + 1
    sb   t0, 8(s6)
    sb   zero, 9(s6)            # turn = 0
    lhu  t2, 0(s6)              # t2 = p
    sh   t2, 4(s6)              # np = p
    lhu  t3, 2(s6)              # t3 = o
    sh   t3, 6(s6)              # no = o
    j    search_loop

next_bound:
    addi s5, s5, 1              # bound + 1
    j    bound_loop

# ===========================================================================
# Part 4: replay the path from the root (T5), then print it.
# ===========================================================================
found:
    addi s8, s7, 1              # len = g + 1
    # ---- replay: apply path[0..len-1] from the root; must reach (0, 0) ----
    mv   t2, a0                  # p = 根節點的 p
    mv   t3, a1                  # o = 根節點的 o
    mv   s9, s4                  # s9 = &path[0]
    add  s10, s9, s8              # s10 = &path[len]

replay_move:
    bgeu s9, s10, replay_check  # 全部走完
    lbu  t0, 0(s9)              # t0 = m
    mv   t4, s0                 # 先假設 R 面
    mv   t5, s1
    li   t6, 3
    bltu t0, t6, replay_turns   # m < 3：就是 R 面
    # 往下一面（B）：m -= 3，列起點各加一列
    addi t0, t0, -3
    li   t1, 10080
    add  t4, t4, t1
    li   t1, 1458
    add  t5, t5, t1
    bltu t0, t6, replay_turns   # m < 3：就是 B 面
    # 再往下一面（D）：往下直接進入 replay_turns
    addi t0, t0, -3
    li   t1, 10080
    add  t4, t4, t1
    li   t1, 1458
    add  t5, t5, t1

replay_turns:
    addi t0, t0, 1              # 要轉 m + 1 次

replay_turn:
    slli t6, t2, 1              # p * 2
    add  t6, t4, t6              # &perm_move[face][p]（這一面的列起點 + p*2）
    lhu  t2, 0(t6)              # p = 轉一次後的 p
    slli t6, t3, 1              # o * 2
    add  t6, t5, t6              # &ori_move[face][o]
    lhu  t3, 0(t6)              # o = 轉一次後的 o
    addi t0, t0, -1
    bnez t0, replay_turn                 # 還沒轉完就繼續
    addi s9, s9, 1              # 下一步
    j    replay_move

replay_check:
    or   t6, t2, t3
    bnez t6, replay_fail        # 沒回到 (0, 0)：驗證失敗
    # ---- print the solution: "R B' D2 ...\n" ----
    la   s11, names
    mv   s9, s4                  # s9 = &path[0]（重新從頭走）

print_move:
    bgeu s9, s10, print_end      # s10 = &path[len]，前面已經算好
    beq  s9, s4, print_name      # 第一步：前面不印空白
    li   a0, ' '
    li   a7, 11
    ecall

print_name:
    lbu  t0, 0(s9)               # m
    slli t0, t0, 1               # m * 2
    add  t0, s11, t0               # &names[2m]
    lbu  a0, 0(t0)               # 第一個字元
    li   a7, 11
    ecall
    lbu  a0, 1(t0)               # 第二個字元
    li   t1, ' '
    beq  a0, t1, print_next       # 是空白就不印
    li   a7, 11
    ecall

print_next:
    addi s9, s9, 1
    j    print_move

print_end:
solved_root:                    # 根節點已解：解長度 0，只印換行
    li   a0, '\n'
    li   a7, 11
    ecall
    li   a0, 0                   # exit 0：已驗證
    li   a7, 93
    ecall

replay_fail:
no_solution:
    li   a0, 1                  # exit code 1：驗證失敗或找不到解
    li   a7, 93
    ecall
