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
	rm -rf ${CGI_BIN}-${USER}/{hgcData,lsSnpPdbChimera.py}
	rsync -ar --exclude=CVS/ --exclude=.svn/ --delete hgcData ../lib/lsSnpPdbChimera.py ${CGI_BIN}-${USER}/
	chmod -R a+rX ${CGI_BIN}-${USER}/{hgcData,lsSnpPdbChimera.py}

alpha::
	rm -fr ${CGI_BIN}/{hgcData,lsSnpPdbChimera.py}
	rsync -ar --exclude=CVS/ --exclude=.svn/ --delete hgcData ../lib/lsSnpPdbChimera.py ${CGI_BIN}/
	chmod -R a+rX,g+w ${CGI_BIN}/{hgcData,lsSnpPdbChimera.py}

beta::
	rm -fr ${CGI_BIN}-beta/{hgcData,lsSnpPdbChimera.py}
	rsync -ar --exclude=CVS/ --exclude=.svn/ --delete hgcData ../lib/lsSnpPdbChimera.py ${CGI_BIN}-beta/
	chmod -R a+rX,g+w ${CGI_BIN}-beta/{hgcData,lsSnpPdbChimera.py}
