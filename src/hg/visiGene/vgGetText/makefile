kentSrc = ../../..
include ../../../inc/common.mk

L = ${kentSrc}/htslib/libhts.a ${MYSQLLIBS}
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = vgGetText.o visiGene.o

DBS = mm7 mm8 mm9 mm10 hg17 hg18 hg19 hg38
ALPHA_DBS =  mm7 mm8 mm9 mm10 hg17 hg18 hg19 hg38

my:: compile
	${MKDIR} ${CGI_BIN}-${USER}/visiGeneData
	vgGetText ${CGI_BIN}-${USER}/visiGeneData/visiGene.text ${DBS}
	ixIxx ${CGI_BIN}-${USER}/visiGeneData/visiGene.text ${CGI_BIN}-${USER}/visiGeneData/visiGene.ix ${CGI_BIN}-${USER}/visiGeneData/visiGene.ixx

alpha:: compile
	${MKDIR} ${CGI_BIN}/visiGeneData
	vgGetText ${CGI_BIN}/visiGeneData/visiGene.text ${ALPHA_DBS}
	ixIxx ${CGI_BIN}/visiGeneData/visiGene.text ${CGI_BIN}/visiGeneData/visiGene.ix ${CGI_BIN}/visiGeneData/visiGene.ixx

beta:: compile
	${MKDIR} ${CGI_BIN}-beta/visiGeneData
	vgGetText ${CGI_BIN}-beta/visiGeneData/visiGene.text ${ALPHA_DBS}
	ixIxx ${CGI_BIN}-beta/visiGeneData/visiGene.text ${CGI_BIN}-beta/visiGeneData/visiGene.ix ${CGI_BIN}-beta/visiGeneData/visiGene.ixx

compile: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/vgGetText $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/vgGetText${EXE}

clean:
	rm -f $O
