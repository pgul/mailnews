HDRS	      = dbsubs.h
LDFLAGS	      = -L.. -g# -s
# If you want to use gcc 2.* uncomment folowin lines
CC            = gcc -posix -Wall
LOCALFLAGS    = -DSDBA -I..
COMPFLAGS     = -DUSG -DHAVE_STDLIB -g#-O2 -fomit-frame-pointer 
CFLAGS        = $(COMPFLAGS) $(LOCALFLAGS)
LLIBS         = #-lcrypt -linet
ELIBS         = -lsbd
LIBS          = $(LLIBS) $(ELIBS)

MAKEFILE      = Makefile

NNTPOBJS      = get_host.o

OBJS	      = sdba.o\
		sdbacnv.o\
		chkbase.o\
		check.o\
		subs.o\
		getuser.o\
		set_uparam.o\
		../gethost.o\
		../connect_nnrp.o\
		../sofgets.o\
		../log.o\
		dbopen.o $(NNTPOBJS)

PROGRAM	      = ../sdba

all:		$(PROGRAM)

$(PROGRAM):     $(OBJS) ../libsbd.a
		$(CC) $(LDFLAGS) $(OBJS) $(LIBS) -o $(PROGRAM)

sdba.o:		coms.h ../conf.h ../dbsubs.h ../nnrp.h

clean:;		rm -f *.o core $(PROGRAM)

check.o:	../check.c ../dbsubs.h
		$(CC) $(CFLAGS) -c ../check.c
subs.o:		../subs.c ../dbsubs.h
		$(CC) $(CFLAGS) -c ../subs.c
set_uparam.o:	../set_uparam.c ../dbsubs.h
		$(CC) $(CFLAGS) -c ../set_uparam.c
dbopen.o:	../dbopen.c ../dbsubs.h
		$(CC) $(CFLAGS) -c ../dbopen.c
chkbase.o:	../chkbase.c ../dbsubs.h
		$(CC) $(CFLAGS) -c ../chkbase.c
sdbacnv.o:	../conf.h ../dbsubs.h ../nnrp.h
getuser.o:	../getuser.c ../conf.h ../dbsubs.h
		$(CC) $(CFLAGS) -c ../getuser.c
