# Build and run the RV32I solver on Ripes.
#
#   make run                         solve the default state on RV32_ISS
#   make run STATE=54721631111111    any 14-character state
#   make run PROC=RV32_5S            another processor model
#   make size                        bytes per section of the linked ELF
#   make run-ref STATE=...           the gcc -O2 reference build of
#   make size-ref                    ../stage3/solver_ida.c, same input
#
# Ripes' own assembler (pinned build) rejects .rodata and .if, so the program
# is assembled with GNU as, linked with the generated tables, and loaded into
# Ripes as an ELF (-t elf). The link uses --no-relax: otherwise ld rewrites
# la into gp-relative addi, but nothing here sets gp (Ripes starts it at 0),
# so .bss/.data accesses would go to the wrong addresses.
# The input state is inlined at assembly time from
# STATE through the generated input.s.
RV_PREFIX ?= riscv64-unknown-elf-
AS := $(RV_PREFIX)as
LD := $(RV_PREFIX)ld --no-relax
ASFLAGS := -march=rv32i -mabi=ilp32
RIPES ?= ripes
STATE ?= 21345671111111
PROC ?= RV32_ISS
TIMEOUT ?= 600000

.PHONY: all run size run-ref size-ref clean FORCE

all: solver.elf

# Rewritten only when STATE changes, so make rebuilds exactly when needed.
input.s: FORCE
	@printf '    .data\n    .globl input\ninput:\n    .string "%s"\n' \
		'$(STATE)' >$@.tmp
	@cmp -s $@.tmp $@ && rm -f $@.tmp || mv $@.tmp $@

../stage3/tables.s:
	$(MAKE) -C ../stage3 tables.s

%.o: %.s
	$(AS) $(ASFLAGS) $< -o $@

solver.elf: solver.o input.o ../stage3/tables.s
	$(AS) $(ASFLAGS) ../stage3/tables.s -o tables.o
	$(LD) -m elf32lriscv solver.o input.o tables.o -o $@

run: solver.elf
	@$(RIPES) --mode cli -t elf --src solver.elf --proc $(PROC) --iret \
		--timeout $(TIMEOUT)

size: solver.elf
	@$(RV_PREFIX)size -A solver.elf | \
		awk '$$1 ~ /^\.(text|data|bss|rodata|sdata|sbss|srodata)/ { print; \
		if ($$1 != ".text") d += $$2 } END { print "static data:", d, "B" }'

# Reference: the author's final C compiled exactly as the assignment states,
# gcc -O2 -march=rv32i -mabi=ilp32, plus what a bare-metal link needs (no
# libc, own _start). Same tables.s and input.s as the assembly.
REF_CFLAGS := -O2 -march=rv32i -mabi=ilp32 -std=c99 -ffreestanding -DTARGET

ref.o: ../stage3/solver_ida.c
	$(RV_PREFIX)gcc $(REF_CFLAGS) -c $< -o $@

ref.elf: ref.o input.o ../stage3/tables.s
	$(AS) $(ASFLAGS) ../stage3/tables.s -o tables.o
	$(LD) -m elf32lriscv ref.o input.o tables.o -o $@

run-ref: ref.elf
	@$(RIPES) --mode cli -t elf --src ref.elf --proc $(PROC) --iret \
		--timeout $(TIMEOUT)

size-ref: ref.elf
	@$(RV_PREFIX)size -A ref.elf | \
		awk '$$1 ~ /^\.(text|data|bss|rodata|sdata|sbss|srodata)/ { print; \
		if ($$1 != ".text") d += $$2 } END { print "static data:", d, "B" }'

clean:
	$(RM) *.o input.s solver.elf ref.elf
