include ../../inc/common.mk

O = ccCp.o 

ccCp: $O
	${CC} ${COPT} ${CFLAGS} ${F64} -o ${BINDIR}/ccCp $O ../../lib/$(MACHTYPE)/jkweb.a 

#	${CC} ${COPT} ${CFLAGS} ${F64} -o /projects/compbio/experiments/hg/bin/$(MACHTYPE)/ccCp $O ../../lib/$(MACHTYPE)/jkweb.a 

clean::
	rm -f ${O}
