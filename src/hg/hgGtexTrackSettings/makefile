kentSrc = ../..
include ../../inc/common.mk

HG_WARN=-Wall

L += -lm ${MYSQLLIBS}
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

A = hgGtexTrackSettings

O = $A.o

include ../../inc/cgi_build_rules.mk

$A.html.h: $A.html
	sed -e 's/\\/\\\\/g; s/"/\\"/g; s/^/"/; s/$$/\\n"/;' $A.html > $A.html.h

${A}.c: $A.html.h

${A}.o: ${A}.c $A.html.h

clean::
	rm -f $A.html.h

compile:: $O
	${CC} $O ${MYLIBS} ${L}
	mv ${AOUT} $A${EXE}
