tradindexed/tdx-util: tradindexed/tdx-util.o libstorage.$(EXTLIB) $(LIBHIST)
	$(LIBLD) $(LDFLAGS) -o $@ tradindexed/tdx-util.o \
	    $(LIBSTORAGE) $(LIBHIST) $(LIBINN) $(EXTSTORAGELIBS) $(LIBS)

$(D)$(PATHBIN)/tdx-util: tradindexed/tdx-util
	$(LI_XPRI) $? $@
