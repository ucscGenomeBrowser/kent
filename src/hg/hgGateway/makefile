kentSrc = ../..
include ../../inc/common.mk

HG_WARN=-Wall

L += -lm ${MYSQLLIBS}
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgapcgi.a ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

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
	${CC} $O ${MYLIBS} ${L}
	mv ${AOUT} $A${EXE}
