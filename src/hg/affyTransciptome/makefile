
L = -lm $(MYSQLLIBS)
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a $(L)

O = affyPairsToSample normalizeSampleFile avgTranscriptomeExps maxTranscriptomeExps scaleSampleFiles averageZoomLevels \
	groupSamples

%: %.c 
	gcc -Wimplicit -ggdb  -I../inc -I../../inc  -o $(HOME)/bin/$(MACHTYPE)/$@ $(addsuffix .c,$@) $(MYLIBS)

all : $(O)
