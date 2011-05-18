include ../../../inc/common.mk

L += ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

A = hgVisiGene

O = hgVisiGene.o captionElement.o configPage.o printCaption.o probePage.o visiSearch.o

include ../../../inc/cgi_build_rules.mk

# be sure to make clean after changing this:
XINC=-DVISIGENE='"visiGene"'

compile: $O 
	${CC} $O ${MYLIBS} ${L}
	mv ${AOUT} $A${EXE}

testVisiSearch:  visiSearch.o testVisiSearch.o
	${CC} -o testVisiSearch visiSearch.o testVisiSearch.o ${MYLIBS} ${L}
