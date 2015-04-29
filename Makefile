CC = gcc
CFLAGS = -Wall -g
LDLIBS = -pthread

all: poesis

poesis: main.o
	$(CC) $^ -o $@ $(LDLIBS)

main.o: main.c
	$(CC) $(CFLAGS) $(LDLIBS) $< -c -o $@

.PHONY: clean
clean:
	rm -f *.o *~ poesis
