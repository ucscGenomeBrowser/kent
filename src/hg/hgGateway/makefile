kentSrc = ../..
include ../../inc/common.mk

HG_WARN=-Wall

L = ${kentSrc}/htslib/libhts.a ${MYSQLLIBS}
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a ${MYSQLLIBS} ../../htslib/libhts.a

A = hgGateway

O = $A.o

include ../../inc/cgi_build_rules.mk

hgGateway.html.h: hgGateway.html
	sed -e 's/\\/\\\\/g; s/"/\\"/g; s/^/"/; s/$$/\\n"/;' hgGateway.html > hgGateway.html.h

${A}.c: hgGateway.html.h

${A}.o: ${A}.c hgGateway.html.h

clean::
	rm -f hgGateway.html.h

compile:: $O
	${CC} $O ${MYLIBS}
	mv ${AOUT} $A${EXE}
