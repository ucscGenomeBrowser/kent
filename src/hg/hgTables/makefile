include ../../inc/common.mk

L = -lm ${MYSQLLIBS}
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

A = hgTables

T = tstbug.o

O = $A.o asObj.o bedList.o chromGraph.o compositeTrack.o custom.o filterFields.o \
    gffOut.o identifiers.o intersect.o correlate.o correlatePlot.o \
    joining.o maf.o mainPage.o rangeHistogram.o schema.o pal.o \
    seqOut.o sumStats.o usage.o wiggle.o wikiTrack.o galaxy.o userRegions.o

include ../../inc/cgi_build_rules.mk

my::
	rm -f ${CGI_BIN}-${USER}/all.joiner
	cp ../makeDb/schema/all.joiner ${CGI_BIN}-${USER}
	chmod a+r ${CGI_BIN}-${USER}/all.joiner
	chmod g+w ${CGI_BIN}-${USER}/all.joiner

alpha::
	rm -f ${CGI_BIN}/all.joiner
	cp ../makeDb/schema/all.joiner ${CGI_BIN}
	chmod a+r ${CGI_BIN}/all.joiner
	chmod g+w ${CGI_BIN}/all.joiner

beta::
	rm -f ${CGI_BIN}-beta/all.joiner
	cp ../makeDb/schema/all.joiner ${CGI_BIN}-beta
	chmod a+r ${CGI_BIN}-beta/all.joiner
	chmod g+w ${CGI_BIN}-beta/all.joiner


compile: $O 
	${CC} -g $O ${MYLIBS} ${L} 
	mv ${AOUT} $A${EXE}

#	all objects depend on hgTables.h

asObj.o:	hgTables.h
bedList.o:	hgTables.h correlate.h
custom.o:	hgTables.h
compositeTrack.o:	hgTables.h
filterFields.o:	hgTables.h
galaxy.o:	hgTables.h
gffOut.o:	hgTables.h
hgTables.o:	hgTables.h
identifiers.o:	hgTables.h
intersect.o:	hgTables.h
javascript.o:	hgTables.h
joining.o:	hgTables.h
maf.o:	hgTables.h
mainPage.o:	hgTables.h
rangeHistogram.o:	hgTables.h
schema.o:	hgTables.h
seqOut.o:	hgTables.h
sumStats.o:	hgTables.h
usage.o:	hgTables.h
wiggle.o:	hgTables.h correlate.h
correlate.o:	hgTables.h correlate.h
correlatePlot.o:	hgTables.h correlate.h

compiletest: $T
	${CC} -Wall $T ${MYLIBS} ${L}

compileLists: listFiles.o
	${CC} -Wall listFiles.o ${MYLIBS} ${L}

