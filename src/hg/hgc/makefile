include ../../inc/common.mk

L += -lm ${MYSQLLIBS} 
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

A = hgc

O = $A.o bamClick.o bigBedClick.o dbRIP.o ccdsClick.o cgapSageClick.o encodeClick.o expClick.o \
	gencodeClick.o gvfClick.o hgdpClick.o lowelab.o mafClick.o makeItemsClick.o mgcClick.o \
	parClick.o peakClusters.o regMotif.o retroClick.o rnaFoldClick.o \
	transMapClick.o txCdsInfo.o t2g.o vcfClick.o virusClick.o wiggleClick.o \
	wikiTrack.o variomeClick.o

include ../../inc/cgi_build_rules.mk

mgsc: compile
	mv $A ${CGI_BIN}-mgsc/$A

compile: $O 
	${CC} ${CFLAGS} $O ${F64} ${MYLIBS} ${L}
	mv ${AOUT} $A${EXE}

my::
	rm -rf ${CGI_BIN}-${USER}/hgcData ${CGI_BIN}-${USER}/lsSnpPdbChimera.py
	rsync -aO --exclude=CVS/ --exclude=.svn/ --delete hgcData ../lib/lsSnpPdbChimera.py ${CGI_BIN}-${USER}/
	chmod -R a+rX ${CGI_BIN}-${USER}/hgcData ${CGI_BIN}-${USER}/lsSnpPdbChimera.py

alpha::
	rm -fr ${CGI_BIN}/hgcData ${CGI_BIN}/lsSnpPdbChimera.py
	rsync -aO --exclude=CVS/ --exclude=.svn/ --delete hgcData ../lib/lsSnpPdbChimera.py ${CGI_BIN}/
	chmod -R a+rX,g+w ${CGI_BIN}/hgcData ${CGI_BIN}/lsSnpPdbChimera.py

beta::
	rm -fr ${CGI_BIN}-beta/hgcData ${CGI_BIN}-beta/lsSnpPdbChimera.py
	rsync -aO --exclude=CVS/ --exclude=.svn/ --delete hgcData ../lib/lsSnpPdbChimera.py ${CGI_BIN}-beta/
	chmod -R a+rX,g+w ${CGI_BIN}-beta/hgcData ${CGI_BIN}-beta/lsSnpPdbChimera.py
