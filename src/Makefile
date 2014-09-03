BINNAME=kopou
CC=gcc
CWARN=-Wall -W
COPT=-O2
CINC=-I../lib/
CFLAGS=-pthread
CDEBUG=-g -ggdb
SRC=aarray.c kevent.c kopou.c kstring.c list.c parsing.c settings.c tcp.c xalloc.c
HDR=aarray.h config.h database.h kevent.h kopou.h kstring.h list.h tcp.h xalloc.h

all: $(BINNAME) 
	mv -f $(BINNAME) ../

$(BINNAME): $(SRC) $(HDR)
	$(CC) $(CWARN) $(COPT) $(CFLAGS) $(CINC) $(SRC) -o $(BINNAME)

debug: $(SRC) $(HDR)
	$(CC) $(CWARN) $(CDEBUG) $(CFLAGS) $(CINC) $(SRC) -o $(BINNAME)
	mv -f $(BINNAME) ../

.PHONY: install 
install:
	@echo "not ready to install yet, try couple of months later."
