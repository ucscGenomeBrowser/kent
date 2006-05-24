include ../../inc/common.mk
HG_WARN=-Wall

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

A = hgTracks

O = $A.o altGraphXTrack.o bedGraph.o chainTrack.o config.o contigTrack.o \
	coverageTrack.o cytoBandTrack.o cutterTrack.o \
        expRatioTracks.o goldTrack.o loweLabTracks.o \
	mafTrack.o netTrack.o pslTrack.o rmskTrack.o \
	sampleTracks.o wabaTrack.o wigTrack.o cds.o \
	wigMafTrack.o encode.o variation.o transcriptome.o\
	rnaFoldTrack.o retroGene.o	

include ../../inc/cgi_build_rules.mk

mgsc: strip
	mv $A ${CGI_BIN}/$A

compile: $O 
	${CC} -o $A $O ${MYLIBS} ${L}

wigTrack.o:	wigCommon.h
bedGraph.o:	wigCommon.h

clean::
	rm -f ct_hgwdev_*.bed hgtIdeo_*.gif hgt_*.gif

