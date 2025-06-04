TARGET_CLI = share_cli
TARGET_SER = share_ser

PRIV_CFLAGS += -I./lib/ -I./
PRIV_LDFLAGS += -lpthread -lrt
SRCS = $(wildcard ./lib/*.c) share_utils.c
OBJS=$(SRCS:.c=.o)
DEPS = $(OBJS:.o=.d)

CC=gcc
#CC=aarch64-linux-gnu-gcc

all: $(TARGET_CLI) $(TARGET_SER)

$(TARGET_CLI): $(OBJS) share_cli.o
	$(CC) -o $@  $(OBJS) share_cli.o $(PRIV_CFLAGS) $(PRIV_LDFLAGS)

$(TARGET_SER): $(OBJS) share_ser.o
	$(CC) -o $@ $(OBJS) share_ser.o $(PRIV_CFLAGS) $(PRIV_LDFLAGS)

%.o: %.c
	$(CC) -c $(PRIV_CFLAGS) $< -o $@

clean:
	rm -rf $(TARGET_CLI) $(TARGET_SER) $(OBJS) $(DEPS) share_cli.o share_ser.o

-include $(DEPS)
