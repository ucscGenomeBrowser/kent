include ../inc/common.mk

L = -lm
MYLIBDIR = ../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkOwnLib.a ${MYLIBDIR}/jkweb.a

O = webBlat.o

webBlat: $O ${MYLIBS}
	${CC} ${COPT} -o webBlat $O ${MYLIBS} $L
	strip webBlat

install: webBlat
	cp webBlat /Library/WebServer/CGI-Executables
	cp webBlat.cfg /Library/WebServer/CGI-Executables

clean:
	rm -f $O

