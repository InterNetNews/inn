##  $Revision$
include ../Makefile.global

##  =()<P	= @<P>@>()=
P	= 

##  =()<LINTFLAGS	= @<LINTFLAGS>@>()=
LINTFLAGS	= -b -h -z  -I../include $(DEFS)

######################################################################
####           NOTHING BELOW HERE IS REALLY CONFIGURABLE          ####
######################################################################

.SUFFIXES:	.E .sh .pl

CPP.c		= $(CC) $(CFLAGS) -E -I.
COMPILE.c	= $(CC) $(CFLAGS) -c -I.
LINK.o		= $(CC) $(LDFLAGS) ($LIBS)
LINK.c		= $(CC) $(CFLAGS) $(LDFLAGS) $(LIBS)
OUTPUT_OPTION	= -o $@



SRC 		= article.c \
		  buffer.c \
		  connection.c \
		  endpoint.c \
		  host.c \
		  innlistener.c \
		  main.c \
		  misc.c \
		  tape.c

INCS		= article.h \
		  buffer.h \
		  config.h \
		  configfile.h \
		  connection.h \
		  endpoint.h \
		  host.h \
		  innlistener.h \
		  misc.h \
		  msgs.h \
		  sysconfig.h \
		  tape.h

MANSRC		= innfeed.1 innfeed.conf.5

EXTRAS		= testListener.pl inet_addr.c uio_maxiov.c configfile.y \
		  configfile.l config_l.c config_y.c config_y.h \
		  procbatch.pl convertconfig.pl malloc.c ChangeLog \
		  startinnfeed.c

OBJS		= $(SRC:.c=.o) \
		  config_l.o config_y.o

INNFEED_BIN	= $(NEWSBIN)/innfeed

STARTINNFEED	= $(NEWSBIN)/startinnfeed


ALL_INSTALLED	= $(NEWSBIN)/innfeed $(NEWSBIN)/startinnfeed \
		  $(NEWSBIN)/procbatch $(NEWSBIN)/convertconfig

MAN_INSTALLED   = $(MANDIR)/man1/innfeed.1 $(MANDIR)/man5/innfeed.conf.5

ALL_GCC_WARNINGS = -pedantic -W -Waggregate-return -Wcast-align \
		   -Wcast-qual -Wcomment -Wformat \
                   -Wimplicit -Winline -Wmissing-prototypes \
                   -Wmissing-declarations -Wnested-externs \
                   -Wparentheses -Wpointer-arith \
                   -Wshadow -Wstrict-prototypes -Wswitch \
                   -Wtraditional -Wtrigraphs \
                   -Wunused -Wwrite-strings

# -Wreturn-type -Wtemplate-debugging 

MORE_WARN	= -Wconversion 

VERSION		= 0.10.1
COMPRESS	= gzip
COMPRESS_EXT	= gz
TARSRC		= $(SRC) $(INCS) Makefile README INSTALL innfeed.conf \
		  $(EXTRAS) $(MANSRC) makedepend.sh
TARDIR		= innfeed-$(VERSION)
TARBALL		= $(TARDIR).tar.$(COMPRESS_EXT)


TAR		= tar
MKDIR		= mkdir
CP		= cp -p
RM		= rm -f
MV		= mv
TAG		= etags
CHMOD		= chmod
INSTALL		= install -c 

MAKEDEPEND	= ./makedepend
DEPENDFLAGS	= -s -c '$(CC) -E'

.c.o:
	$(COMPILE.c) $< $(OUTPUT_OPTION)

.c.E:
	$(CPP.c) $< $(OUTPUT_OPTION)

.sh:
	$(RM) $@ ; $(CP) $< $@ ; $(CHMOD) a-w,a+x $@

.pl:
	$(RM) $@ ; $(CP) $< $@ ; $(CHMOD) a-w,a+x $@

all: innfeed startinnfeed procbatch convertconfig

install: $(ALL_INSTALLED)

innfeed: $(OBJS) version.o $(MALLOC) $(LIBNEWS)
	$(LINK.c) -o $@ $(OBJS) version.o $(LIBNEWS) $(LIBNEWS)

$D$(NEWSBIN)/innfeed: innfeed
	$(SHELL) ../installit.sh $(OWNER) -m 550 -b .OLD $? $@

$D$(NEWSBIN)/procbatch: procbatch
	$(SHELL) ../installit.sh $(OWNER) -m 550 -b .OLD $? $@


$D$(NEWSBIN)/convertconfig: convertconfig
	$(SHELL) ../installit.sh $(OWNER) -m 550 -b .OLD $? $@
	$(SAVEOLD)
	$(COPYFILE)

$D$(NEWSBIN)/startinnfeed: startinnfeed
	-$(SHELL) ../installit.sh -O root -G $(NEWSGROUP) -m 4550 -b .OLD $? $@
	@ME=`(whoami || /usr/ucb/whoami) 2> /dev/null` ;\
	if [ "X$$ME" = Xroot ]; then \
		echo $@ has been installed setuid root. ;\
	else\
		echo "" ; echo "===============================" ;\
		echo  "NOTE NOTE NOTE NOTE NOTE" ;\
		ls -l $@ ;\
		echo $@ needs to be installed setuid root ;\
		echo "" ; echo "" ;\
	fi

objs:	$(OBJS)

tags: $(SRC) $(INCS)
	$(TAG) $(SRC) $(INCS)

clean::
	$(RM) version.c *.E *.o *.a *.core makedepend tags TAGS core \#* *~ 
	$(RM) Makefile.BAK procbatch convertconfig
	$(RM) innfeed startinnfeed uio_maxiov listing.ps logfile lint.log

realclean:: clean 
	$(RM) y.tab.c y.tab.h lex.yy.c config_y.c config_y.h config_l.c
	$(RM) -r $(TARDIR)
	$(RM) $(TARBALL)

distclean:: realclean rcsclean

.PHONY:	tar listing

listing::
	@$(RM) foo.ps ; \
	F=`/bin/ls *.c *.h 2> /dev/null | sed 's/h$$/H/' |\
	   sort | sed 's/H$$/h/'` ;\
	CMD="nenscript -p- $$F | psnup -4 -d1 > listing.ps" ;\
	if [ "X$$F" != X ]; then echo $$CMD ; eval $$CMD ; fi

startinnfeed.o: startinnfeed.c
	$(COMPILE.c) -DINNFEED=\"$(INNFEED_BIN)\" -o $@ $?

startinnfeed: startinnfeed.o
	$(LINK.c) -o $@ $?

uio_maxiov: uio_maxiov.o
	$(LINK.c) -o $@ uio_maxiov.o

check-maxiov: uio_maxiov
	@./uio_maxiov

makedepend: makedepend.sh

version.c: $(OBJS)
	echo 'const char *versionInfo = "innfeed v. $(VERSION) ('\
		`date` ')" ;' |\
		sed -e 's/( /(/' -e 's/ )/)/' > version.c

tar: $(TARBALL)

tardir: $(TARSRC)
	$(RM) -r $(TARDIR)
	$(MKDIR) $(TARDIR)
	$(CP) $(TARSRC) $(TARDIR)

$(TARBALL): tardir
	$(TAR) cvf - $(TARDIR) | $(COMPRESS) > $(TARBALL)

depend: makedepend $(SRC) $(INCS)
	$(MAKEDEPEND) $(DEPENDFLAGS) $(CPPFLAGS) $(SRC)

diffdir::
	echo -n "Directory? " ; read dir ;\
	if [ -d $$dir ]; then\
		for i in *.c *.h Makefile;do\
			[ ! -f $$dir/$$i ] && continue;\
			if ! cmp $$i $$dir/$$i > /dev/null 2>&1; then\
				echo patch $$i "<< 'EOF'";\
				diff $$dir/$$i $$i;\
				echo EOF;\
			fi;\
		done;\
	else\
		echo No such directory $$dir;\
	fi

lint:	$(OBJECTS)
	-lint -u $(LINTFLAGS) $(SRC) > lint.log

config_y.c config_y.h: configfile.y
	$(YACC) $?
	mv y.tab.h config_y.h
	mv y.tab.c config_y.c

config_l.c: configfile.l
	$(LEX) $?
	mv lex.yy.c config_l.c

config_l.o: config_y.o config_l.c

tst:	config_y.c config_l.c
	gcc -DWANT_MAIN -o tst -g -Wall config_y.c config_l.c -ly -ll

# DO NOT DELETE THIS LINE -- make depend depends on it.

article.o: article.c article.h buffer.h config.h endpoint.h misc.h \
   msgs.h sysconfig.h
buffer.o: buffer.c buffer.h config.h misc.h sysconfig.h
connection.o: article.h buffer.h config.h configfile.h connection.c \
   connection.h endpoint.h host.h misc.h msgs.h sysconfig.h
endpoint.o: buffer.h config.h configfile.h endpoint.c endpoint.h host.h \
   misc.h msgs.h sysconfig.h
host.o: article.h buffer.h config.h configfile.h connection.h endpoint.h \
   host.c host.h innlistener.h misc.h msgs.h sysconfig.h tape.h
innlistener.o: article.h buffer.h config.h configfile.h endpoint.h host.h \
   innlistener.c innlistener.h misc.h msgs.h sysconfig.h tape.h
main.o: article.h buffer.h config.h configfile.h connection.h endpoint.h \
   host.h innlistener.h main.c misc.h msgs.h sysconfig.h tape.h
misc.o: config.h endpoint.h misc.c misc.h msgs.h sysconfig.h tape.h
tape.o: article.h config.h configfile.h endpoint.h misc.h msgs.h \
   sysconfig.h tape.c tape.h
