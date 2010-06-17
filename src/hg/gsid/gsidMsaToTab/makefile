include ../../../inc/common.mk

L += $(MYSQLLIBS) -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = gsidMsaToTab.o

gsidMsaToTab: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/gsidMsaToTab $O $(MYLIBS) $L
#	${STRIP} ${BINDIR}/gsidMsaToTab${EXE}

test: test1 
%:	input/%
	${MKDIR} output/$@
	gsidMsaToTab input/$@ output/$@
	diff -x CVS -r expected/$@ output/$@
	rm -r output

clean::
	rm -f ${O}
