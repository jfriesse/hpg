CFLAGS+=-Wall -ggdb

all: sserver sclient corosync-qdevice-net corosync-qnetd

sserver: sserver.c nss-sock.c
	$(CC) $(CFLAGS) `pkg-config --cflags nspr` `pkg-config --cflags nss` nss-sock.c sserver.c \
	`pkg-config --libs nspr` `pkg-config --libs nss` -o sserver

sclient: sclient.c nss-sock.c
	$(CC) $(CFLAGS) `pkg-config --cflags nspr` `pkg-config --cflags nss` nss-sock.c sclient.c \
	`pkg-config --libs nspr` `pkg-config --libs nss` -o sclient

prclist-test: prclist-test.c
	$(CC) $(CFLAGS) `pkg-config --cflags nspr` `pkg-config --cflags nss` nss-sock.c prclist-test.c \
	`pkg-config --libs nspr` `pkg-config --libs nss` -o prclist-test

corosync-qdevice-net: corosync-qdevice-net.c nss-sock.c tlv.c msg.c msgio.c dynar.c
	$(CC) $(CFLAGS) `pkg-config --cflags nspr` `pkg-config --cflags nss` \
	nss-sock.c tlv.c msg.c msgio.c dynar.c \
	corosync-qdevice-net.c \
	`pkg-config --libs nspr` `pkg-config --libs nss` -o corosync-qdevice-net

corosync-qnetd: corosync-qnetd.c nss-sock.c tlv.c msg.c msgio.c qnetd-clients-list.c qnetd-client.c \
    qnetd-poll-array.c qnetd-log.c dynar.c
	$(CC) $(CFLAGS) `pkg-config --cflags nspr` `pkg-config --cflags nss` \
	nss-sock.c tlv.c msg.c msgio.c qnetd-clients-list.c qnetd-client.c qnetd-poll-array.c \
	qnetd-log.c dynar.c \
	corosync-qnetd.c \
	`pkg-config --libs nspr` `pkg-config --libs nss` -o corosync-qnetd
