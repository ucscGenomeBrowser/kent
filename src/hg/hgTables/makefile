include ../../inc/common.mk

L = -lm ${MYSQLLIBS}
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

A = hgTables

O = $A.o asObj.o custom.o fields.o identifiers.o joining.o \
    mainPage.o ranges.o schema.o usage.o

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
	chmod a+rx $A
