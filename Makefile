PRIV_CFLAGS += -I./lib/
PRIV_LDFLAGS += -lpthread -lrt
SRCS = $(wildcard ./lib/*.c)
CC=gcc
#CC=aarch64-linux-gnu-gcc

all:
	gcc share_ser.c $(SRCS) share_utils.c -o share_ser $(PRIV_CFLAGS) $(PRIV_LDFLAGS)
	gcc share_cli.c $(SRCS) share_utils.c -o share_cli $(PRIV_CFLAGS) $(PRIV_LDFLAGS)

clean:
	rm -rf share_ser share_cli

