include ../../inc/common.mk

L = -lm ${MYSQLLIBS}
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

A = hgTables

O = $A.o asObj.o bedList.o custom.o filterFields.o \
    gffOut.o identifiers.o intersect.o javascript.o \
    joining.o mainPage.o rangeHistogram.o schema.o \
    seqOut.o sumStats.o usage.o wiggle.o

include ../../inc/cgi_build_rules.mk

my::
	cp ../makeDb/schema/all.joiner ${CGI_BIN}-${USER}
	chmod a+r ${CGI_BIN}-${USER}/all.joiner
	chmod g+w ${CGI_BIN}-${USER}/all.joiner

alpha::
	cp ../makeDb/schema/all.joiner ${CGI_BIN}
	chmod a+r ${CGI_BIN}/all.joiner
	chmod g+w ${CGI_BIN}/all.joiner

beta::
	cp ../makeDb/schema/all.joiner ${CGI_BIN}-beta
	chmod a+r ${CGI_BIN}-beta/all.joiner
	chmod g+w ${CGI_BIN}-beta/all.joiner


compile: $O 
	${CC} $O ${MYLIBS} ${L} 
	mv a.out $A
