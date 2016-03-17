CFLAGS+=-Wall -ggdb

all: timer-list

timer-list: timer-list.c timer-list-ut.c
	$(CC) $(CFLAGS) `pkg-config --cflags nspr` `pkg-config --cflags nss` timer-list-ut.c timer-list.c \
	`pkg-config --libs nspr` `pkg-config --libs nss`  -o timer-list
