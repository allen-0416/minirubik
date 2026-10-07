# led_test.s -- LED stage 1: light one 4 x 3 facelet at pixel (0, 0)
#
# Build: make (in this directory). Run in the Ripes GUI with an LED Matrix of
# Width 35, Height 25 in the I/O tab.
#
# Layout (assignment; examples/C/leds.c): one 32-bit word per LED holding
# 0x00RRGGBB, row-major: address = BASE + (y * WIDTH + x) * 4.
# One row is 35 * 4 = 140 bytes, so moving down one row is +140: no multiply.
#
# Status: done. Verified in a CLI run on a RAM buffer: exactly 12 words written.

    # Check against the Ripes I/O tab.
    .equ LED_MATRIX_0_BASE, 0xf0000000
    .equ LED_ROW_BYTES, 140             # 35 LEDs x 4 bytes

    .text
    .globl _start
_start:
    li   t0, LED_MATRIX_0_BASE          # t0 = 這一列的起點
    li   t1, 0x00FF0000                 # t1 = 紅色

    # t2 = 目前的 LED 位址, t3 = 剩下幾列, t4 = 這一列剩下幾個 LED
    li   t3, 3                          # 3 列
draw_row:
    mv   t2, t0                         # 從這一列的起點開始
    li   t4, 4                          # 4 個 LED
draw_led:
    sw   t1, 0(t2)                      # 寫入顏色
    addi t2, t2, 4                      # 往右一個 LED（4 B）
    addi t4, t4, -1
    bnez t4, draw_led                   # 這一列還沒畫完
    addi t0, t0, LED_ROW_BYTES          # 往下一列
    addi t3, t3, -1
    bnez t3, draw_row                   # 還有列沒畫

    li   a7, 10                         # exit
    ecall
