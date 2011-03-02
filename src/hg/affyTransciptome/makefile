include ../../inc/common.mk

L += -lz -lm $(MYSQLLIBS)
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a $(L)

O = affyPairsToSample normalizeSampleFile avgTranscriptomeExps maxTranscriptomeExps scaleSampleFiles averageZoomLevels \
	groupSamples

%: %.c 
	${CC} ${COPT} ${CFLAGS} -Wimplicit  -I../inc -I../../inc  -o ${BINDIR}/$@ $(addsuffix .c,$@) $(MYLIBS) ${L}

all : $(O)

clean::
	rm -f ${O}
