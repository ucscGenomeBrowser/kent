include ../../inc/common.mk

L = ${MYSQLLIBS} -lm 
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

A = hgTracks

O = $A.o altGraphXTrack.o chainTrack.o contigTrack.o \
	coverageTrack.o cytoBandTrack.o \
        expRatioTracks.o goldTrack.o loweLabTracks.o \
	mafTrack.o netTrack.o pslTrack.o rmskTrack.o \
	sampleTracks.o wabaTrack.o wigTrack.o

include ../../inc/cgi_build_rules.mk

mgsc: compile
	mv $A ${CGI_BIN}-mgsc/$A

compile: $O 
	${CC} $O ${MYLIBS} ${L}
	mv a.out $A
	chmod a+rx $A
