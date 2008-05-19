include ../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

A = hgTracks

O = $A.o altGraphXTrack.o bedGraph.o chainTrack.o \
	cgapSageTrack.o chromGraphTrack.o config.o contigTrack.o \
	coverageTrack.o cytoBandTrack.o cutterTrack.o \
        expRatioTracks.o factorSource.o goldTrack.o loweLabTracks.o \
	mafTrack.o netTrack.o pslTrack.o rmskTrack.o \
	sampleTracks.o simpleTracks.o switchGear.o transMapTracks.o \
	wabaTrack.o wigTrack.o cds.o \
	wigMafTrack.o encode.o variation.o transcriptome.o\
	rnaFoldTrack.o retroGene.o hapmapTrack.o rnaPLFoldTrack.o\
	wiki.o wikiTrack.o

include ../../inc/cgi_build_rules.mk

ifeq (${GBROWSE}, 1)
  GBROWSE_D=-DGBROWSE
else
  GBROWSE_D=
endif

%.o: %.c
	${CC} ${COPT} ${CFLAGS} ${GBROWSE_D} ${HG_DEFS} ${HG_WARN} ${HG_INC} ${XINC} -o $@ -c $<

mgsc: strip
	mv $A ${CGI_BIN}/$A

compile: $O 
	${CC} -o $A $O ${MYLIBS} ${L}

wigTrack.o:	wigCommon.h
bedGraph.o:	wigCommon.h

switchGear.o:	switchGear.h
cds.o:		cds.h
hgTracks.o:	cds.h
pslTrack.o:	cds.h

clean::
	rm -f ct_hgwdev_*.bed hgtIdeo_*.gif hgt_*.gif

