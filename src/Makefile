BINNAME=kopou-server
CLIENTBINNAME=kopou-client
CC=gcc
CWARN=-Wall -W
COPT=-O2
CINC=-I../lib/
CFLAGS=-pthread
CDEBUG=-g -ggdb
SRC=aarray.c kevent.c kopou.c kstring.c list.c parsing.c settings.c tcp.c xalloc.c networks.c reply.c database.c
HDR=aarray.h config.h kevent.h kopou.h kstring.h list.h tcp.h xalloc.h
CSRC=kopou_client.c tcp.c
CHDR=kopou.h tcp.h

all: $(BINNAME) 
	mv -f $(BINNAME) ../
	mv -f $(CLIENTBINNAME) ../

$(BINNAME): $(SRC) $(HDR) $(CSRC) $(CHDR)
	$(CC) $(CWARN) $(COPT) $(CFLAGS) $(CINC) $(SRC) -o $(BINNAME)
	$(CC) $(CWARN) $(COPT) $(CFLAGS) $(CINC) $(CSRC) -o $(CLIENTBINNAME)

debug: $(SRC) $(HDR) $(CSRC) $(CHDR)
	$(CC) $(CWARN) $(CDEBUG) $(CFLAGS) $(CINC) $(SRC) -o $(BINNAME)
	mv -f $(BINNAME) ../
	$(CC) $(CWARN) $(CDEBUG) $(CFLAGS) $(CINC) $(CSRC) -o $(CLIENTBINNAME)
	mv -f $(CLIENTBINNAME) ../

.PHONY: install 
install:
	@echo "not ready to install yet, try couple of months later."
