include ../../inc/common.mk

L += ${MYSQLLIBS} -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

A = hgTracks

COMMONO = altGraphXTrack.o bamTrack.o bedGraph.o bedTrack.o bigBedTrack.o bigWarn.o bigWigTrack.o \
	chainTrack.o cgapSageTrack.o chromGraphTrack.o config.o container.o contigTrack.o \
	coverageTrack.o cytoBandTrack.o cutterTrack.o \
        expRatioTracks.o factorSource.o hgTracks.o goldTrack.o gvfTrack.o loweLabTracks.o \
	mafTrack.o makeItemsTrack.o multiWig.o netTrack.o pslTrack.o rmskTrack.o \
	sampleTracks.o searchTracks.o simpleTracks.o snakeTrack.o switchGear.o transMapTracks.o \
	wabaTrack.o wigTrack.o cds.o \
	wigMafTrack.o encode.o variation.o vcfTrack.o transcriptome.o\
	rnaFoldTrack.o retroGene.o hapmapTrack.o rnaPLFoldTrack.o\
	wiki.o wikiTrack.o imageV2.o

O = mainMain.o ${COMMONO} 

include ../../inc/cgi_build_rules.mk

ifeq (${GBROWSE}, 1)
  GBROWSE_D=-DGBROWSE
  A = gbrowseTest
  O = $A.o oneTrack.o \
      bedGraph.o cds.o chainTrack.o mafTrack.o netTrack.o pslTrack.o \
      simpleTracks.o wigMafTrack.o wigTrack.o imageV2.o
else
  GBROWSE_D=
endif

%.o: %.c
	${CC} ${COPT} ${CFLAGS} ${GBROWSE_D} ${LOWELAB_DEFS} ${HG_DEFS} ${HG_WARN} ${HG_INC} ${XINC} -o $@ -c $<

mgsc: strip
	mv $A ${CGI_BIN}/$A

RENDERO = renderMain.o ${COMMONO}
RENDERA = ./hgRenderTracks

# when it is time to deliver hgRenderTracks, remove the rm and echo below
compile: compileMain
	@rm -f ${RENDERA}
	@echo "run 'make hgRenderTracks' to obtain that binary"

compileMain: $O
	${CC} ${CFLAGS} -o $A $O ${MYLIBS} ${L}

${RENDERA}: ${RENDERO}
	${CC} ${CFLAGS} -o ${RENDERA} ${RENDERO} ${MYLIBS} ${L}

# install of RENDERA is not enabled yet.
#	When it is desired, would need three different targets:
#	my:: - for personal browser into ${CGI_BIN}-${USER}
#	alpha:: - for browser into ${CGI_BIN}
#	beta:: - for browser into ${CGI_BIN}-beta
#	install:: - into DESTDIR/CGI_BIN

wigTrack.o:	wigCommon.h
bedGraph.o:	wigCommon.h

switchGear.o:	switchGear.h
cds.o:		cds.h
hgTracks.o:	cds.h
pslTrack.o:	cds.h

clean::
	rm -f ct_hgwdev_*.bed hgtIdeo_*.gif hgt_*.gif
	rm -f ${RENDERA} ${RENDERO}
