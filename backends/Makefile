##  $Revision$

include ../Makefile.global

CFLAGS	= $(GCFLAGS)

SOURCES	=	archive.c batcher.c buffchan.c cvtbatch.c filechan.c \
		innxmit.c innxbatch.c nntpget.c shlock.c \
		shrinkfile.c crosspost.c actsync.c inndf.c overchan.c

ALL	=	archive batcher buffchan cvtbatch filechan \
		innxmit innxbatch nntpget shlock \
		shrinkfile crosspost actsync actsyncd actmerge \
		sendxbatches inndf overchan

all:		$(ALL)

install:	all
	for i in ${ALL}; \
		do $(LIBTOOL) ../installit.sh $(OWNER) -m 0555 \
			-b .OLD $${i} $D${PATHBIN}/$${i}; \
	done

clobber clean:
	rm -f *.o $(ALL)
	rm -rf .libs
	rm -f all install profiled

tags ctags:	$(SOURCES)
	$(CTAGS) $(SOURCES) ../lib/*.c ../include/*.h

profiled:
	cp /dev/null profiled

archive:	$(P) archive.o $(LIBNEWS) 
	$(LIBTOOL) $(CC) $(LDFLAGS) -o $@ archive.o $(LIBNEWS) $(LIBS)

batcher:	$(P) batcher.o $(LIBNEWS)
	$(LIBTOOL) $(CC) $(LDFLAGS) -o $@ batcher.o $(LIBNEWS) $(LIBS)

buffchan:	$(P) buffchan.o map.o $(LIBNEWS)
	$(LIBTOOL) $(CC) $(LDFLAGS) -o $@ buffchan.o map.o $(LIBNEWS) $(LIBS)

cvtbatch:	$(P) cvtbatch.o $(LIBNEWS)
	$(LIBTOOL) $(CC) $(LDFLAGS) -o $@ cvtbatch.o $(LIBNEWS) $(LIBS)

filechan:	$(P) filechan.o map.o $(LIBNEWS)
	$(LIBTOOL) $(CC) $(LDFLAGS) -o $@ filechan.o map.o $(LIBNEWS) $(LIBS)

nntpget:	$(P) nntpget.o $(LIBNEWS)
	$(LIBTOOL) $(CC) $(LDFLAGS) -o $@ nntpget.o $(LIBNEWS) $(LIBS)

innxmit:	$(P) innxmit.o $(LIBNEWS)
	$(LIBTOOL) $(CC) $(LDFLAGS) -o $@ innxmit.o $(LIBNEWS) $(LIBS)

innxbatch:	$(P) innxbatch.o $(LIBNEWS)
	$(LIBTOOL) $(CC) $(LDFLAGS) -o $@ innxbatch.o $(LIBNEWS) $(LIBS)

sendxbatches:	$(P) sendxbatches.sh
	rm -f $@
	cp $? $@
	chmod +x $@

crosspost:	$(P) crosspost.o $(LIBNEWS)
	$(LIBTOOL) $(CC) $(LDFLAGS) -o $@ crosspost.o $(LIBNEWS) $(LIBS)

actsync:	$(P) actsync.o $(LIBNEWS)
	$(LIBTOOL) $(CC) $(LDFLAGS) -o $@ actsync.o $(LIBNEWS) $(LIBS)

actsyncd:	$(P) actsyncd.sh
	rm -f $@
	cp $? $@
	chmod +x $@

actmerge: 	$(P) actmerge.sh
	rm -f $@
	cp $? $@
	chmod +x $@

shlock:		$(P) shlock.o $(LIBNEWS)
	$(LIBTOOL) $(CC) $(LDFLAGS) -o $@ shlock.o $(LIBNEWS) $(LIBS)

shrinkfile:	$(P) shrinkfile.o $(LIBNEWS)
	$(LIBTOOL) $(CC) $(LDFLAGS) -o $@ shrinkfile.o $(LIBNEWS) $(LIBS)

overchan:	$(P) overchan.o $(LIBNEWS)
	$(LIBTOOL) $(CC) $(LDFLAGS) -o $@ overchan.o $(LIBNEWS) $(LIBS)

inndf:	$(P) inndf.o
	$(LIBTOOL) $(CC) $(LDFLAGS) -o $@ inndf.o


$(LIBNEWS):
	(cd ../lib ; $(MAKE) )

##  Dependencies.  Default list, below, is probably good enough.
depend:		Makefile $(SOURCES)
	makedepend $(DEFS) $(SOURCES)

# DO NOT DELETE THIS LINE -- make depend depends on it.
actsync.o:	../include/clibrary.h
actsync.o:	../include/configdata.h
actsync.o:	../include/libinn.h
actsync.o:	../include/macros.h
actsync.o:	../include/paths.h
actsync.o:	../include/qio.h
archive.o:	../include/clibrary.h
archive.o:	../include/configdata.h
archive.o:	../include/libinn.h
archive.o:	../include/macros.h
archive.o:	../include/paths.h
archive.o:	../include/qio.h
batcher.o:	../include/clibrary.h
batcher.o:	../include/configdata.h
batcher.o:	../include/libinn.h
batcher.o:	../include/macros.h
batcher.o:	../include/paths.h
buffchan.o:	../include/clibrary.h
buffchan.o:	../include/configdata.h
buffchan.o:	../include/libinn.h
buffchan.o:	../include/macros.h
buffchan.o:	../include/paths.h
buffchan.o:	../include/qio.h
cvtbatch.o:	../include/clibrary.h
cvtbatch.o:	../include/configdata.h
cvtbatch.o:	../include/libinn.h
cvtbatch.o:	../include/macros.h
cvtbatch.o:	../include/paths.h
cvtbatch.o:	../include/qio.h
filechan.o:	../include/clibrary.h
filechan.o:	../include/configdata.h
filechan.o:	../include/libinn.h
filechan.o:	../include/macros.h
filechan.o:	../include/paths.h
innxbatch.o:	../include/clibrary.h
innxbatch.o:	../include/configdata.h
innxbatch.o:	../include/libinn.h
innxbatch.o:	../include/macros.h
innxbatch.o:	../include/nntp.h 
innxmit.o:	../include/clibrary.h
innxmit.o:	../include/configdata.h
innxmit.o:	../include/dbz.h
innxmit.o:	../include/libinn.h
innxmit.o:	../include/macros.h
innxmit.o:	../include/nntp.h
innxmit.o:	../include/paths.h
innxmit.o:	../include/qio.h
nntpget.o:	../include/clibrary.h
nntpget.o:	../include/configdata.h
nntpget.o:	../include/dbz.h
nntpget.o:	../include/libinn.h
nntpget.o:	../include/macros.h
nntpget.o:	../include/nntp.h
nntpget.o:	../include/paths.h
shlock.o:	../include/clibrary.h
shlock.o:	../include/configdata.h
shrinkfile.o:	../include/clibrary.h
shrinkfile.o:	../include/configdata.h
shrinkfile.o:	../include/libinn.h
shrinkfile.o:	../include/macros.h
inndf.o:	../include/config.h
