include ../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

A = hgVisiGene

O = hgVisiGene.o captionElement.o printCaption.o probePage.o visiSearch.o

include ../../../inc/cgi_build_rules.mk

compile: $O 
	${CC} $O ${MYLIBS} ${L}
	mv ${AOUT} $A${EXE}

html: bigImage.html
	cp bigImage.html /usr/local/apache/htdocs/visiGene/

htmltest: bigImage.html
	cp bigImage.html /usr/local/apache/htdocs/visiGene/bigImageTest.html

testVisiSearch:  visiSearch.o testVisiSearch.o
	${CC} -o testVisiSearch visiSearch.o testVisiSearch.o ${MYLIBS} ${L}

