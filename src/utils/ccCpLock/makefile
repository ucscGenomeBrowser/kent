include ../../inc/common.mk


O = ccCp.o 

ccCp: $O
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/ccCp $O ../../lib/$(MACHTYPE)/jkweb.a ${L} ${L}

#	${CC} ${COPT} ${CFLAGS} -o /projects/compbio/experiments/hg/bin/$(MACHTYPE)/ccCp $O ../../lib/$(MACHTYPE)/jkweb.a ${L} ${L}

clean::
	rm -f ${O}
