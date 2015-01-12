CFLAGS+=-Wall -ggdb

sserver: sserver.c
	$(CC) $(CFLAGS) `pkg-config --cflags nspr` `pkg-config --cflags nss` nss-sock.c sserver.c \
	`pkg-config --libs nspr` `pkg-config --libs nss` -o sserver

prclist-test: prclist-test.c
	$(CC) $(CFLAGS) `pkg-config --cflags nspr` `pkg-config --cflags nss` nss-sock.c prclist-test.c \
	`pkg-config --libs nspr` `pkg-config --libs nss` -o prclist-test
