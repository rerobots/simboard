CFLAGS = -Wall


repl-simavr: repl.c
	$(CC) $(CFLAGS) -o repl-simavr -lsimavr repl.c

.PHONY: clean
clean:
	rm -rf repl-simavr *.dSYM
