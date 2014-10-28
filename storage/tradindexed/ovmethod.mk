tradindexed/tdx-util.$(EXTOBJ): tradindexed/tdx-util.c
	$(LIBCC) $(CFLAGS) -c -o $@ tradindexed/tdx-util.c

tradindexed/tdx-util: tradindexed/tdx-util.$(EXTOBJ) libstorage.$(EXTLIB) $(LIBHIST)
	$(LIBLD) $(LDFLAGS) -o $@ tradindexed/tdx-util.$(EXTOBJ) \
	    $(LIBSTORAGE) $(LIBHIST) $(LIBINN) $(STORAGE_LIBS) $(LIBS)
