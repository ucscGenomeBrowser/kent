include ../../inc/common.mk


L += -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = pslPretty.o

pslPretty: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${DESTDIR}${BINDIR}/pslPretty $O $(MYLIBS) $L

test:: testRna testDnax

testRna::
	${MKDIR} test/output
	pslPretty -long -axt test/input/rna.psl test/input/hCrea.geno test/input/hCrea.mrna test/output/rna.axt

testDnax::
	${MKDIR} test/output
	pslPretty -long -axt test/input/dnax.psl test/input/mCrea.geno test/input/hCrea.geno test/output/dnax.axt

testOld:: 
	${MKDIR} output
	pslPretty -long -axt input/test1.psl input/S1.lst input/S2.lst output/test1.axt

clean::
	rm -f ${O}
