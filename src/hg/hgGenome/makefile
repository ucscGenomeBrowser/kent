kentSrc = ../..
include ../../inc/common.mk

L += $(MYSQLLIBS) -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  $(MYLIBDIR)/jkhgapcgi.a $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

A = hgGenome

O = browseRegions.o clickOnImage.o configure.o correlate.o \
    hgGenome.o mainPage.o sortGenes.o upload.o usage.o \
    custom.o wiggle.o maf.o bedList.o chromGraph.o import.o

include ../../inc/cgi_build_rules.mk

compile: $O 
	${CC} $O ${MYLIBS} ${L}
	mv ${AOUT} $A${EXE}

backup:
	zip ~/hgGenome.zip *.c *.h makefile
