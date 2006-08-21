include ../../inc/common.mk

L = $(MYSQLLIBS) -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  ${MYLIBDIR}/jkpbap.a $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

A = hgSubj

O = hgSubj.o \
    clinical.o demog.o \
    vaccine.o sequence.o

include ../../inc/cgi_build_rules.mk

my::
	rsync -ar --exclude=CVS/ --delete hgSubjData ${CGI_BIN}-${USER}/
	chmod -R a+rX ${CGI_BIN}-${USER}/hgSubjData

alpha::
	rm -fr ${CGI_BIN}/hgSubjData
	rsync -ar --exclude=CVS/ --delete hgSubjData ${CGI_BIN}/
	chmod -R g+w ${CGI_BIN}/hgSubjData
	chmod -R a+rX ${CGI_BIN}/hgSubjData

beta::
	rm -fr ${CGI_BIN}-beta/hgSubjData
	rsync -ar --exclude=CVS/ --delete hgSubjData ${CGI_BIN}-beta/
	chmod -R g+w ${CGI_BIN}-beta/hgSubjData
	chmod -R a+rX ${CGI_BIN}-beta/hgSubjData

compile: $O 
	${CC} $O ${MYLIBS} ${L}
	mv ${AOUT} $A${EXE}

