kentSrc = ../..
include ../../inc/common.mk

HG_INC += -I../../optimalLeaf
L += ${MYSQLLIBS}
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgapcgi.a ${MYLIBDIR}/jkhgap.a  ${MYLIBDIR}/jkweb.a ${MYLIBDIR}/optimalLeaf.a
COPT = -ggdb

A = hgTracks
RENDERA = hgRenderTracks

COMMONO = adjacencyTrack.o pubsTracks.o altGraphXTrack.o bamTrack.o barChartTrack.o baseTrack.o bedGraph.o \
	bedTrack.o  bigBedTrack.o bigRmskTrack.o bigWarn.o bigWigTrack.o \
	chainTrack.o cgapSageTrack.o chromGraphTrack.o config.o container.o contigTrack.o \
	coverageTrack.o cytoBandTrack.o cutterTrack.o decorator.o exportedDataHubs.o \
	expRatioTracks.o factorSource.o hgTracks.o interactTrack.o \
	gencodeTracks.o goldTrack.o gtexTracks.o gtexEqtlClusterTrack.o gvfTrack.o \
	halSnakeTrack.o lollyTrack.o longRangeTrack.o loraxTrack.o loweLabTracks.o lrgTrack.o \
	mafTrack.o makeItemsTrack.o menu.o mouseOver.o multiWig.o netTrack.o peptideAtlasTrack.o \
	pslTrack.o quickLift.o recTrackSets.o rmskJoinedTrack.o rmskTrack.o \
	sampleTracks.o searchTracks.o simpleTracks.o snake.o sortWig.o switchGear.o transMapTracks.o \
	wabaTrack.o wigTrack.o cds.o \
	wigMafTrack.o encode.o variation.o vcfTrack.o transcriptome.o\
	rnaFoldTrack.o retroGene.o hapmapTrack.o rnaPLFoldTrack.o\
	wiki.o wikiTrack.o imageV2.o extTools.o hicTrack.o heatmap.o

O = mainMain.o ${COMMONO} 

export RENDERO = renderMain.o ${COMMONO}

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

compile: compileMain ${RENDERA}

compileMain: $O
	${CC} ${CFLAGS} -o $A $O ${MYLIBS} ${L}
	cp extTools.ra ${CGI_BIN_DEST}

${RENDERA}: ${RENDERO}
	${MAKE} -f makefile.hgRenderTracks ${MAKECMDGOALS}

wigTrack.o:	wigCommon.h
bedGraph.o:	wigCommon.h

switchGear.o:	switchGear.h
cds.o:		cds.h
hgTracks.o:	cds.h
pslTrack.o:	cds.h

clean::
	rm -f ct_hgwdev_*.bed hgtIdeo_*.gif hgt_*.gif
	rm -f ${RENDERA} ${RENDERO}
