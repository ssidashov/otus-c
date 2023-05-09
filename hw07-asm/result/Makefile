AS = nasm

all: fixed-prog

fixed-prog: fixed-prog.o
	$(CC) -no-pie $^ -o $@

fixed-prog.o: leak_fixed_optimized_main.asm
	$(AS) -felf64 $^ -o $@

clean:
	rm -f fixed-prog *.o core

PHONY: clean
