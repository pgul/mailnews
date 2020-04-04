LIBRARY	      = ../libsbd.a

COMPFLAGS      = -DUSG -O2 -g
CFLAGS         = $(COMPFLAGS) -Wall -I..
CC             = gcc -posix 

OBJS	      = add_free.o\
		dbz.o\
		dbzhist.o\
		ext.o\
		flock.o\
		f_size.o\
		get_domain.o\
		get_free.o\
		get_gname.o\
		get_gtree.o\
		get_uaddr.o\
		get_uparam.o\
		get_utree.o\
		getgroup.o\
		isdis.o\
		issubs.o\
		lower_str.o\
		pberr.o\
		rm_tree.o\
		sav_entry.o\
		sav_hdr.o\
		savebase.o\
		strins.o\
		strhash.o\
		wildmat.o
all:		$(LIBRARY)

$(LIBRARY):	$(OBJS)
		@echo -n "Loading $(LIBRARY) ... "
		@ar cru $(LIBRARY) $(OBJS)
		@ranlib $(LIBRARY)
		@echo "done"

clean:;		@rm -f $(OBJS) $(LIBRARY)

library:        $(LIBRARY)

###
add_free.o: ../dbsubs.h
get_domain.o: ../dbsubs.h
get_free.o: ../dbsubs.h
get_gname.o: ../dbsubs.h
get_gtree.o: ../dbsubs.h
get_uaddr.o: ../dbsubs.h
get_uparam.o: ../dbsubs.h
get_utree.o: ../dbsubs.h
getgroup.o: ../dbsubs.h
issubs.o: ../dbsubs.h
pberr.o: ../dbsubs.h
rm_tree.o: ../dbsubs.h
sav_entry.o: ../dbsubs.h
sav_hdr.o: ../dbsubs.h
savebase.o: ../dbsubs.h
