kentSrc = ../..
include ../../inc/common.mk

L += -lm $(SOCKETLIB)
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkOwnLib.a ${MYLIBDIR}/jkweb.a

O = webPcr.o

webPcr: $O ${MYLIBS}
	${CC} ${COPT} -o webPcr $O ${MYLIBS} $L
	${STRIP} webPcr${EXE}

clean:
	rm -f $O webPcr

installOsX: webPcr
	cp webPcr /Library/WebServer/CGI-Executables
	cp webPcr.cfg /Library/WebServer/CGI-Executables

installUcsc: webPcr
	cp webPcr /usr/local/apache/cgi-bin-$(USER)
	cp webPcr.cfg /usr/local/apache/cgi-bin-$(USER)

installAlpha: webPcr
	cp webPcr /usr/local/apache/cgi-bin
	cp webPcr.cfg /usr/local/apache/cgi-bin

