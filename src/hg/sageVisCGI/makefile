include ../../inc/common.mk
HG_WARN=-Wall

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

A = sageVisCGI

O = gnuplot_i.o gnuPlot2D.o rosChr22Dat.o sageVisCGI.o

include ../../inc/cgi_build_rules.mk

mgsc: strip
	mv $A ${CGI_BIN}/$A

compile: $O 
	${CC} -o $A $O ${MYLIBS} ${L}
