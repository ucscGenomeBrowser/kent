kentSrc = ../../..
include ../../../inc/common.mk

L = ${kentSrc}/htslib/libhts.a ${MYSQLLIBS}
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  ${MYLIBDIR}/jkpbap.a $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

A = gisaidSubj

O = gisaidSubj.o \
    demog.o \
    sequence.o

include ../../../inc/cgi_build_rules.mk

my::
	rsync -ar --exclude=CVS/ --exclude=.svn/ --delete gisaidSubjData ${CGI_BIN}-${USER}/
	chmod -R a+rX ${CGI_BIN}-${USER}/gisaidSubjData

alpha::
	rm -fr ${CGI_BIN}/gisaidSubjData
	rsync -ar --exclude=CVS/ --exclude=.svn/ --delete gisaidSubjData ${CGI_BIN}/
	chmod -R g+w ${CGI_BIN}/gisaidSubjData
	chmod -R a+rX ${CGI_BIN}/gisaidSubjData

beta::
	rm -fr ${CGI_BIN}-beta/gisaidSubjData
	rsync -ar --exclude=CVS/ --exclude=.svn/ --delete gisaidSubjData ${CGI_BIN}-beta/
	chmod -R g+w ${CGI_BIN}-beta/gisaidSubjData
	chmod -R a+rX ${CGI_BIN}-beta/gisaidSubjData

compile: $O 
	${CC} $O ${MYLIBS} ${L}
	mv ${AOUT} $A${EXE}

