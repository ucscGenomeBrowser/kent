kentSrc = ../..
include ../../inc/common.mk

L += $(MYSQLLIBS) -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgapcgi.a ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkOwnLib.a ${MYLIBDIR}/jkweb.a

COMMONO = phyloPlace.o parsimonyProto.o runUsher.o treeToAuspiceJson.o vcfFromFasta.o \
	  writeCustomTracks.o

A = hgPhyloPlace
O = hgPhyloPlace.o ${COMMONO}

include ../../inc/cgi_build_rules.mk

UTILA = phyloPlace
UTILO = phyloPlaceMain.o ${COMMONO}

compile: compileCgi ${UTILA}

compileCgi: $O
	${CC} ${CFLAGS} $O ${MYLIBS} ${L}
	mv ${AOUT} $A${EXE}

${UTILA}: ${UTILO}
	${CC} ${CFLAGS} -o ${UTILA} ${UTILO} ${MYLIBS} ${L}

clean::
	rm -f ${UTILO} ${UTILA}
