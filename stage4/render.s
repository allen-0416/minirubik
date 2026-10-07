# render.s -- LED Matrix renderer, included by solver.s under .if RENDER
#
# Draws the cube as an unfolded net on a 35 x 25 LED Matrix and redraws it
# after every move the solver prints. The CLI build (RENDER = 0) contains
# none of this file.
#
# Net: 8 x 6 facelets of 4 x 3 pixels, a 1-pixel gap between faces:
#   col:  0 1 | 2 3 | 4 5 | 6 7
#   row 0,1       | U U |
#   row 2,3 L L | F F | R R | B B
#   row 4,5       | D D |
# x = col * 4 + (col >> 1), y = row * 3 + (row >> 1); one LED row is 140 bytes.
#
# Positions 0..7 follow README.md: 0 FUL (fixed), 1 FUR, 2 FDR, 3 FDL,
# 4 BUR, 5 BDR, 6 BDL, 7 BUL; cubie[k] / twist[k] describe position k + 1.
# Slots: a position's 3 stickers, slot 0 on U or D, then counterclockwise
# seen from outside the corner. twist t puts the cubie's U/D sticker in
# slot t, so slot k shows sticker s = k - t (+3 if negative).
#
# Registers (all free once the search has finished):
#   render_state      s0 i, s1 c, s2 t, s3 &slot_cell[i], s5 k, s7 saved ra
#   apply_move_state  a0 move (0..8), a1..a6, t0..t6
#   draw_facelet      a0 col, a1 row, a2 colour, t0..t6

    .equ LED_MATRIX_0_BASE, 0xf0000000  # GUI build: provided by Ripes instead
    .equ LED_ROW_BYTES, 140

    .section .rodata
    .balign 4
col_off:                        # x * 4 of each facelet column
    .word 0, 16, 36, 52, 72, 88, 108, 124
row_off:                        # y * 140 of each facelet row
    .word 0, 420, 980, 1400, 1960, 2380
home_color:                     # [c][s]: colour of sticker s of cubie c
    .word 0x00FFFFFF, 0x00FF8000, 0x0000C000   # 0 FUL: U L F
    .word 0x00FFFFFF, 0x0000C000, 0x00FF0000   # 1 FUR: U F R
    .word 0x00FFFF00, 0x00FF0000, 0x0000C000   # 2 FDR: D R F
    .word 0x00FFFF00, 0x0000C000, 0x00FF8000   # 3 FDL: D F L
    .word 0x00FFFFFF, 0x00FF0000, 0x000000FF   # 4 BUR: U R B
    .word 0x00FFFF00, 0x000000FF, 0x00FF0000   # 5 BDR: D B R
    .word 0x00FFFF00, 0x00FF8000, 0x000000FF   # 6 BDL: D L B
    .word 0x00FFFFFF, 0x000000FF, 0x00FF8000   # 7 BUL: U B L
slot_cell:                      # [i][k]: (col, row) of slot k at position i
    .byte 2, 1,  1, 2,  2, 2   # 0 FUL: U L F
    .byte 3, 1,  3, 2,  4, 2   # 1 FUR: U F R
    .byte 3, 4,  4, 3,  3, 3   # 2 FDR: D R F
    .byte 2, 4,  2, 3,  1, 3   # 3 FDL: D F L
    .byte 3, 0,  5, 2,  6, 2   # 4 BUR: U R B
    .byte 3, 5,  6, 3,  5, 3   # 5 BDR: D B R
    .byte 2, 5,  0, 3,  7, 3   # 6 BDL: D L B
    .byte 2, 0,  7, 2,  0, 2   # 7 BUL: U B L

# One quarter turn of face f, as in solver.c: position k takes
# cubie[qt_source[f][k]] with twist + qt_twist[f][k] (mod 3).
qt_source:
    .byte 1, 4, 2, 0, 3, 5, 6           # R
    .byte 0, 1, 2, 4, 5, 6, 3           # B
    .byte 0, 2, 5, 3, 1, 4, 6           # D
qt_twist:
    .byte 1, 2, 0, 2, 1, 0, 0           # R
    .byte 0, 0, 0, 1, 2, 1, 2           # B
    .byte 0, 0, 0, 0, 0, 0, 0           # D

    .bss
qt_cubie: .zero 8                       # result of one quarter turn
qt_twist_new: .zero 8

    .text
# ---------------------------------------------------------------------------
# render_state: draw all 24 facelets from cubie[] / twist[].
# ---------------------------------------------------------------------------
render_state:
    mv   s7, ra                         # calls draw_facelet
    li   s0, 0                          # position i
    la   s3, slot_cell
pos_loop:
    li   s1, 0                          # i = 0: the fixed cubie 0, twist 0
    li   s2, 0
    beqz s0, have_ct
    la   t0, cubie
    add  t0, t0, s0
    lbu  s1, -1(t0)                     # c = cubie[i - 1] + 1
    addi s1, s1, 1
    la   t0, twist
    add  t0, t0, s0
    lbu  s2, -1(t0)                     # t = twist[i - 1]
have_ct:
    li   s5, 0                          # slot k
slot_loop:
    sub  t1, s5, s2                     # s = k - t, +3 if negative
    bgez t1, s_ok
    addi t1, t1, 3
s_ok:
    slli t2, s1, 3                      # home_color + c * 12 + s * 4
    slli t3, s1, 2
    add  t2, t2, t3
    slli t3, t1, 2
    add  t2, t2, t3
    la   t3, home_color
    add  t2, t2, t3
    lw   a2, 0(t2)                      # colour
    slli t3, s5, 1                      # slot_cell[i] + k * 2
    add  t3, s3, t3
    lbu  a0, 0(t3)                      # col
    lbu  a1, 1(t3)                      # row
    jal  ra, draw_facelet
    addi s5, s5, 1
    li   t0, 3
    bne  s5, t0, slot_loop
    addi s3, s3, 6                      # 3 slots x 2 bytes
    addi s0, s0, 1
    li   t0, 8
    bne  s0, t0, pos_loop
    mv   ra, s7
    ret

# ---------------------------------------------------------------------------
# apply_move_state: apply move a0 (0..8, face * 3 + turn) to cubie[] and
# twist[], as turn + 1 quarter turns. Leaf.
# ---------------------------------------------------------------------------
apply_move_state:
    la   a1, qt_source                  # face rows, R first (7 bytes each)
    la   a2, qt_twist
    li   t6, 3
    bltu a0, t6, ams_turns
    addi a0, a0, -3                     # B
    addi a1, a1, 7
    addi a2, a2, 7
    bltu a0, t6, ams_turns
    addi a0, a0, -3                     # D
    addi a1, a1, 7
    addi a2, a2, 7
ams_turns:
    addi a0, a0, 1                      # quarter turns
    la   a3, cubie
    la   a4, twist
    la   a5, qt_cubie
    la   a6, qt_twist_new

ams_quarter:                            # one quarter turn into the scratch
    li   t0, 0                          # k
ams_k:
    add  t1, a1, t0
    lbu  t1, 0(t1)                      # src = qt_source[f][k]
    add  t2, a3, t1
    lbu  t2, 0(t2)                      # cubie[src]
    add  t3, a4, t1
    lbu  t3, 0(t3)                      # twist[src]
    add  t4, a2, t0
    lbu  t4, 0(t4)
    add  t3, t3, t4                     # + qt_twist[f][k], mod 3
    bltu t3, t6, ams_mod_ok
    addi t3, t3, -3
ams_mod_ok:
    add  t5, a5, t0
    sb   t2, 0(t5)
    add  t5, a6, t0
    sb   t3, 0(t5)
    addi t0, t0, 1
    li   t5, 7
    bne  t0, t5, ams_k

    li   t0, 0                          # copy the scratch back
ams_copy:
    add  t1, a5, t0
    lbu  t2, 0(t1)
    add  t1, a3, t0
    sb   t2, 0(t1)
    add  t1, a6, t0
    lbu  t2, 0(t1)
    add  t1, a4, t0
    sb   t2, 0(t1)
    addi t0, t0, 1
    li   t1, 7
    bne  t0, t1, ams_copy

    addi a0, a0, -1
    bnez a0, ams_quarter
    ret

# ---------------------------------------------------------------------------
# draw_facelet: fill the 4 x 3 block of facelet (a0 col, a1 row) with a2.
# Leaf.
# ---------------------------------------------------------------------------
draw_facelet:
    la   t5, row_off                    # t0 = base + row_off[row] + col_off[col]
    slli t6, a1, 2
    add  t5, t5, t6
    lw   t5, 0(t5)
    la   t6, col_off
    slli t0, a0, 2
    add  t6, t6, t0
    lw   t6, 0(t6)
    li   t0, LED_MATRIX_0_BASE
    add  t0, t0, t5
    add  t0, t0, t6

    li   t3, 3                          # rows
draw_row:
    mv   t2, t0
    li   t4, 4                          # LEDs per row
draw_led:
    sw   a2, 0(t2)
    addi t2, t2, 4
    addi t4, t4, -1
    bnez t4, draw_led
    addi t0, t0, LED_ROW_BYTES
    addi t3, t3, -1
    bnez t3, draw_row
    ret
