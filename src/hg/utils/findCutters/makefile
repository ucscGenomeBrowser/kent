kentSrc = ../../..
include ../../../inc/common.mk

L += $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  ${MYLIBDIR}/jkhgapcgi.a $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

A = findCutters
O = ${A}.o

compile: $(O) $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/${A} $O $(MYLIBS) $L

clean:
	rm -f $(O)
