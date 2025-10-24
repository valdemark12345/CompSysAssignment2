CC=gcc
CFLAGS=-g -Wall -Wextra -pedantic -std=gnu99 -pthread
EXAMPLES=fibs fauxgrep fauxgrep-mt fhistogram fhistogram-mt
TESTS=fhistogram-test.sh fauxgrep-test.sh
.PHONY: all test clean ../src.zip

all: $(TESTS) $(EXAMPLES)

job_queue.o: job_queue.c job_queue.h
	$(CC) -c job_queue.c $(CFLAGS)

%: %.c job_queue.o
	$(CC) -o $@ $^ $(CFLAGS)

test: $(TESTS)
	@set e; for test in $(TESTS); do echo ./$$test; ./$$test; done

clean:
	rm -rf $(EXAMPLES) *.o *.dSYM core

format:
	clang-format -i $(EXAMPLES) *.c

zip: ../src.zip

../src.zip:
	make clean
	cd .. && zip src.zip -r src
