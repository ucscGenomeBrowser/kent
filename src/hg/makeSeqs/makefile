JKSRCDIR = /cluster/home/ctrople/kent/src
JKLIBDIR = $(JKSRCDIR)/lib/$(MACHTYPE)

.c.o:
	${CC} ${COPT} ${CFLAGS} -Wimplicit -I$(JKSRCDIR)/inc -I$(JKSRCDIR)/hg/inc -c $*.c

L = $(MYSQLLIBS) -lm 
MYLIBS =  $(JKLIBDIR)/jkhgap.a $(JKLIBDIR)/jkweb.a

O = makeSeqs.o

makeSeqs: $O 
	${CC} ${COPT} ${CFLAGS} $O $(MYLIBS) $(L) 
	mv a.out makeSeqs
	${STRIP} makeSeqs${EXE}
	chmod a+rx makeSeqs
	cp makeSeqs $(HOME)/bin/$(MACHTYPE)
