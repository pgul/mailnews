LDFLAGS	      = -L../..# -s
# If you want to use gcc 2.* uncomment folowin lines
CC            = gcc -posix -Wall
LOCALFLAGS    = -DSDBA -I../..
COMPFLAGS     = -DCONVERT -DUSG -O2 -g#-fomit-frame-pointer 
CFLAGS        = $(COMPFLAGS) $(LOCALFLAGS)
LIBS          = -lsbd
OWNER         = news
GROUP         = news


MAKEFILE      = Makefile

OBJS	      = convert.o\
		sdbacnv.o\
		subs.o\
		dbopen.o\
		../../chkbase.o\
		../../gethost.o

PROGRAM	      = convert

all:		$(PROGRAM)

$(PROGRAM):     $(OBJS) ../../libsbd.a 
		$(CC) $(LDFLAGS) $(OBJS) $(LIBS) -o $(PROGRAM)
		chown $(OWNER) $(PROGRAM)
		chgrp $(GROUP) $(PROGRAM)
		chmod ug+s $(PROGRAM)

convert.o:		../../conf.h ../../dbsubs.h

clean:;		rm -f *.o core $(PROGRAM)

convert.o: olddbsubs.h

sdbacnv.o: ../../sdba.d/sdbacnv.c ../../dbsubs.h
		$(CC) $(CFLAGS) -c ../../sdba.d/sdbacnv.c

dbopen.o: ../../dbopen.c ../../dbsubs.h
		$(CC) $(CFLAGS) -c ../../dbopen.c

subs.o:	../../subs.c
		$(CC) $(CFLAGS) -c ../../subs.c
