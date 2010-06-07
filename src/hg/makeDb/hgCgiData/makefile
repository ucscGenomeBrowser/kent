include ../../../inc/common.mk

my::
	rsync -ar --exclude=CVS/ --exclude=.svn/ --exclude=makefile --delete ../hgCgiData ${CGI_BIN}-${USER}/
	chmod -R a+rX ${CGI_BIN}-${USER}/hgCgiData

alpha::
	rm -fr ${CGI_BIN}/hgCgiData
	rsync -ar --exclude=CVS/ --exclude=.svn/ --exclude=makefile --delete ../hgCgiData ${CGI_BIN}/
	chmod -R g+w ${CGI_BIN}/hgCgiData
	chmod -R a+rX ${CGI_BIN}/hgCgiData

beta::
	rm -fr ${CGI_BIN}-beta/hgCgiData
	rsync -ar --exclude=CVS/ --exclude=.svn/ --exclude=makefile --delete ../hgCgiData ${CGI_BIN}-beta/
	chmod -R g+w ${CGI_BIN}-beta/hgCgiData
	chmod -R a+rX ${CGI_BIN}-beta/hgCgiData

compile::
	
install::
	rm -fr ${DESTDIR}${CGI_BIN}/hgCgiData
	rsync -ar --exclude=CVS/ --exclude=.svn/ --exclude=makefile --delete ../hgCgiData ${DESTDIR}${CGI_BIN}/
	chmod -R g+w ${DESTDIR}${CGI_BIN}/hgCgiData
	chmod -R a+rX ${DESTDIR}${CGI_BIN}/hgCgiData

clean::
	
