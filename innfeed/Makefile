# -*- makefile -*-
#
# Author:       James Brister <brister@vix.com> -- berkeley-unix --
# Start Date:   Wed Nov 29 22:01:56 1995
# Project:      INN (innfeed)
# File:         Makefile
# RCSId:        $Id$
# Description:  Makefile for innfeed.
# 

# Where the programs get installed.
BINDIR		= /usr/news/local

# Where your man pages live.
MANDIR		= /usr/local/man

# The compiler you want. NOTE... AIX 3.2.5 users do NOT use CC. See the 
# comment in sysconfig.h
CC		= gcc

# How to run lex and yacc. Innfeed requires Flex (but you shouldn't need it).
LEX		= lex -d
YACC		= yacc -t -d

# Debug option for compiler -g or -O (or both if using gcc).
DBFLAGS		= -g 

# Add any architecture specific defines, flags etc. you need
OSFLAGS		=
# OSFLAGS	= -DSOLARIS			# uncomment for Solaris

# Any extra non-architecture specific defines you may need.
#
# Add -DUSE_INN_INCLUDES to get INN headers files used (see INCDIRS below)
# Add -DNO_SBRK if compilation of malloc.c complains about sbrk() prototype.
# Add -DDYNAMIC_CONNECTIONS to include dynamic max-connections patch
DEFINES		=  

# Any system specific libraries. If you get undefines at link time for names
# matching yy*, then you may need ``-ly -ll''.
LIBRARIES	=
# uncomment for SOLARIS but you may have to leave out the -lresolv
# LIBRARIES	= -lnsl -lsocket -lresolv	

## Add any `-L' flags here
LDFLAGS		= 

## Add any local includes '-I' needed here. (e.g. if using -DUSE_INN_INCLUDES)
# INCDIRS	= -I/usr/local/src/inn-1.4/include
INCDIRS		= 

## Uncomment the next line to build in larry wall's version of malloc. NOTE: 
## You need to build malloc.o with a -I pointing at the include subdirectory
## of your inn source tree. See INCDIRS above.
# MALLOC	= malloc.o

# Uncomment the next line if you don't have a inet_aton() in your
# libraries (Solaris is one such OS).
# EXTRA_SRC	= inet_addr.c

# Any extra compiler flags for warning generation.
WARNINGS	=
#WARNINGS	= -Wall $(ALL_GCC_WARNINGS) -DCHECK_FORMATS

## Uncomment the next three lines to use the GNU dmalloc library.
# DMALLOC		= -DUSE_DMALLOC
# DMALLOC_LIB	= -L/usr/local/gnu/lib -ldmalloc 
# DMALLOC_INCDIR	= -I/usr/local/gnu/include

LINTFLAGS	= -b -h -z $(DEFINES)


######################################################################
####           NOTHING BELOW HERE IS REALLY CONFIGURABLE          ####
######################################################################

.SUFFIXES:	.E .sh .pl

CPP.c		= $(CC) $(CFLAGS) $(CPPFLAGS) -E -I.
COMPILE.c	= $(CC) $(CFLAGS) $(CPPFLAGS) -c -I.
LINK.o		= $(CC) $(LDFLAGS)
LINK.c		= $(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS)
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

OBJS		= $(SRC:.c=.o) $(EXTRA_SRC:.c=.o) \
		  config_l.o config_y.o

INNFEED_BIN	= $(BINDIR)/innfeed

STARTINNFEED	= $(BINDIR)/startinnfeed


ALL_INSTALLED	= $(BINDIR)/innfeed $(BINDIR)/startinnfeed \
		  $(BINDIR)/procbatch $(BINDIR)/convertconfig

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

CFLAGS		= $(OSFLAGS) $(DBFLAGS) $(WARNINGS)
CPPFLAGS	= $(INCDIRS) $(DMALLOC_INCDIR) $(DMALLOC) $(DEFINES)

LOADLIBES	= $(DMALLOC_LIB) $(LIBRARIES)

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

install: $(ALL_INSTALLED) $(MAN_INSTALLED)

innfeed: $(OBJS) version.o $(MALLOC)
	$(LINK.c) -o $@ $(OBJS) version.o $(MALLOC) $(LOADLIBES)

SAVEOLD		= rm -f $@.orig ; mv $@ $@.orig
COPYFILE	= $(INSTALL) $? $@

$(INNFEED_BIN): innfeed
	$(SAVEOLD)
	$(COPYFILE)

$(BINDIR)/procbatch: procbatch
	$(SAVEOLD)
	$(COPYFILE)

$(BINDIR)/convertconfig: convertconfig
	$(SAVEOLD)
	$(COPYFILE)

$(BINDIR)/startinnfeed: startinnfeed
	$(SAVEOLD)
	$(COPYFILE)
	chown root.news $@
	chmod 04755 $@

objs:	$(OBJS)

tags: $(SRC) $(INCS)
	$(TAG) $(SRC) $(INCS)

clean::
	$(RM) version.c *.E *.o *.a *.core makedepend tags TAGS core \#* *~ 
	$(RM) Makefile.BAK procbatch convertconfig
	$(RM) innfeed startinnfeed uio_maxiov listing.ps logfile lint.log

rcsclean::
	rcsclean -u

rcscoall::
	-co -q -u RCS/*,v

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

malloc.o: malloc.c
	$(COMPILE.c) -Ddebug $< $(OUTPUT_OPTION)

# FIXYACC	= -e '/^.line [0-9][0-9]* "y.tab.c"[ \t]*$$/d' \
# 	  -e '/^.line [0-9][0-9]* "configfile.y"[ \t]*$$/d'
# FIXLEX	= -e '/^.line [0-9][0-9]* "lex.yy.c"[ \t]*$$/d' \
# 	  -e '/^.line [0-9][0-9]* "configfile.l"/d'

config_y.c config_y.h: configfile.y
	$(YACC) $?
	mv y.tab.h config_y.h
	mv y.tab.c config_y.c
#	sed $(FIXYACC) < y.tab.c > config_y.c
#	rm -f y.tab.c

config_l.c: configfile.l
	$(LEX) $?
	mv lex.yy.c config_l.c
# 	sed $(FIXLEX) < lex.yy.c > config_l.c
#	rm -f lex.yy.c

config_l.o: config_y.o config_l.c

tst:	config_y.c config_l.c
	gcc -DWANT_MAIN -o tst -g -Wall config_y.c config_l.c -ly -ll

# DO NOT DELETE THIS LINE -- make depend depends on it.

article.o: article.c article.h buffer.h config.h endpoint.h misc.h \
   msgs.h sysconfig.h
buffer.o: buffer.c buffer.h config.h misc.h sysconfig.h
connection.o: article.h buffer.h config.h configfile.h connection.c \
   connection.h endpoint.h host.h misc.h msgs.h sysconfig.h
endpoint.o: buffer.h config.h endpoint.c endpoint.h misc.h msgs.h \
   sysconfig.h
host.o: article.h buffer.h config.h configfile.h connection.h \
   endpoint.h host.c host.h innlistener.h misc.h msgs.h sysconfig.h \
   tape.h
innlistener.o: article.h buffer.h config.h configfile.h endpoint.h \
   host.h innlistener.c innlistener.h misc.h msgs.h sysconfig.h tape.h
main.o: article.h buffer.h config.h configfile.h connection.h \
   endpoint.h host.h innlistener.h main.c misc.h msgs.h sysconfig.h \
   tape.h
misc.o: config.h endpoint.h misc.c misc.h msgs.h sysconfig.h tape.h
tape.o: article.h config.h configfile.h endpoint.h misc.h msgs.h \
   sysconfig.h tape.c tape.h
