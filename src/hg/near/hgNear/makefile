include ../../../inc/common.mk

L = $(MYSQLLIBS) -lm 
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a
A = hgNear

O = $A.o advFilter.o association.o configure.o examples.o expRatio.o getSeq.o \
	go.o gifLabel.o knownGene.o order.o pfam.o search.o userSettings.o

include ../../../inc/cgi_build_rules.mk

my::
	chmod -R a+r hgNearData
	chmod a+rx hgNearData
	find hgNearData -type d -exec /bin/chmod -f a+rx {} \;
	rsync -ar --delete hgNearData ${CGI_BIN}-${USER}/
	find ${CGI_BIN}-${USER} -type d -name CVS | xargs rm -fr

alpha::
	rm -fr ${CGI_BIN}/hgNearData
	rsync -ar --delete hgNearData ${CGI_BIN}/
	find ${CGI_BIN} -type d -name CVS | xargs rm -fr

beta::
	rm -fr ${CGI_BIN}-beta/hgNearData
	rsync -ar --delete hgNearData ${CGI_BIN}-beta/
	find ${CGI_BIN}-beta -type d -name CVS | xargs rm -fr

compile: $O $(MYLIBS)
	${CC} ${COPT} -o $A $O $(MYLIBS) $L
	chmod a+rx $A
