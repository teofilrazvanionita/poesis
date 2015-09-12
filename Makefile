CC = gcc
CFLAGS = -Wall -g
LDLIBS = -pthread
#MYSQL_LIBS = -L/usr/lib -lmysqlclient -lpthread -lz -lm -lssl -lcrypto -ldl
MYSQL_LIBS = `mysql_config --libs`
#MYSQL_CFLAGS = -I/usr/include/mysql
MYSQL_CFLAGS = `mysql_config --cflags`

all: poesis

poesis: main.o
	$(CC) $^ -o $@ $(LDLIBS) $(MYSQL_LIBS)

main.o: main.c
	$(CC) $(CFLAGS) $(MYSQL_CFLAGS) $(LDLIBS) $< -c -o $@

.PHONY: clean
clean:
	rm -f *.o *~ poesis
