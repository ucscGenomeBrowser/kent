include ../../inc/common.mk

L = -lm ${MYSQLLIBS}
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

A = hgTables

O = $A.o asObj.o bedList.o custom.o filterFields.o \
    gffOut.o identifiers.o intersect.o javascript.o \
    joining.o mainPage.o rangeHistogram.o schema.o \
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
	wget -O ${CGI_BIN}/galaAvail.tab http://gala.cse.psu.edu/cgi-bin/live_versions
	cp ${CGI_BIN}/galaAvail.tab ${CGI_BIN}-${USER}

beta::
	rm -f ${CGI_BIN}-beta/all.joiner
	cp ../makeDb/schema/all.joiner ${CGI_BIN}-beta
	chmod a+r ${CGI_BIN}-beta/all.joiner
	chmod g+w ${CGI_BIN}-beta/all.joiner
	wget -O ${CGI_BIN}-beta/galaAvail.tab http://gala.cse.psu.edu/cgi-bin/live_versions


compile: $O 
	${CC} $O ${MYLIBS} ${L} 
	mv a.out $A
