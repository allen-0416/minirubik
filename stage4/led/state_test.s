# state_test.s -- LED stage 3: render any cube state from cubie[] / twist[]
#
# Build: make (or make ripes for the GUI editor). Run with an LED Matrix of
# Width 35, Height 25.
#
# Positions 0..7 follow README.md: 0 FUL (fixed anchor), 1 FUR, 2 FDR, 3 FDL,
# 4 BUR, 5 BDR, 6 BDL, 7 BUL. cubie[k] / twist[k] (k = 0..6) describe position
# k + 1, 0-based, exactly as solver.s parses them; cubie value j is the cubie
# whose home is position j + 1.
#
# Slots: the 3 stickers of a position, slot 0 on the U or D face, then
# counterclockwise seen from outside the corner. A cubie's sticker 0 is its
# U/D colour. twist = t means sticker 0 sits in slot t, so
#     slot k shows sticker s = k - t, plus 3 if negative   (no % 3)
# and sticker s of cubie c has the colour of slot s at position c.
#
# Test state: one R turn from solved (cubie = 1420356,
# twist = 1202100). Expected net, W white, O orange,
# G green, R red, b blue, Y yellow:
#
#   col 0 1 2 3 4 5 6 7
#   . . W G . . . .
#   . . W G . . . .
#   O O G Y R R W b
#   O O G Y R R W b
#   . . Y b . . . .
#   . . Y b . . . .
#
# Status: TODO the render loop in _start.

    .equ LED_MATRIX_0_BASE, 0xf0000000
    .equ LED_ROW_BYTES, 140

    .section .rodata
    .balign 4
col_off:
    .word 0, 16, 36, 52, 72, 88, 108, 124
row_off:
    .word 0, 420, 980, 1400, 1960, 2380
# home_color[c][s]: colour of slot s at position c = colour of cubie c's
# sticker s. 3 words per position (12 bytes).
home_color:
    .word 0x00FFFFFF, 0x00FF8000, 0x0000C000   # 0 FUL: U L F
    .word 0x00FFFFFF, 0x0000C000, 0x00FF0000   # 1 FUR: U F R
    .word 0x00FFFF00, 0x00FF0000, 0x0000C000   # 2 FDR: D R F
    .word 0x00FFFF00, 0x0000C000, 0x00FF8000   # 3 FDL: D F L
    .word 0x00FFFFFF, 0x00FF0000, 0x000000FF   # 4 BUR: U R B
    .word 0x00FFFF00, 0x000000FF, 0x00FF0000   # 5 BDR: D B R
    .word 0x00FFFF00, 0x00FF8000, 0x000000FF   # 6 BDL: D L B
    .word 0x00FFFFFF, 0x000000FF, 0x00FF8000   # 7 BUL: U B L
# slot_cell[i][k]: (col, row) of slot k at position i. 3 byte pairs per
# position (6 bytes).
slot_cell:
    .byte 2, 1,  1, 2,  2, 2   # 0 FUL: U L F
    .byte 3, 1,  3, 2,  4, 2   # 1 FUR: U F R
    .byte 3, 4,  4, 3,  3, 3   # 2 FDR: D R F
    .byte 2, 4,  2, 3,  1, 3   # 3 FDL: D F L
    .byte 3, 0,  5, 2,  6, 2   # 4 BUR: U R B
    .byte 3, 5,  6, 3,  5, 3   # 5 BDR: D B R
    .byte 2, 5,  0, 3,  7, 3   # 6 BDL: D L B
    .byte 2, 0,  7, 2,  0, 2   # 7 BUL: U B L

    .data
# Test state: one R from solved. Same layout as solver.s's cubie / twist.
cubie:  .byte 1, 4, 2, 0, 3, 5, 6
twist:  .byte 1, 2, 0, 2, 1, 0, 0

    .text
    .globl _start

    # TODO: render all 8 positions.
    # Keep loop variables in s registers (draw_facelet overwrites t0..t6):
    #   s0 = position i (0..7)
    #   s1 = c (cubie at i, as a position number 0..7), s2 = t (its twist)
    #   s3 = &slot_cell[i][0] (+6 per position)
    #   s4 = slot k (0..2)
    # For each i:
    #   i == 0: c = 0, t = 0 (the anchor never moves)
    #   else:   c = cubie[i - 1] + 1, t = twist[i - 1]
    # For each k: s = k - t (+3 if negative); colour = home_color[c][s]
    #   (address home_color + c * 12 + s * 4: c * 12 = (c << 3) + (c << 2));
    #   a0 = slot_cell[i][k].col, a1 = .row, a2 = colour; jal ra, draw_facelet
_start:
    li   s0, 0                      # i = 0
    la   s3, slot_cell              # &slot_cell[0][0]

pos_loop:
    # ---- c, t ----
    li   s1, 0                      # 先假設是錨點：c = 0
    li   s2, 0                      #              t = 0
    beqz s0, have_ct                # i == 0：就是錨點
    la   t0, cubie
    add  t0, t0, s0
    lbu  s1, -1(t0)                  # cubie[i - 1]（t0 已經是 cubie + i）
    addi s1, s1, 1                  # + 1，變成位置編號
    la   t0, twist
    add  t0, t0, s0
    lbu  s2, -1(t0)                  # twist[i - 1]

have_ct:
    li   s4, 0                      # k = 0

slot_loop:
    # ---- s = k - t，負的就 + 3 ----
    sub  t1, s4, s2
    bgez t1, s_ok
    addi t1, t1, 3

s_ok:
    # ---- 顏色 = home_color[c][s]：位址 = home_color + c*12 + s*4 ----
    slli t2, s1, 3                  # c * 8
    slli t3, s1, 2                  # c * 4
    add  t2, t2, t3                 # c * 12
    slli t3, t1, 2                  # s * 4
    add  t2, t2, t3
    la   t3, home_color
    add  t2, t2, t3
    lw   a2, 0(t2)                  # 顏色
    # ---- (col, row) = slot_cell[i][k]：位址 = s3 + k*2 ----
    slli t3, s4, 1                  # k * 2
    add  t3, s3, t3
    lbu  a0, 0(t3)                  # col
    lbu  a1, 1(t3)                  # row
    jal  ra, draw_facelet
    addi s4, s4, 1
    li   t0, 3
    bne  s4, t0, slot_loop          # 3 個 slot
    addi s3, s3, 6                  # 下一個位置的 slot_cell（每個位置幾個位元組？）
    addi s0, s0, 1
    li   t0, 8
    bne  s0, t0, pos_loop           # 8 個位置

    li   a7, 10
    ecall

    li   a7, 10                         # exit
    ecall

# ---------------------------------------------------------------------------
# draw_facelet (from net_test.s): a0 = col, a1 = row, a2 = colour.
# Leaf function: uses only t0..t6, leaves a0..a2 unchanged.
# ---------------------------------------------------------------------------
draw_facelet:
    # t0 = LED_MATRIX_0_BASE + row_off[row] + col_off[col]
    la   t5, row_off
    slli t6, a1, 2                      # row * 4（word 表）
    add  t5, t5, t6
    lw   t5, 0(t5)                      # t5 = row_off[row]
    la   t6, col_off
    slli t0, a0, 2                      # col * 4（word 表）
    add  t6, t6, t0
    lw   t6, 0(t6)                      # t6 = col_off[col]
    li   t0, LED_MATRIX_0_BASE
    add  t0, t0, t5
    add  t0, t0, t6

    # 3 x 4 block, as in led_test.s; colour from a2
    # t2 = 目前的 LED 位址, t3 = 剩下幾列, t4 = 這一列剩下幾個 LED
    li   t3, 3                          # 3 列
draw_row:
    mv   t2, t0                         # 從這一列的起點開始
    li   t4, 4                          # 4 個 LED
draw_led:
    sw   a2, 0(t2)                      # 寫入顏色
    addi t2, t2, 4                      # 往右一個 LED（4 B）
    addi t4, t4, -1
    bnez t4, draw_led                   # 這一列還沒畫完
    addi t0, t0, LED_ROW_BYTES          # 往下一列
    addi t3, t3, -1
    bnez t3, draw_row                   # 還有列沒畫
    ret
