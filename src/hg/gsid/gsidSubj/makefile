kentSrc = ../../..
include ../../../inc/common.mk

L = ${kentSrc}/htslib/libhts.a ${MYSQLLIBS}
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  ${MYLIBDIR}/jkpbap.a $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

A = gsidSubj

O = gsidSubj.o \
    clinical.o demog.o \
    vaccine.o sequence.o

include ../../../inc/cgi_build_rules.mk

my::
	rsync -ar --exclude=CVS/ --exclude=.svn/ --delete gsidSubjData ${CGI_BIN}-${USER}/
	chmod -R a+rX ${CGI_BIN}-${USER}/gsidSubjData

alpha::
	rm -fr ${CGI_BIN}/gsidSubjData
	rsync -ar --exclude=CVS/ --exclude=.svn/ --delete gsidSubjData ${CGI_BIN}/
	chmod -R g+w ${CGI_BIN}/gsidSubjData
	chmod -R a+rX ${CGI_BIN}/gsidSubjData

beta::
	rm -fr ${CGI_BIN}-beta/gsidSubjData
	rsync -ar --exclude=CVS/ --exclude=.svn/ --delete gsidSubjData ${CGI_BIN}-beta/
	chmod -R g+w ${CGI_BIN}-beta/gsidSubjData
	chmod -R a+rX ${CGI_BIN}-beta/gsidSubjData

compile: $O 
	${CC} $O ${MYLIBS} ${L}
	mv ${AOUT} $A${EXE}

