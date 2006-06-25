include ../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = vgGetText.o visiGene.o

DBS = mm7 mm8 hg17 hg18

compile: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/vgGetText $O ${MYLIBS} $L
	strip ${HOME}/bin/${MACHTYPE}/vgGetText

my: compile
	${MKDIR} ${CGI_BIN}-${USER}/visiGeneData
	vgGetText ${CGI_BIN}-${USER}/visiGeneData/visiGene.text ${DBS}
	ixIxx ${CGI_BIN}-${USER}/visiGeneData/visiGene.text ${CGI_BIN}-${USER}/visiGeneData/visiGene.ix ${CGI_BIN}-${USER}/visiGeneData/visiGene.ixx

alpha: compile
	${MKDIR} ${CGI_BIN}/visiGeneData
	vgGetText ${CGI_BIN}/visiGeneData/visiGene.text ${DBS}
	ixIxx ${CGI_BIN}/visiGeneData/visiGene.text ${CGI_BIN}/visiGeneData/visiGene.ix ${CGI_BIN}/visiGeneData/visiGene.ixx

clean:
	rm -f $O
