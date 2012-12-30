##  $Id$

include ../Makefile.global

top	      = ..
CFLAGS	      = $(GCFLAGS) $(SASLINC)

ALL	      = innfeed procbatch imapfeed

SOURCES	      = article.c buffer.c config_l.c config_y.c connection.c endpoint.c \
		host.c imap_connection.c innlistener.c main.c misc.c \
		tape.c

INCLUDES      = article.h buffer.h configfile.h config_y.h connection.h \
		endpoint.h host.h innfeed.h innlistener.h misc.h tape.h

# The objects linked into innfeed.  All SOURCES except connection.o or
# imap_connection.o.
OBJECTS	      = article.o buffer.o config_l.o config_y.o endpoint.o host.o \
		innlistener.o main.o misc.o tape.o

all: $(ALL)

warnings:
	$(MAKE) COPT='$(WARNINGS)' all

install: all
	$(LI_XPRI) innfeed $D$(PATHBIN)/innfeed
	$(LI_XPRI) imapfeed $D$(PATHBIN)/imapfeed
	$(CP_XPRI) procbatch $D$(PATHBIN)/procbatch

bootstrap: config_y.c config_y.h config_l.c

clean:
	rm -f *.o $(ALL)
	rm -f innfeedp
	rm -rf .libs

clobber distclean: clean 
	rm -f y.tab.c y.tab.h lex.yy.c

maintclean: distclean
	rm -f config_l.c config_y.c config_y.h

$(FIXSCRIPT):
	@echo Run configure before running make.  See INSTALL for details.
	@exit 1


##  Compilation rules.

INNFEEDLIBS	= $(LIBSTORAGE) $(LIBHIST) $(LIBINN) $(STORAGE_LIBS) $(LIBS)

config_y.c config_y.h: configfile.y
	$(YACC) -d configfile.y
	mv y.tab.h config_y.h
	mv y.tab.c config_y.c
	touch config_y.h
config_y.h: config_y.c

config_l.c: configfile.l
	$(LEX) $?
	mv lex.yy.c config_l.c

imap_connection.o: imap_connection.c
	$(CC) $(CFLAGS) $(SASL_CPPFLAGS) -c $<

innfeed: $(OBJECTS) connection.o $(LIBSTORAGE) $(LIBINN)
	$(LIBLD) $(LDFLAGS) -o $@ $(OBJECTS) connection.o $(INNFEEDLIBS)

imapfeed: $(OBJECTS) imap_connection.o $(LIBSTORAGE) $(LIBINN)
	$(LIBLD) $(LDFLAGS) -o $@ $(OBJECTS) imap_connection.o \
	    $(SASL_LDFLAGS) $(SASL_LIBS) $(INNFEEDLIBS)

procbatch: procbatch.in $(FIXSCRIPT)
	$(FIXSCRIPT) procbatch.in

tst: config_y.c config_l.c
	gcc -DWANT_MAIN -o tst -g -Wall config_y.c config_l.c -ly -ll


##  Profiling.  These rules have not been checked for a while and may need
##  some work.

profiled: innfeedp

innfeedp: $(SOURCES)
	rm -f $(OBJECTS)
	$(MAKEPROFILING) innfeed
	mv innfeed innfeedp
	rm -f $(OBJECTS)


##  Dependencies.  Default list, below, is probably good enough.

depend: Makefile $(SOURCES)
	$(MAKEDEPEND) '$(CFLAGS)' $(SOURCES)

# DO NOT DELETE THIS LINE -- make depend depends on it.
article.o: article.c innfeed.h ../include/inn/timer.h \
  ../include/inn/defines.h ../include/inn/system.h ../include/config.h \
  ../include/inn/defines.h ../include/inn/options.h ../include/clibrary.h \
  ../include/config.h ../include/portable/mmap.h ../include/config.h \
  ../include/inn/messages.h ../include/inn/libinn.h \
  ../include/inn/storage.h ../include/inn/options.h article.h misc.h \
  buffer.h endpoint.h
buffer.o: buffer.c innfeed.h ../include/inn/timer.h \
  ../include/inn/defines.h ../include/inn/system.h ../include/config.h \
  ../include/inn/defines.h ../include/inn/options.h ../include/clibrary.h \
  ../include/config.h ../include/inn/messages.h ../include/inn/libinn.h \
  buffer.h misc.h
config_l.o: config_l.c innfeed.h ../include/inn/timer.h \
  ../include/inn/defines.h ../include/inn/system.h \
  ../include/inn/libinn.h configfile.h config_y.h misc.h \
  ../include/config.h ../include/inn/defines.h ../include/inn/options.h
config_y.o: config_y.c innfeed.h ../include/inn/timer.h \
  ../include/inn/defines.h ../include/inn/system.h ../include/config.h \
  ../include/inn/defines.h ../include/inn/options.h ../include/clibrary.h \
  ../include/config.h ../include/inn/messages.h ../include/inn/libinn.h \
  configfile.h misc.h
connection.o: connection.c innfeed.h ../include/inn/timer.h \
  ../include/inn/defines.h ../include/inn/system.h ../include/config.h \
  ../include/inn/defines.h ../include/inn/options.h ../include/clibrary.h \
  ../include/config.h ../include/portable/socket.h ../include/config.h \
  ../include/portable/getaddrinfo.h ../include/portable/getnameinfo.h \
  ../include/portable/time.h ../include/inn/innconf.h \
  ../include/inn/messages.h ../include/inn/network.h \
  ../include/portable/socket.h ../include/inn/libinn.h article.h misc.h \
  buffer.h configfile.h connection.h endpoint.h host.h
endpoint.o: endpoint.c innfeed.h ../include/inn/timer.h \
  ../include/inn/defines.h ../include/inn/system.h ../include/config.h \
  ../include/inn/defines.h ../include/inn/options.h ../include/clibrary.h \
  ../include/config.h ../include/portable/socket.h ../include/config.h \
  ../include/portable/getaddrinfo.h ../include/portable/getnameinfo.h \
  ../include/portable/time.h ../include/inn/innconf.h \
  ../include/inn/messages.h ../include/inn/libinn.h buffer.h misc.h \
  configfile.h endpoint.h host.h
host.o: host.c innfeed.h ../include/inn/timer.h ../include/inn/defines.h \
  ../include/inn/system.h ../include/config.h ../include/inn/defines.h \
  ../include/inn/options.h ../include/clibrary.h ../include/config.h \
  ../include/portable/socket.h ../include/config.h \
  ../include/portable/getaddrinfo.h ../include/portable/getnameinfo.h \
  ../include/inn/innconf.h ../include/inn/messages.h \
  ../include/inn/network.h ../include/portable/socket.h \
  ../include/inn/version.h ../include/inn/libinn.h article.h misc.h \
  buffer.h configfile.h connection.h endpoint.h host.h innlistener.h \
  tape.h
imap_connection.o: imap_connection.c ../include/config.h \
  ../include/inn/defines.h ../include/inn/system.h \
  ../include/inn/options.h ../include/clibrary.h ../include/config.h \
  ../include/portable/socket.h ../include/config.h \
  ../include/portable/getaddrinfo.h ../include/portable/getnameinfo.h \
  ../include/inn/messages.h ../include/inn/defines.h \
  ../include/inn/libinn.h buffer.h misc.h connection.h endpoint.h host.h \
  innfeed.h ../include/inn/timer.h article.h configfile.h
innlistener.o: innlistener.c innfeed.h ../include/inn/timer.h \
  ../include/inn/defines.h ../include/inn/system.h ../include/config.h \
  ../include/inn/defines.h ../include/inn/options.h ../include/clibrary.h \
  ../include/config.h ../include/inn/libinn.h ../include/inn/messages.h \
  ../include/inn/nntp.h article.h misc.h buffer.h configfile.h endpoint.h \
  host.h innlistener.h tape.h
main.o: main.c innfeed.h ../include/inn/timer.h ../include/inn/defines.h \
  ../include/inn/system.h ../include/config.h ../include/inn/defines.h \
  ../include/inn/options.h ../include/clibrary.h ../include/config.h \
  ../include/portable/socket.h ../include/config.h \
  ../include/portable/getaddrinfo.h ../include/portable/getnameinfo.h \
  ../include/portable/time.h ../include/inn/innconf.h \
  ../include/inn/messages.h ../include/inn/version.h \
  ../include/inn/libinn.h ../include/inn/storage.h \
  ../include/inn/options.h article.h misc.h buffer.h configfile.h \
  connection.h endpoint.h host.h innlistener.h tape.h
misc.o: misc.c innfeed.h ../include/inn/timer.h ../include/inn/defines.h \
  ../include/inn/system.h ../include/config.h ../include/inn/defines.h \
  ../include/inn/options.h ../include/clibrary.h ../include/config.h \
  ../include/inn/messages.h ../include/inn/libinn.h endpoint.h misc.h \
  tape.h
tape.o: tape.c innfeed.h ../include/inn/timer.h ../include/inn/defines.h \
  ../include/inn/system.h ../include/config.h ../include/inn/defines.h \
  ../include/inn/options.h ../include/clibrary.h ../include/config.h \
  ../include/inn/innconf.h ../include/inn/messages.h \
  ../include/inn/libinn.h article.h misc.h configfile.h endpoint.h tape.h
