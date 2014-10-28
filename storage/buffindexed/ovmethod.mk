# This rule requires a compiler that supports -o with -c.  Since it's normally
# used by developers, that should be acceptable.
buffindexed/buffindexed_d.$(EXTOBJ): buffindexed/buffindexed.c
	$(LIBCC) $(CFLAGS) -DBUFF_DEBUG -c -o $@ buffindexed/buffindexed.c

buffindexed/buffindexed_d: buffindexed/buffindexed_d.$(EXTOBJ) libstorage.$(EXTLIB) $(LIBHIST)
	$(LIBLD) $(LDFLAGS) -o $@ buffindexed/buffindexed_d.$(EXTOBJ) \
	    $(LIBSTORAGE) $(LIBHIST) $(LIBINN) $(STORAGE_LIBS) $(LIBS)
