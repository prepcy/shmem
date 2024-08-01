PRIV_CFLAGS += -I./lib/include/ -I.
PRIV_LDFLAGS += -lpthread -lrt
SRCS=$(wildcard lib/src/*.c)
#CC=gcc
CC=aarch64-linux-gnu-gcc

all:
	$(CC) -o client client.c $(SRCS) $(PRIV_CFLAGS) $(PRIV_LDFLAGS)
	$(CC) -o server server.c $(SRCS) $(PRIV_CFLAGS) $(PRIV_LDFLAGS)
clean:
	rm -rf client server
