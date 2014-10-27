BINNAME=kopou
CC=gcc
CWARN=-W
CWARN+=-Waddress 
CWARN+=-Warray-bounds  
CWARN+=-Wchar-subscripts  
CWARN+=-Wenum-compare
CWARN+=-Wimplicit-int 
CWARN+=-Wimplicit-function-declaration 
CWARN+=-Wcomment  
CWARN+=-Wformat
CWARN+=-Wmain  
CWARN+=-Wmissing-braces 
CWARN+=-Wnonnull  
CWARN+=-Wparentheses  
#CWARN+=-Wpointer-sign  
CWARN+=-Wreturn-type  
CWARN+=-Wsequence-point  
CWARN+=-Wstrict-aliasing  
CWARN+=-Wstrict-overflow=1  
#CWARN+=-Wswitch  
CWARN+=-Wtrigraphs  
CWARN+=-Wuninitialized  
CWARN+=-Wunknown-pragmas  
CWARN+=-Wunused-parameter
CWARN+=-Wunused-function  
CWARN+=-Wunused-label     
CWARN+=-Wunused-value     
CWARN+=-Wunused-variable  
CWARN+=-Wvolatile-register-var  

COPT=-O2
CFLAGS=-pthread
CDEBUG=-g -ggdb
SRC=aarray.c kevent.c main.c kstring.c list.c http.c settings.c tcp.c xalloc.c networks.c reply.c database.c map.c bucket.c jenkins_hash.c bgsaver.c version.c
HDR=aarray.h common.h kevent.h kopou.h kstring.h list.h tcp.h xalloc.h map.h jenkins_hash.h

all: $(BINNAME) 
	mv -f $(BINNAME) ../

$(BINNAME): $(SRC) $(HDR)
	$(CC) $(CWARN) $(COPT) $(CFLAGS) $(SRC) -o $(BINNAME)

debug: $(SRC) $(HDR)
	$(CC) $(CWARN) $(CDEBUG) $(CFLAGS) $(SRC) -o $(BINNAME)
	mv -f $(BINNAME) ../

.PHONY: install 
install:
	@echo "not ready to install yet, try later."
