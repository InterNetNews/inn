tradindexed/tdx-util.o: tradindexed/tdx-util.c
	$(CC) $(CFLAGS) -c -o $@ tradindexed/tdx-util.c

tradindexed/tdx-util: tradindexed/tdx-util.o libstorage.$(EXTLIB) $(LIBHIST)
	$(LIBLD) $(LDFLAGS) -o $@ tradindexed/tdx-util.o \
	    $(LIBSTORAGE) $(LIBHIST) $(LIBINN) $(STORAGE_LIBS) $(LIBS)

$(D)$(PATHBIN)/tdx-util: tradindexed/tdx-util
	$(LI_XPRI) $? $@
