include ../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

A = hgVisiGene

O = hgVisiGene.o captionElement.o configPage.o printCaption.o probePage.o visiSearch.o

include ../../../inc/cgi_build_rules.mk

CFLAGS=-DVISIGENE='"visiGene"'

my::
	cd ../vgGetText && ${MAKE}

alpha::
	cd ../vgGetText && ${MAKE} alpha

beta::
	cd ../vgGetText && ${MAKE} alpha

compile: $O 
	${CC} $O ${MYLIBS} ${L}
	mv ${AOUT} $A${EXE}

push: compile
	mv $A${EXE} ${CGI_BIN}-${USER}

html: bigImage.html
	cp bigImage.html /usr/local/apache/htdocs/

htmltest: bigImage.html
	cp bigImage.html /usr/local/apache/htdocs/bigImageTest.html

gif:
	cp dot_clear.gif /usr/local/apache/htdocs/images/

testVisiSearch:  visiSearch.o testVisiSearch.o
	${CC} -o testVisiSearch visiSearch.o testVisiSearch.o ${MYLIBS} ${L}

