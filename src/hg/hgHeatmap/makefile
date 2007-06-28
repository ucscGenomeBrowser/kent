include ../../inc/common.mk

COPT = -g
L = $(MYSQLLIBS) -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

A = hgHeatmap

O = clickOnImage.o configure.o hgHeatmap.o mainPage.o usage.o

include ../../inc/cgi_build_rules.mk

compile: $O 
	${CC} $O ${MYLIBS} ${L}
	mv ${AOUT} $A${EXE}

