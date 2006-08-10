include ../../inc/common.mk

HG_WARN=-Wall

L = -lm ${MYSQLLIBS}
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

A = hgTables

T = tstbug.o

O = $A.o asObj.o bedList.o chromGraph.o compositeTrack.o custom.o filterFields.o \
    gffOut.o identifiers.o intersect.o correlate.o correlatePlot.o \
    javascript.o joining.o maf.o mainPage.o rangeHistogram.o schema.o \
    seqOut.o sumStats.o usage.o wiggle.o gala.o galaxy.o

include ../../inc/cgi_build_rules.mk

my::
	rm -f ${CGI_BIN}-${USER}/all.joiner
	cp ../makeDb/schema/all.joiner ${CGI_BIN}-${USER}
	chmod a+r ${CGI_BIN}-${USER}/all.joiner
	chmod g+w ${CGI_BIN}-${USER}/all.joiner
	rm -f ${CGI_BIN}-${USER}/galaAvail.tab
	cp galaAvail.tab ${CGI_BIN}-${USER}/galaAvail.tab
	chmod a+r ${CGI_BIN}-${USER}/galaAvail.tab
	chmod g+w ${CGI_BIN}-${USER}/galaAvail.tab

alpha::
	rm -f ${CGI_BIN}/all.joiner
	cp ../makeDb/schema/all.joiner ${CGI_BIN}
	chmod a+r ${CGI_BIN}/all.joiner
	chmod g+w ${CGI_BIN}/all.joiner
	rm -f ${CGI_BIN}/galaAvail.tab
	cp galaAvail.tab ${CGI_BIN}/galaAvail.tab
	chmod a+r ${CGI_BIN}/galaAvail.tab
	chmod g+w ${CGI_BIN}/galaAvail.tab
	# Original source of galaAvail.tab:
	#	wget --quiet -O ${CGI_BIN}/galaAvail.tab \
	#	  http://gala.cse.psu.edu/cgi-bin/live_versions

beta::
	rm -f ${CGI_BIN}-beta/all.joiner
	cp ../makeDb/schema/all.joiner ${CGI_BIN}-beta
	chmod a+r ${CGI_BIN}-beta/all.joiner
	chmod g+w ${CGI_BIN}-beta/all.joiner
	rm -f ${CGI_BIN}-beta/galaAvail.tab
	cp galaAvail.tab ${CGI_BIN}-beta/galaAvail.tab
	chmod a+r ${CGI_BIN}-beta/galaAvail.tab
	chmod g+w ${CGI_BIN}-beta/galaAvail.tab
	# Original source of galaAvail.tab:
	#	wget --quiet -O ${CGI_BIN}-beta/galaAvail.tab \
	#	  http://gala.cse.psu.edu/cgi-bin/live_versions


compile: $O 
	${CC} -g $O ${MYLIBS} ${L} 
	mv ${AOUT} $A${EXE}

#	all objects depend on hgTables.h

asObj.o:	hgTables.h
bedList.o:	hgTables.h correlate.h
custom.o:	hgTables.h
compositeTrack.o:	hgTables.h
filterFields.o:	hgTables.h
gala.o:	hgTables.h
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

