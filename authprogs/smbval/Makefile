# makefile for smblib
# Type make system, where system is ULTRIX, DU, DECOSF1, Solaris etc

include ../../Makefile.global

CFLAGS = $(GCFLAGS) -I../../include

INCLUDES = smblib.h smblib-priv.h

RFCNB = session.o rfcnb-util.o rfcnb-io.o

OBJS = smblib.o smblib-util.o file.o smb-errors.o exper.o smblib-api.o smbencrypt.o smbdes.o md4.o

VALIDATE = valid.o session.o rfcnb-util.o \
           rfcnb-io.o smblib-util.o smblib.o smbencrypt.o smbdes.o md4.o

.SUFFIXES: .c .o .h

dummy: all

smbvalid.a:	$(VALIDATE) 
	ar rc $@ $(VALIDATE)
	$(RANLIB) $@

all: smbvalid.a

.c.o:	$(INCLUDES)
	$(UPLIBCC) $(CFLAGS) -c $*.c

clean:
	rm -f *.o *.lo smbvalid.a
	rm -rf .libs
