include ../../../inc/common.mk

L += $(MYSQLLIBS) -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

A = mafSplitPos
O = ${A}.o

${A}: compile
	${STRIP} $(BINDIR)/${A}${EXE}

compile: $O ${MYLIBS}
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/${A} $O ${MYLIBS} ${L}

clean::
	rm -f ${O}
