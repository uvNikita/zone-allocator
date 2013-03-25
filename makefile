CFLAGS := -lm -std=gnu99 -g -Wall 

all: clean test.bin main.bin

allocator.o: allocator.c allocator.h
	gcc $(CFLAGS) -c allocator.c

test.o: test.c allocator.h
	gcc $(CFLAGS) -c test.c

test.bin: allocator.o test.o
		gcc $(CFLAGS) allocator.o test.o -o test.bin

main.o: main.c allocator.h
	gcc $(CFLAGS) -c main.c

main.bin: allocator.o main.o
		gcc $(CFLAGS) allocator.o main.o -o main.bin

clean:
		rm -f *.o *.bin
