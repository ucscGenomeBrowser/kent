
SNP_DIRS=snpLoad snpMask snpValid

all::
	@for D in ${SNP_DIRS} x; do \
	    if test "$$D" != "x" ; then \
		( cd $$D && echo $$D && ${MAKE} ) ;\
		x=$$? ; if [ $$x -ne 0 ]; then exit $$x ; fi \
	    fi ;\
	done

clean::
	@for D in ${SNP_DIRS} ; do \
	    ( cd "$$D" && echo cleaning $$D && ${MAKE} clean ) ; \
	done

compile::
	@for D in ${SNP_DIRS} ; do \
	    ( cd "$$D" && echo compile $$D && ${MAKE} compile ) ; \
	done
