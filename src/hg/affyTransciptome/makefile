kentSrc = ../..
include ../../inc/common.mk

L += $(MYSQLLIBS)
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a $(L)

O = affyPairsToSample normalizeSampleFile avgTranscriptomeExps maxTranscriptomeExps scaleSampleFiles averageZoomLevels \
	groupSamples

%: %.c 
	${CC} ${COPT} ${CFLAGS} -Wimplicit  -I../inc -I../../inc  -o ${DESTDIR}${BINDIR}/$@ $(addsuffix .c,$@) $(MYLIBS) ${L}

all : $(O)

clean::
	rm -f ${O}
