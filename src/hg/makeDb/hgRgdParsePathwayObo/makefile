kentSrc = ../../..
include ../../../inc/common.mk

L += $(MYSQLLIBS) -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgRgdParsePathwayObo.o

hgRgdParsePathwayObo: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/hgRgdParsePathwayObo $O $(MYLIBS) $L
#	${STRIP} ${BINDIR}/hgRgdParsePathwayObo${EXE}

test: test1 
%:	input/%
	${MKDIR} output/$@
	hgRgdParsePathwayObo input/$@ output/$@
	diff -x CVS -r expected/$@ output/$@
	rm -fr output

clean::
	rm -fr ${O} output
