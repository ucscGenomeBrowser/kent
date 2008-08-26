include ../../inc/common.mk

L = -lm ${MYSQLLIBS} 
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

A = hgc

O = $A.o cgapSageClick.o encodeClick.o expClick.o mafClick.o regMotif.o wiggleClick.o retroClick.o \
	lowelab.o rnaFoldClick.o transMapClick.o txCdsInfo.o \
	ccdsClick.o mgcClick.o dbRIP.o wikiTrack.o

include ../../inc/cgi_build_rules.mk

mgsc: compile
	mv $A ${CGI_BIN}-mgsc/$A

compile: $O 
	${CC} ${CFLAGS} $O ${F64} ${MYLIBS} ${L}
	mv ${AOUT} $A${EXE}

my::
	rsync -ar --exclude=CVS/ --exclude=.svn/ --delete hgcData ${CGI_BIN}-${USER}/
	chmod -R a+rX ${CGI_BIN}-${USER}/hgcData

alpha::
	rm -fr ${CGI_BIN}/hgcData
	rsync -ar --exclude=CVS/ --exclude=.svn/ --delete hgcData ${CGI_BIN}/
	chmod -R g+w ${CGI_BIN}/hgcData
	chmod -R a+rX ${CGI_BIN}/hgcData

beta::
	rm -fr ${CGI_BIN}-beta/hgcData
	rsync -ar --exclude=CVS/ --exclude=.svn/ --delete hgGeneData ${CGI_BIN}-beta/
	chmod -R g+w ${CGI_BIN}-beta/hgcData
	chmod -R a+rX ${CGI_BIN}-beta/hgcData
