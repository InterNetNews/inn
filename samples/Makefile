##  $Id$
##
##  All the actual installation work of any files in the samples directory
##  is done via the site directory, so that one can maintain one's news
##  configuration in the site directory and use make commands to update the
##  server automatically.  All this Makefile does is run fixscript on a few
##  files that don't need the full power of configure (and clean up after
##  them on make clean).

include ../Makefile.global

top	      = ..

ALL           = nnrpd_auth.pl nnrpd_access.pl \
                nnrpd_auth_wrapper.pl nnrpd_access_wrapper.pl

EXTRA         = inn.conf innreport.conf newsfeeds sasl.conf

all: $(ALL)

clean:
	rm -f $(ALL)

clobber distclean: clean
	rm -f $(EXTRA)

install:
depend:

profiled: all


##  Build rules.

FIX             = $(FIXSCRIPT)

nnrpd_auth.pl:			nnrpd_auth.pl.in           $(FIX) ; $(FIX) $@.in
nnrpd_access.pl:		nnrpd_access.pl.in         $(FIX) ; $(FIX) $@.in
nnrpd_auth_wrapper.pl:		nnrpd_auth_wrapper.pl.in   $(FIX) ; $(FIX) $@.in
nnrpd_access_wrapper.pl:	nnrpd_access_wrapper.pl.in $(FIX) ; $(FIX) $@.in
