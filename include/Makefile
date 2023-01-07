##  Currently just handles creation of the automatically generated header
##  files.  Eventually, rules for installing INN's header files will go
##  here.

include ../Makefile.global

top	      = ..

ALL	      = inn/portable-getaddrinfo.h inn/portable-getnameinfo.h \
		inn/portable-macros.h inn/portable-socket.h \
		inn/portable-stdbool.h inn/system.h inn/version.h

EXTRA	      = config.h inn/paths.h

all: $(ALL) $(EXTRA)

warnings: all

bootstrap:

clean:
	rm -f $(ALL)

clobber distclean maintclean: clean
	rm -f $(EXTRA)

depend:

profiled: all

$(EXTRA) $(FIXSCRIPT):
	@echo Run configure before running make.  See INSTALL for details.
	@exit 1


##  Build rules.

inn/portable-getaddrinfo.h: portable/getaddrinfo.h
	$(SED) 's/"portable\//"inn\/portable-/g' portable/getaddrinfo.h \
	  | $(SED) -e 's/HAVE_GETADDRINFO/INN_HAVE_GETADDRINFO/g' \
	           -e 's/config\.h/inn\/system.h/g' \
	           -e 's/portable\/socket\.h/inn\/portable-socket.h/g' \
	  > inn/portable-getaddrinfo.h

inn/portable-getnameinfo.h: portable/getnameinfo.h
	$(SED) 's/"portable\//"inn\/portable-/g' portable/getnameinfo.h \
	  | $(SED) -e 's/HAVE_GETNAMEINFO/INN_HAVE_GETNAMEINFO/g' \
	           -e 's/config\.h/inn\/system.h/g' \
	           -e 's/portable\/socket\.h/inn\/portable-socket.h/g' \
	  > inn/portable-getnameinfo.h

inn/portable-macros.h: portable/macros.h
	$(SED) 's/"portable\//"inn\/portable-/g' portable/macros.h \
	  > inn/portable-macros.h

inn/portable-socket.h: portable/socket.h
	$(SED) 's/"portable\//"inn\/portable-/g' portable/socket.h \
	  | $(SED) -e 's/HAVE_BROKEN_IN6_ARE_ADDR_EQUAL/INN_HAVE_BROKEN_IN6_ARE_ADDR_EQUAL/g' \
	           -e 's/HAVE_GETADDRINFO_ADDRCONFIG/INN_HAVE_GETADDRINFO_ADDRCONFIG/g' \
	           -e 's/HAVE_LONG_LONG_INT/INN_HAVE_LONG_LONG_INT/g' \
	           -e 's/HAVE_SOCKLEN_T/INN_HAVE_SOCKLEN_T/g' \
	           -e 's/HAVE_STRUCT_SOCKADDR_SA_LEN/INN_HAVE_STRUCT_SOCKADDR_SA_LEN/g' \
	           -e 's/HAVE_STRUCT_SOCKADDR_STORAGE/INN_HAVE_STRUCT_SOCKADDR_STORAGE/g' \
	           -e 's/HAVE_DECL_INET_ATON/INN_HAVE_DECL_INET_ATON/g' \
	           -e 's/HAVE_DECL_INET_NTOA/INN_HAVE_DECL_INET_NTOA/g' \
	           -e 's/HAVE_INET_ATON/INN_HAVE_INET_ATON/g' \
	           -e 's/HAVE_INET_NTOA/INN_HAVE_INET_NTOA/g' \
	           -e 's/HAVE_INET_NTOP/INN_HAVE_INET_NTOP/g' \
	           -e 's/config\.h/inn\/system.h/g' \
	  > inn/portable-socket.h

inn/portable-stdbool.h: portable/stdbool.h
	$(SED) 's/"portable\//"inn\/portable-/g' portable/stdbool.h \
	  | $(SED) -e 's/HAVE__BOOL/INN_HAVE__BOOL/g' \
	           -e 's/HAVE_STDBOOL_H/INN_HAVE_STDBOOL_H/g' \
	           -e 's/config\.h/inn\/system.h/g' \
	  > inn/portable-stdbool.h

inn/system.h: config.h $(top)/support/mksystem
	$(SHELL) $(top)/support/mksystem $(AWK) config.h > $@

inn/version.h: $(top)/support/mkversion $(top)/Makefile.global
	$(SHELL) $(top)/support/mkversion '$(VERSION)' '$(VERSION_EXTRA)' > $@


##  Installation rules.

install:
	$(top)/support/install-sh $(OWNER) -m 0755 -d $D$(PATHINCLUDE)/inn
	for F in inn/*.h ; do \
	    $(CP_RPUB) $$F $D$(PATHINCLUDE)/$$F ; \
	done
