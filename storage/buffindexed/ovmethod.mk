# This rule requires a compiler that supports -o with -c.  Since it's normally
# used by developers, that should be acceptable.
buffindexed/buffindexed_d.o: buffindexed/buffindexed.c
	$(CC) $(CFLAGS) -DDEBUG -c -o $@ buffindexed/buffindexed.c

buffindexed/buffindexed_d: buffindexed/buffindexed_d.o libstorage.$(EXTLIB) $(LIBHIST)
	$(LIBLD) $(LDFLAGS) -o $@ buffindexed/buffindexed_d.o \
	    buffindexed/shmem.o expire.o ov.o \
	    $(LIBSTORAGE) $(LIBHIST) $(LIBINN) $(STORAGE_LIBS) $(LIBS)

$(D)$(PATHBIN)/buffindexed_d: buffindexed/buffindexed_d
	$(LI_XPRI) $? $@
