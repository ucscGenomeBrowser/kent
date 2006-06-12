include ../../../inc/common.mk

L = $(MYSQLLIBS) -lm 
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a
A = hgNear

O = $A.o advFilter.o afiAli.o association.o configure.o \
	customColumn.o examples.o expRatio.o flyBase.o getSeq.o \
	go.o knownGene.o order.o pfam.o search.o userSettings.o

include ../../../inc/cgi_build_rules.mk

my::
	rsync -ar --exclude=CVS/ --delete hgNearData ${CGI_BIN}-${USER}/
	chmod -R a+rX ${CGI_BIN}-${USER}/hgNearData

alpha::
	rm -fr ${CGI_BIN}/hgNearData
	rsync -ar --exclude=CVS/ --delete hgNearData ${CGI_BIN}/
	chmod -R g+w ${CGI_BIN}/hgNearData
	chmod -R a+rX ${CGI_BIN}/hgNearData

beta::
	rm -fr ${CGI_BIN}-beta/hgNearData
	rsync -ar --exclude=CVS/ --delete hgNearData ${CGI_BIN}-beta/
	chmod -R g+w ${CGI_BIN}-beta/hgNearData
	chmod -R a+rX ${CGI_BIN}-beta/hgNearData

compile: $O $(MYLIBS)
	${CC} ${COPT} -o $A $O $(MYLIBS) $L

test:
	$(CGI_BIN)-$(USER)/hgNear

