DEST	      = /usr/mnews/bin
CC            = gcc -posix
LDFLAGS	      = -L.. -s
CLOCALFLAGS   = -I..# -DDEBUG -g
COMPFLAGS     = -O2 -DUSG
CFLAGS        = $(COMPFLAGS) $(CLOCALFLAGS)
ELIBS         = -lsbd
LIBS	      = $(ELIBS)# -linet

OBJS	      = get_nntp.o\
		nntpserv.o\
		../gethost.o\
		../chkbase.o\
		../log.o\
		dbopen.o

PROGRAM	      = ../nntpserv

SRCS	      = dbz.c\
		dbzhist.c\
		get_nntp.c\
		nntpserv.c

all:		$(PROGRAM)

$(PROGRAM):     $(OBJS)
		$(CC) $(LDFLAGS) $(OBJS) $(LIBS) -o $(PROGRAM)

dbopen.o: ../dbopen.c ../dbsubs.h ../conf.h
		$(CC) -c $(CFLAGS) -DNNTPSERV -o dbopen.o ../dbopen.c

clean:;		rm -f *.o *.b core $(PROGRAM)

install:	$(PROGRAM)
		install -s $(PROGRAM) $(DEST)
###
get_nntp.o: ../conf.h
nntpserv.o: ../conf.h ../dbsubs.h
