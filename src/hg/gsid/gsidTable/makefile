kentSrc = ../../..
include ../../../inc/common.mk

L = ${kentSrc}/htslib/libhts.a ${MYSQLLIBS} 
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a
A = gsidTable

O = $A.o advFilter.o configure.o getSeq.o gsidSubjInfo.o

include ../../../inc/cgi_build_rules.mk

my::
	rsync -ar --exclude=CVS/ --exclude=.svn/ --delete gsidTableData ${CGI_BIN}-${USER}/
	chmod -R a+rX ${CGI_BIN}-${USER}/gsidTableData

alpha::
	rm -fr ${CGI_BIN}/gsidTableData
	rsync -ar --exclude=CVS/ --exclude=.svn/ --delete gsidTableData ${CGI_BIN}/
	chmod -R g+w ${CGI_BIN}/gsidTableData
	chmod -R a+rX ${CGI_BIN}/gsidTableData

beta::
	rm -fr ${CGI_BIN}-beta/gsidTableData
	rsync -ar --exclude=CVS/ --exclude=.svn/ --delete gsidTableData ${CGI_BIN}-beta/
	chmod -R g+w ${CGI_BIN}-beta/gsidTableData
	chmod -R a+rX ${CGI_BIN}-beta/gsidTableData

compile: $O $(MYLIBS)
	${CC} ${COPT} -o $A $O $(MYLIBS) $L

test:
	$(CGI_BIN)-$(USER)/$A
