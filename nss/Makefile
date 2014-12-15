CFLAGS+=-Wall -ggdb

sserver: sserver.c
	$(CC) $(CFLAGS) `pkg-config --cflags nspr` `pkg-config --cflags nss` sserver.c \
	`pkg-config --libs nspr` `pkg-config --libs nss` -o sserver
