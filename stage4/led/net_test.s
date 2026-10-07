# net_test.s -- LED stage 2: draw_facelet and the solved net
#
# Build: make (in this directory). Run net_test.elf in the Ripes GUI with an
# LED Matrix of Width 35, Height 25 in the I/O tab.
#
# The net is 8 x 6 facelets, 4 x 3 pixels each, with a 1-pixel gap between
# faces (between columns 1|2, 3|4, 5|6 and rows 1|2, 3|4):
#
#   col:  0 1 | 2 3 | 4 5 | 6 7
#   row 0       | U U |
#   row 1       | U U |
#   row 2 L L | F F | R R | B B
#   row 3 L L | F F | R R | B B
#   row 4       | D D |
#   row 5       | D D |
#
# x pixel = col * 4 + (col >> 1),  y pixel = row * 3 + (row >> 1)
# LED byte offset = y * 140 + x * 4   (140 = 35 LEDs x 4 bytes per row)
# Last column: x = 31, pixels 31..34 of 35. Last row: y = 17, rows 17..19 of 20.
#
# Status:
#   draw_facelet  done. Verified in a CLI run on a RAM buffer: (col 2, row 0)
#                 writes exactly x = 9..12, y = 0..2.
#   _start        TODO: draw all 6 faces from the faces table.

    # Check against the Ripes I/O tab (see led_test.s).
    .equ LED_MATRIX_0_BASE, 0xf0000000
    .equ LED_ROW_BYTES, 140

    .section .rodata
    .balign 4
# Byte offset of the top-left LED of each facelet column (x * 4) and row
# (y * 140), from the formulas above.
col_off:
    .word 0, 16, 36, 52, 72, 88, 108, 124
row_off:
    .word 0, 420, 980, 1400, 1960, 2380
# Per face: top-left col, top-left row, colour 0x00RRGGBB (12 bytes each).
faces:
    .word 2, 0, 0x00FFFFFF              # U  白
    .word 0, 2, 0x00FF8000              # L  橘
    .word 2, 2, 0x0000C000              # F  綠
    .word 4, 2, 0x00FF0000              # R  紅
    .word 6, 2, 0x000000FF              # B  藍
    .word 2, 4, 0x00FFFF00              # D  黃

    .text
    .globl _start

_start:
    la   s0, faces                  # s0 = &faces[0]
    li   s1, 6                      # 6 個面
face_loop:
    lw   s2, 0(s0)                  # col0   （每筆 3 個 word，偏移量 0、4、8）
    lw   s3, 4(s0)                  # row0
    lw   s4, 8(s0)                  # color

    # 左上 (col0, row0)
    mv   a0, s2
    mv   a1, s3
    mv   a2, s4
    jal  ra, draw_facelet
    # 右上 (col0 + 1, row0)
    addi a0, s2, 1
    mv   a1, s3
    mv   a2, s4
    jal  ra, draw_facelet
    # 左下 (col0, row0 + 1)
    mv   a0, s2
    addi a1, s3, 1
    mv   a2, s4
    jal  ra, draw_facelet
    # 右下 (col0 + 1, row0 + 1)
    addi a0, s2, 1
    addi a1, s3, 1
    mv   a2, s4
    jal  ra, draw_facelet

    addi s0, s0, 12                  # 下一個面（一筆幾個位元組？）
    addi s1, s1, -1
    bnez s1, face_loop

    li   a7, 10                     # exit
    ecall

# ---------------------------------------------------------------------------
# draw_facelet: a0 = col (0..7), a1 = row (0..5), a2 = colour 0x00RRGGBB.
# Draws a 4 x 3 block. Leaf function: calls nothing, uses only t0..t6,
# leaves a0..a2 unchanged.
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
