
L = -lz -lm $(MYSQLLIBS)
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a $(L)

O = affyPairsToSample normalizeSampleFile avgTranscriptomeExps maxTranscriptomeExps scaleSampleFiles averageZoomLevels \
	groupSamples

%: %.c 
	${CC} ${COPT} ${CFLAGS} -Wimplicit  -I../inc -I../../inc  -o $(HOME)/bin/$(MACHTYPE)/$@ $(addsuffix .c,$@) $(MYLIBS)

all : $(O)
