include ../../inc/common.mk

HG_WARN=-Wall

L = -lm ${MYSQLLIBS}
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

A = hgTables

O = $A.o asObj.o bedList.o custom.o filterFields.o \
    gffOut.o identifiers.o intersect.o javascript.o \
    joining.o maf.o mainPage.o rangeHistogram.o schema.o \
    seqOut.o sumStats.o usage.o wiggle.o gala.o

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
	wget --quiet -O ${CGI_BIN}/galaAvail.tab \
	  http://gala.cse.psu.edu/cgi-bin/live_versions

beta::
	rm -f ${CGI_BIN}-beta/all.joiner
	cp ../makeDb/schema/all.joiner ${CGI_BIN}-beta
	chmod a+r ${CGI_BIN}-beta/all.joiner
	chmod g+w ${CGI_BIN}-beta/all.joiner
	wget --quiet -O ${CGI_BIN}-beta/galaAvail.tab \
	  http://gala.cse.psu.edu/cgi-bin/live_versions


compile: $O 
	${CC} $O ${MYLIBS} ${L} 
	mv a.out $A

#	all objects depend on hgTables.h

asObj.o:	hgTables.h
bedList.o:	hgTables.h
custom.o:	hgTables.h
filterFields.o:	hgTables.h
gala.o:	hgTables.h
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
wiggle.o:	hgTables.h
