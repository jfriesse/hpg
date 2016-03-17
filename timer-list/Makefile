CFLAGS+=-Wall -ggdb

all: timer-list

timer-list: timer-list.c
	$(CC) $(CFLAGS) `pkg-config --cflags nspr` `pkg-config --cflags nss` timer-list.c \
	`pkg-config --libs nspr` `pkg-config --libs nss` -D _TIMER_LIST_UNIT_TEST_ -o timer-list
