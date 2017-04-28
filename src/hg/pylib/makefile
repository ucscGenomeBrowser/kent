kentSrc = ../..
include ../../inc/common.mk

#cannot use common include rules, don't know how to overwrite :: targets
#include ../../inc/cgi_build_rules.mk

ifeq (${CGI_BIN_USER},)
    CGI_BIN_USER=${CGI_BIN}-${USER}
endif

ifeq (${CGI_BIN_BETA},)
    CGI_BIN_ALPHA=${CGI_BIN}-beta
endif

# Take the MySQLdb implementation of the current machine and copy it into the
# CGI-BIN/pylib/MySQLdb/ directory. This saves mirrors and our admins having to
# install this library. Hopefully the binary libraries work on Ubuntu and other
# distros as they work on our RedHat-based distro

my:: compile 
	mkdir -p ${CGI_BIN_USER}/pylib/
	rm -f ${CGI_BIN_USER}/pylib/hgLib.{py,pyc,pyo}
	cp -p hgLib.py ${CGI_BIN_USER}/pylib/hgLib.py
	cp -p hgLib.pyo ${CGI_BIN_USER}/pylib/hgLib.pyo
	
	mkdir -p ${CGI_BIN_USER}/pylib/MySQLdb
	cp -R `python2.7 -c 'import MySQLdb, os.path; print os.path.dirname(MySQLdb.__file__)'`/* ${CGI_BIN_USER}/pylib/MySQLdb/

beta:: compile
	mkdir -p ${CGI_BIN_BETA}/pylib/
	rm -f ${CGI_BIN_BETA}/pylib/hgLib.{py,pyc,pyo}
	cp -p hgLib.py ${CGI_BIN_BETA}/pylib/hgLib.py
	cp -p hgLib.pyo ${CGI_BIN_BETA}/pylib/hgLib.pyo
	
	mkdir -p ${CGI_BIN_BETA}/pylib/MySQLdb
	cp -R `python2.7 -c 'import MySQLdb, os.path; print os.path.dirname(MySQLdb.__file__)'`/* ${CGI_BIN_BETA}/pylib/MySQLdb/

alpha:: compile
	mkdir -p ${CGI_BIN}/pylib/
	rm -f ${CGI_BIN}/pylib/hgLib.{py,pyc,pyo}
	cp -p hgLib.py ${CGI_BIN}/pylib/hgLib.py
	cp -p hgLib.pyo ${CGI_BIN}/pylib/hgLib.pyo
	
	mkdir -p ${CGI_BIN}/pylib/MySQLdb
	cp -R `python2.7 -c 'import MySQLdb, os.path; print os.path.dirname(MySQLdb.__file__)'`/* ${CGI_BIN}/pylib/MySQLdb/

compile:
	# byte-compile and strip all python source code to .pyo files
	python2.7 -OO -m compileall ./

clean::
	rm -f *.pyc *.pyo

	# ? clean up the CGI-BIN directory, remove the MysqlDb library ?
	#rm -f ${CGI_BIN}/pylib/MySQLdb/*
	#rm -f ${CGI_BIN}/pylib/MySQLdb/constants/*
	#rmdir ${CGI_BIN}/pylib/MySQLdb/constants ${CGI_BIN}/pylib/MySQLdb/

lint::
	pylint -E hgLib.py

install::
