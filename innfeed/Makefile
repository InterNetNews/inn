##  $Id$

include ../Makefile.global

######################################################################
####           NOTHING BELOW HERE IS REALLY CONFIGURABLE          ####
######################################################################

CFLAGS	= $(GCFLAGS)

.SUFFIXES:	.E .sh .pl

CPP.c		= $(CC) $(CFLAGS) -E -I.
COMPILE.c	= $(CC) $(CFLAGS) -c -I.
LINK.c		= $(LIBTOOL) $(CC) $(CFLAGS) $(LDFLAGS)
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
		  configfile.h \
		  connection.h \
		  endpoint.h \
		  host.h \
		  innfeed.h \
		  innlistener.h \
		  misc.h \
		  msgs.h \
		  tape.h

OBJS		= $(SRC:.c=.o) \
		  config_l.o config_y.o

INNFEED_BIN	= $D$(PATHBIN)/innfeed

STARTINNFEED	= $D$(PATHBIN)/startinnfeed


ALL_INSTALLED	= $D$(PATHBIN)/innfeed $D$(PATHBIN)/startinnfeed \
		  $D$(PATHBIN)/procbatch $D$(PATHBIN)/innfeed-convcfg

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

VERSION		= 1.0

CP		= cp -p
RM		= rm -f
MV		= mv
TAG		= etags
CHMOD		= chmod

MAKEDEPEND	= ./makedepend
DEPENDFLAGS	= -s -c '$(CC) -E'

.c.o:
	$(COMPILE.c) $< $(OUTPUT_OPTION)

.c.E:
	$(CPP.c) $< $(OUTPUT_OPTION)

.sh:
	$(RM) $@ ; $(CP) $< $@ ; $(CHMOD) a-w,a+x $@

all: innfeed startinnfeed innfeed-convcfg procbatch

install: $(ALL_INSTALLED)

innfeed: $(OBJS) version.o $(MALLOC) $(LIBSTORAGE) $(LIBINN)
	$(LINK.c) -o $@ $(OBJS) version.o $(LIBSTORAGE) $(LIBINN) $(EXTSTORAGELIBS) $(LIBS)

innfeed-convcfg: innfeed-convcfg.in $(FIXSCRIPT)
	$(FIXSCRIPT) -i innfeed-convcfg.in

procbatch: procbatch.in $(FIXSCRIPT)
	$(FIXSCRIPT) procbatch.in

$(D)$(PATHBIN)/innfeed: innfeed
	$(LI_XPRI) $? $@

$(D)$(PATHBIN)/procbatch: procbatch
	$(CP_XPRI) $? $@

$(D)$(PATHBIN)/innfeed-convcfg: innfeed-convcfg
	$(CP_XPRI) $? $@
	$(SAVEOLD)
	$(COPYFILE)

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

objs:	$(OBJS)

tags: $(SRC) $(INCS)
	$(TAG) $(SRC) $(INCS)

clean::
	$(RM) version.c *.E *.o *.a *.core makedepend tags TAGS core \#* *~ 
	$(RM) Makefile.BAK
	$(RM) innfeed startinnfeed uio_maxiov listing.ps logfile
	$(RM) innfeed-convcfg procbatch
	$(RM) -rf .libs

realclean:: clean 
	$(RM) y.tab.c y.tab.h lex.yy.c config_y.c config_y.h

clobber distclean:: realclean

.PHONY:	listing

listing::
	@$(RM) foo.ps ; \
	F=`/bin/ls *.c *.h 2> /dev/null | sed 's/h$$/H/' |\
	   sort | sed 's/H$$/h/'` ;\
	CMD="nenscript -p- $$F | psnup -4 -d1 > listing.ps" ;\
	if [ "X$$F" != X ]; then echo $$CMD ; eval $$CMD ; fi

startinnfeed.o: startinnfeed.c
	$(COMPILE.c) $?

startinnfeed: startinnfeed.o $(LIBINN)
	$(LINK.c) -o $@ startinnfeed.o $(LIBINN) $(LIBS)

makedepend: makedepend.sh

version.c: $(OBJS)
	echo 'const char *versionInfo = "innfeed v. $(VERSION) ('\
		`date` ')" ;' |\
		sed -e 's/( /(/' -e 's/ )/)/' > version.c

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

config_y.c config_y.h: configfile.y
	$(YACC) -d $?
	mv y.tab.h config_y.h
	mv y.tab.c config_y.c

config_l.c: configfile.l
	$(LEX) $?
	mv lex.yy.c config_l.c

config_l.o: config_y.o config_l.c

tst:	config_y.c config_l.c
	gcc -DWANT_MAIN -o tst -g -Wall config_y.c config_l.c -ly -ll

# DO NOT DELETE THIS LINE -- make depend depends on it.

article.o: article.c article.h buffer.h innfeed.h endpoint.h misc.h \
   msgs.h
buffer.o: buffer.c buffer.h innfeed.h misc.h
connection.o: article.h buffer.h innfeed.h configfile.h connection.c \
   connection.h endpoint.h host.h misc.h msgs.h
endpoint.o: buffer.h innfeed.h configfile.h endpoint.c endpoint.h host.h \
   misc.h msgs.h
host.o: article.h buffer.h innfeed.h configfile.h connection.h endpoint.h \
   host.c host.h innlistener.h misc.h msgs.h tape.h
innlistener.o: article.h buffer.h innfeed.h configfile.h endpoint.h host.h \
   innlistener.c innlistener.h misc.h msgs.h tape.h
main.o: article.h buffer.h innfeed.h configfile.h connection.h endpoint.h \
   host.h innlistener.h main.c misc.h msgs.h tape.h
misc.o: innfeed.h endpoint.h misc.c misc.h msgs.h tape.h
tape.o: article.h innfeed.h configfile.h endpoint.h misc.h msgs.h \
   tape.c tape.h
