#
# Usenet to E-mail Gateway
#
# (C) Copyright 1992 by Stanislav Voronyi, Kharkov, Ukraine
# All rights reserved.
#
OWNER=  news
GROUP=  news
MAILGROUP=mail
#
# If you want to use gcc 2.* uncomment folowin lines
CC            = gcc #-posix
CFLAGS = -Wall -m486 -DUSG -D__linux__ -Dlint -DNEED_UNCHAR -O2 -g# -fomit-frame-pointer
LDFLAGF = -g
#
SHELL=/bin/sh
LIBDIR=/usr/mnews/lib
LBINDIR=/usr/mnews/bin
BINDIR=/usr/bin
LIBS=-L. -lsbd
INSTALL = install -s -c -m 500 -o $(OWNER) -g $(GROUP) 

all:    libsbd.a newsserver feeder lister newgroups newsserv sdba #nntpserv

libsbd.a:	dbsubs.h libs/*.c
		(cd libs;make "CC=$(CC)" "COMPFLAGS=$(CFLAGS)")

newsserver:	newsserv.o gsn.o date.o gethost.o grecn.o subs.o set_uparam.o\
		check.o dbopen.o chkbase.o message.o version.o log.o \
		mnewssmtp.o connect_nnrp.o sofgets.o getuser.o libsbd.a
	$(CC) $(LDFLAGS) -o newsserver newsserv.o gsn.o date.o gethost.o\
		grecn.o subs.o check.o dbopen.o chkbase.o set_uparam.o\
		message.o version.o log.o mnewssmtp.o connect_nnrp.o sofgets.o\
		getuser.o $(LIBS)

feeder: feeder.o gsn.o gethost.o lock.o dbopen.o chkbase.o log.o mnewssmtp.o nntprecv.o sofgets.o
	$(CC) $(LDFLAGS) -o feeder feeder.o gsn.o gethost.o lock.o log.o mnewssmtp.o nntprecv.o sofgets.o
#		dbopen.o chkbase.o  $(LIBS) # This need to compile feeder
# without MEMMAP

lister: lister.o date.o gethost.o lock.o notfunc.o message.o dbopen.o\
	chkbase.o log.o unmime.o mnewssmtp.o
	$(CC) $(LDFLAGS) -o lister lister.o date.o gethost.o lock.o notfunc.o\
		message.o log.o unmime.o mnewssmtp.o
#		dbopen.o chkbase.o $(LIBS) # This need to compile lister
#without MEMMAP

newgroups: newgroups.o date.o gethost.o notfunc.o dbopen.o message.o\
	   chkbase.o connect_nnrp.o log.o sofgets.o
		$(CC) $(LDFLAGS) -o newgroups newgroups.o date.o gethost.o\
		notfunc.o message.o connect_nnrp.o log.o sofgets.o
#		dbopen.o chkbase.o $(LIBS) # This need without MEMMAP

newsserv:  daemon.o log.o connect_nnrp.o sofgets.o
		$(CC) $(LDFLAGS) -o newsserv daemon.o log.o connect_nnrp.o sofgets.o

sdba:	sdba.d/*.c check.c dbopen.c set_uparam.c subs.c getuser.c libsbd.a\
	conf.h dbsubs.h nnrp.h
	(cd sdba.d;make "CC=$(CC)" "COMPFLAGS=$(CFLAGS)" "ELIBS=$(LIBS)")

nntpserv:	;@[ -d nntp ] && (cd nntp;make "CC=$(CC)" "COMPFLAGS=$(CFLAGS)" "ELIBS=$(LIBS)") || echo No nntp part

letters.h:	Letters lettertab
		./lettertab Letters > letters.h

lettertab:	lettertab.c
		$(CC) -s -O -o lettertab lettertab.c

check.o:	dbsubs.h
chkbase.o:	dbsubs.h
dbopen.o:	dbsubs.h
mailerr.o:	dbsubs.h
set_uparam.o:	dbsubs.h
subs.o:		dbsubs.h
unmime.o:	dbsubs.h
mnewssmtp.o:	mnewssmtp.h
nntprecv.o:	conf.h dbsubs.h
sofgets.o:	conf.h
connect_nnrp.o:	conf.h dbsubs.h nnrp.h
daemon.o:	conf.h nnrp.h
getuser.o:	conf.h dbsubs.h

newsserv.o:	coms.h conf.h dbsubs.h letters.h nnrp.h mnewssmtp.h

feeder.o:	conf.h dbsubs.h mnewssmtp.h
newgroups.o:	conf.h dbsubs.h nnrp.h 
lock.o: conf.h
lister.o:	conf.h dbsubs.h getuser.c mnewssmtp.h

install: all
	[ -d /usr/mnews ] || mkdir /usr/mnews
	[ -d /usr/mnews/lib ] || mkdir /usr/mnews/lib
	[ -d /usr/mnews/bin ] || mkdir /usr/mnews/bin
	chown $(OWNER) /usr/mnews
	chgrp $(GROUP) /usr/mnews
	chown $(OWNER) /usr/mnews/bin
	chgrp $(GROUP) /usr/mnews/bin
	chown $(OWNER) /usr/mnews/lib
	chgrp $(GROUP) /usr/mnews/lib
	install -s -c -m 4550 -o $(OWNER) -g $(MAILGROUP) newsserver $(LBINDIR)
	install -s -c -m 4550 -o $(OWNER) -g $(MAILGROUP) newsserv $(LBINDIR)
	$(INSTALL) feeder $(LBINDIR)
	$(INSTALL) lister $(LBINDIR)
	$(INSTALL) sdba $(BINDIR)
	cp copybase.sh $(LIBDIR)
	cp files/mnews.help.* files/list.help.* files/list-help.* $(LIBDIR)
	cp files/news.* $(LIBDIR)
	cp files/msg.* $(LIBDIR)
	cp files/bad.users files/extcmds files/otlup.txt $(LIBDIR)
	cp files/moderators $(LIBDIR)
	cp files/bad.users.sh $(LBINDIR)
	chown $(OWNER) $(LIBDIR)/mnews.help.*
	chown $(OWNER) $(LIBDIR)/list.help.*
	chown $(OWNER) $(LIBDIR)/list-help.*
	chown $(OWNER) $(LIBDIR)/news.*
	chown $(OWNER) $(LIBDIR)/msg.*
	chown $(OWNER) $(LBINDIR)/bad.users.sh
	chmod 644  $(LIBDIR)/mnews.help.* $(LIBDIR)/list.help.*
	chmod 644  $(LIBDIR)/news.* $(LIBDIR)/msg.*
	[ -d /var/spool/mailnews ] || mkdir /var/spool/mailnews
	[ -d /var/spool/mailnews/base ] || mkdir /var/spool/mailnews/base
	chown $(OWNER) /var/spool/mailnews /var/spool/mailnews/base
	chgrp $(GROUP) /var/spool/mailnews /var/spool/mailnews/base
	chmod 700 /var/spool/mailnews/base
	[ -d /var/spool/mailnews/queue ] || mkdir /var/spool/mailnews/queue
	chown $(OWNER) /var/spool/mailnews/queue
	chgrp $(GROUP) /var/spool/mailnews/queue
	chmod 700 /var/spool/mailnews/queue

clean:
	-rm -f *.o
	-rm -f *.b *.bak newsserver lister feeder sdba newgroups newsserv lettertab letters.h
	(cd sdba.d;make clean)
	(cd libs;make clean)
	([ -d nntp ] &&(cd nntp;make clean) || echo No nntp)
