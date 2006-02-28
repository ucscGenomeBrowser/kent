include ../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a
DBS = mm7 hg17

O = vgGetText.o visiGene.o

my:: vgGetText
	${MKDIR} ${CGI_BIN}-${USER}/visiGeneData
	vgGetText ${CGI_BIN}-${USER}/visiGeneData/visiGene.text ${DBS}
	ixIxx ${CGI_BIN}-${USER}/visiGeneData/visiGene.text ${CGI_BIN}-${USER}/visiGeneData/visiGene.ix ${CGI_BIN}-${USER}/visiGeneData/visiGene.ixx

alpha:: vgGetText
	${MKDIR} ${CGI_BIN}/visiGeneData
	vgGetText ${CGI_BIN}/visiGeneData/visiGene.text ${DBS}
	ixIxx ${CGI_BIN}/visiGeneData/visiGene.text ${CGI_BIN}/visiGeneData/visiGene.ix ${CGI_BIN}/visiGeneData/visiGene.ixx

vgGetText: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/vgGetText $O ${MYLIBS} $L
	strip ${HOME}/bin/${MACHTYPE}/vgGetText

clean:
	rm -f $O
