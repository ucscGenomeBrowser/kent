include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkOwnLib.a ${MYLIBDIR}/jkweb.a

O = webPcr.o

webPcr: $O ${MYLIBS}
	${CC} ${COPT} -o webPcr $O ${MYLIBS} $L
	strip webPcr

clean:
	rm -f $O

install: webPcr
	cp webPcr /Library/WebServer/CGI-Executables
	cp webPcr.cfg /Library/WebServer/CGI-Executables

