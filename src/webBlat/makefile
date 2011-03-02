include ../inc/common.mk

L += -lm $(SOCKETLIB)
MYLIBDIR = ../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkOwnLib.a ${MYLIBDIR}/jkweb.a

O = webBlat.o

webBlat: $O ${MYLIBS}
	${CC} ${COPT} -o webBlat $O ${MYLIBS} ${L}
	${STRIP} webBlat${EXE}

installOsX: webBlat
	cp webBlat /Library/WebServer/CGI-Executables
	cp webBlat.cfg /Library/WebServer/CGI-Executables

installUcsc: webBlat
	cp webBlat /usr/local/apache/cgi-bin-$(USER)
	cp webBlat.cfg /usr/local/apache/cgi-bin-$(USER)

installAlpha: webBlat
	cp webBlat /usr/local/apache/cgi-bin
	cp webBlat.cfg /usr/local/apache/cgi-bin

clean:
	rm -f $O webBlat

