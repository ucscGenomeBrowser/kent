include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

A = blastToPsl
O = $A.o 
BINDIR = $(HOME)/bin/$(MACHTYPE)

all: ${BINDIR}/blastToPsl

${BINDIR}/blastToPsl: blastToPsl
	cp -f blastToPsl $(BINDIR)
	${STRIP} $(BINDIR)/blastToPsl${EXE}
blastToPsl: ${O} ${MYLIBS}
	${CC} -o blastToPsl ${O} ${MYLIBS} ${L} 

my: compile scoreSort blast0To8 pslUniq pslChain
	cp -f blastToPsl $(BINDIR)
	${STRIP} $(BINDIR)/blastToPsl${EXE}
	mv scoreSort $(BINDIR)
	mv blast0To8 $(BINDIR)
#	mv smashTab $(BINDIR)
	mv pslUniq $(BINDIR)
#	mv vegaBuildInfo $(BINDIR)
	mv pslChain $(BINDIR)

pslChain: pslChain.o
	${CC} -static pslChain.o ${F64} ${MYLIBS} ${L} 
	mv a.out pslChain
vegaBuildInfo: vegaBuildInfo.o
	${CC} vegaBuildInfo.o ${F64} ${MYLIBS} ${L} 
	mv a.out vegaBuildInfo
pslUniq: pslUniq.o
	${CC} pslUniq.o ${F64} ${MYLIBS} ${L} 
	mv a.out pslUniq
blast0ToPsl: blast0ToPsl.o
	${CC} -static blast0ToPsl.o ${F64} ${MYLIBS} ${L} 
	mv a.out blast0ToPsl
tryToPsl: tryToPsl.o
	${CC} tryToPsl.o ${F64} ${MYLIBS} ${L} 
	mv a.out tryToPsl
blastLink: blastLink.o
	${CC} blastLink.o ${F64} ${MYLIBS} ${L} 
	mv a.out blastLink
smashTab: smashTab.o
	${CC} smashTab.o ${F64} ${MYLIBS} ${L} 
	mv a.out smashTab
blast0To8: blast0To8.o
	${CC} blast0To8.o ${F64} ${MYLIBS} ${L} 
	mv a.out blast0To8

scoreSort: scoreSort.o
	${CC} scoreSort.o ${F64} ${MYLIBS} ${L} 
	mv a.out scoreSort

compile: $O 
	${CC} $O ${F64} ${MYLIBS} ${L} 
	mv a.out $A
#	${STRIP} $A${EXE}
	chmod a+rx $A


debug: $O
	${CC} $O ${MYLIBS} ${L} 
	mv a.out $A

test:	compile
	cd tests && ${MAKE} test

clean:
	rm -f *.o blastToPsl scoreSort blast0To8 smashTab blast0ToPsl \
		pslUniq vegaBuildInfo pslChain
	cd tests && ${MAKE} clean

tags: 
	ctags *.h *.c ../lib/*.c ../inc/*.h ../../lib/*.c ../../inc/*.h
