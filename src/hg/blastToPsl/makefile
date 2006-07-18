include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

A = blastToPsl
O = $A.o 
BINDIR = $(HOME)/bin/$(MACHTYPE)

all: ${BINDIR}/blastToPsl ${BINDIR}/pslUniq

${BINDIR}/blastToPsl: blastToPsl
	cp -f blastToPsl $(BINDIR)
	${STRIP} $(BINDIR)/blastToPsl${EXE}

${BINDIR}/pslUniq: pslUniq
	cp -f pslUniq $(BINDIR)
	${STRIP} $(BINDIR)/pslUniq${EXE}

blastToPsl: ${O} ${MYLIBS}
	${CC} -o blastToPsl ${O} ${MYLIBS} ${L} 

my: compile scoreSort blast0To8 pslUniq pslChain
	cp -f blastToPsl $(BINDIR)
	${STRIP} $(BINDIR)/blastToPsl${EXE}
	mv scoreSort $(BINDIR)
	mv blast0To8 $(BINDIR)
	mv pslUniq $(BINDIR)
	mv pslChain $(BINDIR)

pslChain: pslChain.o
	${CC} pslChain.o ${F64} ${MYLIBS} ${L} 
	mv ${AOUT} pslChain
pslUniq: pslUniq.o
	${CC} pslUniq.o ${F64} ${MYLIBS} ${L} 
	mv ${AOUT} pslUniq
blast0To8: blast0To8.o
	${CC} blast0To8.o ${F64} ${MYLIBS} ${L} 
	mv ${AOUT} blast0To8

scoreSort: scoreSort.o
	${CC} scoreSort.o ${F64} ${MYLIBS} ${L} 
	mv ${AOUT} scoreSort

compile: $O 
	${CC} $O ${F64} ${MYLIBS} ${L} 
	mv ${AOUT} $A${EXE}
#	${STRIP} $A${EXE}
	chmod a+rx $A


debug: $O
	${CC} $O ${MYLIBS} ${L} 
	mv ${AOUT} $A${EXE}

test:	compile
	cd tests && ${MAKE} test

clean:
	rm -f *.o blastToPsl scoreSort blast0To8 \
		pslUniq pslChain
	cd tests && ${MAKE} clean

tags: 
	ctags *.h *.c ../lib/*.c ../inc/*.h ../../lib/*.c ../../inc/*.h
