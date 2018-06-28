kentSrc = ../../..
include ../../../inc/common.mk

L = ${kentSrc}/htslib/libhts.a ${MYSQLLIBS}
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  ${MYLIBDIR}/jkpbap.a $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

A = gisaidSample

O = gisaidSample.o \
    demog.o \
    sequence.o

include ../../../inc/cgi_build_rules.mk

my::
	rsync -ar --exclude=CVS/ --exclude=.svn/ --delete gisaidSampleData ${CGI_BIN}-${USER}/
	chmod -R a+rX ${CGI_BIN}-${USER}/gisaidSampleData

alpha::
	rm -fr ${CGI_BIN}/gisaidSampleData
	rsync -ar --exclude=CVS/ --exclude=.svn/ --delete gisaidSampleData ${CGI_BIN}/
	chmod -R g+w ${CGI_BIN}/gisaidSampleData
	chmod -R a+rX ${CGI_BIN}/gisaidSampleData

beta::
	rm -fr ${CGI_BIN}-beta/gisaidSampleData
	rsync -ar --exclude=CVS/ --exclude=.svn/ --delete gisaidSampleData ${CGI_BIN}-beta/
	chmod -R g+w ${CGI_BIN}-beta/gisaidSampleData
	chmod -R a+rX ${CGI_BIN}-beta/gisaidSampleData

compile: $O 
	${CC} $O ${MYLIBS} ${L}
	mv ${AOUT} $A${EXE}
