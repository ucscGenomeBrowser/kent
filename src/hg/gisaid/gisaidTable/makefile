kentSrc = ../../..
include ../../../inc/common.mk

L = ${kentSrc}/htslib/libhts.a ${MYSQLLIBS} 
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a
A = gisaidTable

O = $A.o advFilter.o configure.o getSeq.o gisaidSubjInfo.o

include ../../../inc/cgi_build_rules.mk

my::
	rsync -ar --exclude=CVS/ --exclude=.svn/ --delete gisaidTableData ${CGI_BIN}-${USER}/
	chmod -R a+rX ${CGI_BIN}-${USER}/gisaidTableData

alpha::
	rm -fr ${CGI_BIN}/gisaidTableData
	rsync -ar --exclude=CVS/ --exclude=.svn/ --delete gisaidTableData ${CGI_BIN}/
	chmod -R g+w ${CGI_BIN}/gisaidTableData
	chmod -R a+rX ${CGI_BIN}/gisaidTableData

beta::
	rm -fr ${CGI_BIN}-beta/gisaidTableData
	rsync -ar --exclude=CVS/ --exclude=.svn/ --delete gisaidTableData ${CGI_BIN}-beta/
	chmod -R g+w ${CGI_BIN}-beta/gisaidTableData
	chmod -R a+rX ${CGI_BIN}-beta/gisaidTableData

compile: $O $(MYLIBS)
	${CC} ${COPT} -o $A $O $(MYLIBS) $L

test:
	$(CGI_BIN)-$(USER)/$A


