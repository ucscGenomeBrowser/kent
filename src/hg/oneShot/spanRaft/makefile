kentSrc = ../../..
include ../../../inc/common.mk

O = spanRaft.o 

spanRaft: $(O)
	cc -o ${BINDIR}/spanRaft $O ../../../lib/$(MACHTYPE)/jkweb.a

clean::
	rm -f ${O}
