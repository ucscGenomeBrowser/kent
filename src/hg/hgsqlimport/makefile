include ../../inc/common.mk
HG_WARN = ${HG_WARN_ERR}

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

A = hgsqlimport

O = hgsqlimport.o

hgsqlimport: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/$A $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/$A${EXE}

clean::
	rm -f ${O}
