##  $Id$

include ../Makefile.global

top	        = ..
CFLAGS		= $(GCFLAGS)

ALL		= innfeed procbatch startinnfeed imapfeed

SOURCES		= article.c buffer.c config_l.c config_y.c \
		  endpoint.c host.c innlistener.c main.c misc.c \
		  startinnfeed.c tape.c version.c

INCLUDES	= article.h buffer.h configfile.h config_y.h connection.h \
		  endpoint.h host.h innfeed.h innlistener.h misc.h msgs.h \
		  tape.h

# The objects linked into innfeed.  All SOURCES except startinnfeed.
OBJECTS		= article.o buffer.o config_l.o config_y.o \
		  endpoint.o host.o innlistener.o main.o misc.o tape.o \
		  version.o

INSTALLED	= $(D)$(PATHBIN)/innfeed \
		  $(D)$(PATHBIN)/procbatch \
		  $(D)$(PATHBIN)/startinnfeed \
		  $(D)$(PATHBIN)/imapfeed

all: $(ALL)

warnings:
	$(MAKE) COPT='$(WARNINGS)' all

clean:
	rm -f *.o $(ALL) version.c innfeed-convcfg
	rm -rf .libs

clobber distclean: clean 
	rm -f tags y.tab.c y.tab.h lex.yy.c config_y.c config_y.h

tags: $(SOURCES) $(INCLUDES)
	$(CTAGS) $(SOURCES) $(INCLUDES)


##  Compilation rules.

INNFEEDLIBS	= $(LIBSTORAGE) $(LIBHIST) $(LIBINN) $(EXTSTORAGELIBS) $(LIBS)

config_y.c config_y.h: configfile.y
	$(YACC) -d $?
	mv y.tab.h config_y.h
	mv y.tab.c config_y.c

config_l.c: configfile.l
	$(LEX) $?
	mv lex.yy.c config_l.c

version.c: Makefile ../Makefile.global
	version=`echo '$(VERSION) ($(VERSION_EXTRA))' | sed 's/ ()//'` ; \
	echo 'const char *versionInfo = "innfeed' "$$version\" ;" > $@

innfeed: $(OBJECTS) connection.o $(LIBSTORAGE) $(LIBINN)
	$(LIBLD) $(LDFLAGS) -o $@ $(OBJECTS) connection.o $(INNFEEDLIBS)

imapfeed: $(OBJECTS) imap_connection.o $(LIBSTORAGE) $(LIBINN)
	$(LIBLD) $(LDFLAGS) -o $@ $(OBJECTS) imap_connection.o $(INNFEEDLIBS)

procbatch: procbatch.in $(FIXSCRIPT)
	$(FIXSCRIPT) procbatch.in

startinnfeed: startinnfeed.o $(LIBINN)
	$(LIBLD) $(LDFLAGS) -o $@ startinnfeed.o $(LIBINN) $(LIBS)

# Not normally built.
innfeed-convcfg: innfeed-convcfg.in $(FIXSCRIPT)
	$(FIXSCRIPT) -i innfeed-convcfg.in

tst: config_y.c config_l.c
	gcc -DWANT_MAIN -o tst -g -Wall config_y.c config_l.c -ly -ll


##  Installation rules.  Installation commands set in Makefile.global.

install: $(INSTALLED)

$(D)$(PATHBIN)/innfeed:		innfeed		; $(LI_XPRI) $? $@
$(D)$(PATHBIN)/imapfeed:	imapfeed	; $(LI_XPRI) $? $@
$(D)$(PATHBIN)/procbatch:	procbatch	; $(CP_XPRI) $? $@

$(D)$(PATHBIN)/startinnfeed: startinnfeed
	@ME=`$(WHOAMI)` ; \
	if [ x"$$ME" = xroot ] ; then \
	    echo $(LI_SPRI) startinnfeed $(D)$(PATHBIN)/startinnfeed ; \
	    $(LI_SPRI) startinnfeed $(D)$(PATHBIN)/startinnfeed ; \
	else \
	    echo $(LI_XPRI) startinnfeed $(D)$(PATHBIN)/startinnfeed ; \
	    $(LI_XPRI) startinnfeed $(D)$(PATHBIN)/startinnfeed ; \
	    echo '' ; \
	    echo '========================' ; \
	    echo 'NOTE NOTE NOTE NOTE NOTE' ; \
	    ls -l $@ ; \
	    echo '$@ needs to be installed setuid root' ; \
	    echo '' ; echo ; \
	fi


##  Dependencies.  Default list, below, is probably good enough.

depend: Makefile $(SOURCES)
	$(MAKEDEPEND) $(CFLAGS) $(SOURCES)

# DO NOT DELETE THIS LINE -- make depend depends on it.
article.o: article.c innfeed.h ../include/inn/timer.h \
 ../include/config.h ../include/clibrary.h ../include/portable/mmap.h \
 ../include/libinn.h ../include/storage.h article.h misc.h buffer.h \
 endpoint.h msgs.h
buffer.o: buffer.c ../include/libinn.h ../include/inn/defines.h \
 ../include/config.h innfeed.h ../include/inn/timer.h buffer.h misc.h
config_l.o: config_l.c innfeed.h ../include/inn/timer.h configfile.h \
 config_y.h ../include/libinn.h ../include/config.h
config_y.o: config_y.c innfeed.h ../include/inn/timer.h \
 ../include/config.h ../include/clibrary.h configfile.h \
 ../include/libinn.h msgs.h misc.h
endpoint.o: endpoint.c innfeed.h ../include/inn/timer.h \
 ../include/config.h ../include/clibrary.h \
 ../include/portable/socket.h ../include/portable/time.h \
 ../include/libinn.h buffer.h misc.h configfile.h endpoint.h host.h \
 msgs.h
host.o: host.c innfeed.h ../include/inn/timer.h ../include/config.h \
 ../include/clibrary.h ../include/portable/socket.h \
 ../include/libinn.h article.h misc.h buffer.h configfile.h \
 connection.h endpoint.h host.h innlistener.h msgs.h tape.h
innlistener.o: innlistener.c innfeed.h ../include/inn/timer.h \
 ../include/config.h ../include/clibrary.h ../include/libinn.h \
 article.h misc.h buffer.h configfile.h endpoint.h host.h \
 innlistener.h msgs.h ../include/nntp.h tape.h
main.o: main.c innfeed.h ../include/inn/timer.h ../include/config.h \
 ../include/clibrary.h ../include/portable/socket.h \
 ../include/portable/time.h ../include/libinn.h ../include/storage.h \
 article.h misc.h buffer.h configfile.h connection.h endpoint.h host.h \
 innlistener.h msgs.h tape.h
misc.o: misc.c innfeed.h ../include/inn/timer.h ../include/config.h \
 ../include/clibrary.h ../include/libinn.h endpoint.h misc.h msgs.h \
 tape.h
startinnfeed.o: startinnfeed.c ../include/config.h \
 ../include/inn/defines.h ../include/clibrary.h ../include/libinn.h \
 ../include/macros.h
tape.o: tape.c innfeed.h ../include/inn/timer.h ../include/config.h \
 ../include/clibrary.h ../include/libinn.h article.h misc.h \
 configfile.h endpoint.h msgs.h tape.h
version.o: version.c
