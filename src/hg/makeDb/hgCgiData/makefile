include ../../../inc/common.mk

my::
	rm -fr ${CGI_BIN}-${USER}/hgCgiData
	rsync -aO --exclude=CVS/ --exclude=.svn/ --exclude=makefile --delete ../hgCgiData ${CGI_BIN}-${USER}/
	chmod -R g+w ${CGI_BIN}-${USER}/hgCgiData
	chmod -R a+rX ${CGI_BIN}-${USER}/hgCgiData

alpha::
	rm -fr ${CGI_BIN}/hgCgiData
	rsync -aO --exclude=CVS/ --exclude=.svn/ --exclude=makefile --delete ../hgCgiData ${CGI_BIN}/
	chmod -R g+w ${CGI_BIN}/hgCgiData
	chmod -R a+rX ${CGI_BIN}/hgCgiData

beta::
	rm -fr ${CGI_BIN}-beta/hgCgiData
	rsync -aO --exclude=CVS/ --exclude=.svn/ --exclude=makefile --delete ../hgCgiData ${CGI_BIN}-beta/
	chmod -R g+w ${CGI_BIN}-beta/hgCgiData
	chmod -R a+rX ${CGI_BIN}-beta/hgCgiData

compile::
	
install::
	rm -fr ${DESTDIR}${CGI_BIN}/hgCgiData
	rsync -aO --exclude=CVS/ --exclude=.svn/ --exclude=makefile --delete ../hgCgiData ${DESTDIR}${CGI_BIN}/
	chmod -R g+w ${DESTDIR}${CGI_BIN}/hgCgiData
	chmod -R a+rX ${DESTDIR}${CGI_BIN}/hgCgiData

clean::
	
