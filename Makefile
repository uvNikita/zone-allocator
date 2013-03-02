all: clean test.bin

test.bin:
		gcc -std=gnu99 -g -Wall allocator.c test.c -o test.bin

main.bin:
		gcc -std=gnu99 -g -Wall allocator.c main.c -o main.bin

clean:
		rm -f *.o *.bin
