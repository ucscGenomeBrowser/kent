include ../../../inc/common.mk

L = $(MYSQLLIBS) -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = spToDb.o

spToDb: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/spToDb $O $(MYLIBS) $L
#	${STRIP} $(HOME)/bin/$(MACHTYPE)/spToDb${EXE}

test: test1 
%:	input/%
	${MKDIR} output/$@
	spToDb input/$@ output/$@
	diff -x CVS -r expected/$@ output/$@
	rm -r output

