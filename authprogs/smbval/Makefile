##  $Id$

include ../../Makefile.global

top	      = ../..
CFLAGS	      = $(GCFLAGS)

ALL	      = smbvalid.a

SOURCES	      = rfcnb-io.c rfcnb-util.c session.c smbdes.c \
		smbencrypt.c smblib-util.c smblib.c valid.c

OBJECTS	      = $(SOURCES:.c=.o)

all: $(ALL)

warnings:
	$(MAKE) COPT='$(WARNINGS)' all

smbvalid.a: $(OBJECTS)
	ar rc $@ $(OBJECTS)
	$(RANLIB) $@

clobber clean distclean:
	rm -f *.o smbvalid.a

depend: Makefile $(SOURCES)
	$(MAKEDEPEND) $(CFLAGS) $(SOURCES)

# DO NOT DELETE THIS LINE -- make depend depends on it.
rfcnb-io.o: rfcnb-io.c ../../include/config.h \
 ../../include/inn/defines.h ../../include/clibrary.h rfcnb-priv.h \
 rfcnb-error.h rfcnb-common.h byteorder.h rfcnb-util.h rfcnb-io.h
rfcnb-util.o: rfcnb-util.c ../../include/config.h \
 ../../include/inn/defines.h ../../include/clibrary.h rfcnb-priv.h \
 rfcnb-error.h rfcnb-common.h byteorder.h rfcnb-util.h rfcnb-io.h
session.o: session.c ../../include/config.h \
 ../../include/inn/defines.h ../../include/clibrary.h rfcnb-priv.h \
 rfcnb-error.h rfcnb-common.h byteorder.h rfcnb-util.h
smbdes.o: smbdes.c
smbencrypt.o: smbencrypt.c ../../include/config.h \
 ../../include/inn/defines.h ../../include/clibrary.h smblib-priv.h \
 smblib-common.h byteorder.h
smblib-util.o: smblib-util.c ../../include/config.h \
 ../../include/inn/defines.h ../../include/clibrary.h smblib-priv.h \
 smblib-common.h byteorder.h rfcnb.h rfcnb-error.h rfcnb-common.h
smblib.o: smblib.c ../../include/config.h ../../include/inn/defines.h \
 ../../include/clibrary.h smblib-priv.h smblib-common.h byteorder.h \
 rfcnb.h rfcnb-error.h rfcnb-common.h
valid.o: valid.c ../../include/config.h ../../include/inn/defines.h \
 smblib-priv.h smblib-common.h byteorder.h valid.h
