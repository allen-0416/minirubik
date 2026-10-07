# LED Matrix experiments for the Ripes GUI (not measured with --iret).
#
#   make            build led_test.elf
#   then in the Ripes GUI: I/O tab -> add LED Matrix (Width 35, Height 25),
#   load led_test.elf as an ELF program, and run.
#
# GNU as does not know Ripes' LED_MATRIX_0_BASE symbol, so led_test.s
# defines it with .equ; check the value against the I/O tab. --no-relax keeps
# la pc-relative: ld would otherwise use gp, which Ripes starts at 0.
RV_PREFIX ?= riscv64-unknown-elf-
ASFLAGS := -march=rv32i -mabi=ilp32

.PHONY: all ripes clean

all: led_test.elf net_test.elf state_test.elf

# For the Ripes GUI editor: Ripes' own assembler provides LED_MATRIX_0_BASE
# when an LED Matrix is instantiated, but rejects .section .rodata and
# .balign. Open the *_ripes.s file in the GUI instead of loading an ELF.
ripes: led_test_ripes.s net_test_ripes.s state_test_ripes.s

%_ripes.s: %.s
	sed -e 's/^    \.section \.rodata$$/    .data/' -e '/^    \.balign /d' \
		-e '/^    \.globl _start$$/d' -e '/^    \.equ LED_MATRIX_0_BASE,/d' $< >$@

%.o: %.s
	$(RV_PREFIX)as $(ASFLAGS) $< -o $@

%.elf: %.o
	$(RV_PREFIX)ld --no-relax -m elf32lriscv $< -o $@

clean:
	$(RM) *.o *.elf *_ripes.s
