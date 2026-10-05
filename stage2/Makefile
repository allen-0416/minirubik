CC ?= cc
CFLAGS ?= -O2 -std=c99 -Wall -Wextra -Wno-unused-function -Wno-sign-compare
CAND ?= example_perfect.c
BIN := check-$(basename $(notdir $(CAND)))

.PHONY: all oracle check clean

all: oracle $(BIN)

oracle: out/dist.bin

out/dist.bin: oracle.c oracle_lib.h ../solver.c
	@mkdir -p out
	$(CC) $(CFLAGS) oracle.c -o oracle
	./oracle

check: $(BIN)

$(BIN): check.c oracle_lib.h candidate.h $(CAND) ../solver.c
	$(CC) $(CFLAGS) -DCANDIDATE='"$(CAND)"' check.c -o $@

clean:
	rm -f oracle compat survey check-* out/dist.bin out/d11.txt

compat: compat.c oracle_lib.h ../solver.c
	$(CC) $(CFLAGS) compat.c -o $@

survey: survey.c oracle_lib.h ../solver.c
	$(CC) $(CFLAGS) survey.c -o $@
