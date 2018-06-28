kentSrc = ../../..
include ../../../inc/common.mk

L = ${kentSrc}/htslib/libhts.a ${MYSQLLIBS}
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  ${MYLIBDIR}/jkpbap.a $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

A = gsidSubj3

O = gsidSubj3.o \
    clinical.o demog.o \
    vaccine.o sequence.o

include ../../../inc/cgi_build_rules.mk

my::
	rsync -ar --exclude=CVS/ --exclude=.svn/ --delete gsidSubj3Data ${CGI_BIN}-${USER}/
	chmod -R a+rX ${CGI_BIN}-${USER}/gsidSubj3Data

alpha::
	rm -fr ${CGI_BIN}/gsidSubj3Data
	rsync -ar --exclude=CVS/ --exclude=.svn/ --delete gsidSubj3Data ${CGI_BIN}/
	chmod -R g+w ${CGI_BIN}/gsidSubj3Data
	chmod -R a+rX ${CGI_BIN}/gsidSubj3Data

beta::
	rm -fr ${CGI_BIN}-beta/gsidSubj3Data
	rsync -ar --exclude=CVS/ --exclude=.svn/ --delete gsidSubj3Data ${CGI_BIN}-beta/
	chmod -R g+w ${CGI_BIN}-beta/gsidSubj3Data
	chmod -R a+rX ${CGI_BIN}-beta/gsidSubj3Data

compile: $O 
	${CC} $O ${MYLIBS} ${L}
	mv ${AOUT} $A${EXE}

