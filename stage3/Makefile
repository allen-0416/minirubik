# Stage 3: the author's final C (solver_ida.c), the table generator, and the
# host correctness gates.
#
#   make                 host solver_ida, gen_tables, tables.s
#   make h1 / make h2    gates H1 and H2 (gen_tables runs both, then writes
#                        tables.s only if they pass)
#   make h3              gate H3 over all 3,674,160 states (~7.5 min)
#   make check           test vectors and invalid inputs, like ../Makefile
#   make no-m            RV32I build of the search: no M, no libgcc, no libc
#
# The RV32I reference build of solver_ida.c (gcc -O2) is in ../stage4.
CC ?= cc
CFLAGS ?= -O2 -std=c99 -Wall -Wextra -Wpedantic -Wno-sign-compare
RV_PREFIX ?= riscv64-unknown-elf-
VECTORS := ../tests/solutions.txt
INVALID_STATES := 1234567111111 123456711111111 02345671111111 82345671111111 \
	12345671111110 12345671111114 1234567111111a 11345671111111 12345671111112

.PHONY: all h1 h2 h3 check no-m clean

all: solver_ida gen_tables tables.s

solver_ida: solver_ida.c practice/my_tables.h practice/my_pdb.h
	$(CC) $(CFLAGS) $< -o $@

# Gates H1 and H2, then the read-only tables for the RV32I program.
gen_tables: gen_tables.c practice/my_tables.h practice/my_pdb.h ../solver.c
	$(CC) $(CFLAGS) $< -o $@

tables.s: gen_tables ../stage2/out/dist.bin
	./gen_tables $@ | tee data/h1_h2.txt

h1 h2: gen_tables ../stage2/out/dist.bin
	./gen_tables tables.s | tee data/h1_h2.txt

../stage2/out/dist.bin:
	$(MAKE) -C ../stage2 oracle

# H3 runs the author's exercise-5 search through its test harness.
h3: ../stage2/out/dist.bin
	$(CC) $(CFLAGS) -D_POSIX_C_SOURCE=200809L practice/ex5_stack.c \
		-o practice/ex5
	cd practice && ./ex5 all | tee ../data/h3_own.txt

# Same solution length as the baseline for every vector (the path itself may
# differ: any shortest path is accepted), and the baseline's rejections.
check: solver_ida
	@n=0; while IFS='|' read -r state solution; do \
		case "$$state" in ""|\#*) continue ;; esac; \
		got=$$(./solver_ida "$$state") || { echo "$$state: failed"; exit 1; }; \
		want=$$(printf '%s\n' "$$solution" | wc -w); \
		have=$$(printf '%s\n' "$$got" | wc -w); \
		test "$$want" -eq "$$have" || { \
			echo "$$state: $$have moves, want $$want"; exit 1; }; \
		n=$$((n + 1)); \
	done <$(VECTORS); echo "$$n vectors: optimal length"
	@for bad in $(INVALID_STATES); do \
		./solver_ida "$$bad" >/dev/null 2>&1; s=$$?; \
		test $$s -eq 2 || { echo "$$bad: status $$s, want 2"; exit 1; }; \
	done; echo "invalid inputs rejected with status 2"

# Compile the target build (tables external) and require no undefined
# symbol other than the tables and the input string.
no-m: solver_ida.c
	$(RV_PREFIX)gcc -O2 -march=rv32i -mabi=ilp32 -std=c99 -ffreestanding \
		-DTARGET -c $< -o solver_ida_rv.o
	@undef=$$($(RV_PREFIX)nm -u solver_ida_rv.o | awk '{print $$2}' | \
		grep -vxE 'perm_move|ori_move|pdb_p|pdb_o|input'); \
		test -z "$$undef" || { echo "undefined: $$undef"; exit 1; }
	@echo "RV32I build calls no external routine (only tables and input)"

clean:
	$(RM) solver_ida gen_tables solver_ida_rv.o practice/ex5
