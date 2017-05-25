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

my:: compile 
	mkdir -p ${CGI_BIN_USER}/pyLib/
	rm -f ${CGI_BIN_USER}/pyLib/hgLib.{py,pyc,pyo}
	cp -p hgLib.py ${CGI_BIN_USER}/pyLib/hgLib.py
	cp -p hgLib.pyo ${CGI_BIN_USER}/pyLib/hgLib.pyo
	
	mkdir -p ${CGI_BIN_USER}/pyLib/MySQLdb
	#cp -R `python2.7 -c 'import MySQLdb, os.path; print os.path.dirname(MySQLdb.__file__)'`/* ${CGI_BIN_USER}/pyLib/MySQLdb/

beta:: compile
	mkdir -p ${CGI_BIN_BETA}/pyLib/
	rm -f ${CGI_BIN_BETA}/pyLib/hgLib.{py,pyc,pyo}
	cp -p hgLib.py ${CGI_BIN_BETA}/pyLib/hgLib.py
	cp -p hgLib.pyo ${CGI_BIN_BETA}/pyLib/hgLib.pyo
	
	mkdir -p ${CGI_BIN_BETA}/pyLib/MySQLdb
	#cp -R `python2.7 -c 'import MySQLdb, os.path; print os.path.dirname(MySQLdb.__file__)'`/* ${CGI_BIN_BETA}/pyLib/MySQLdb/

alpha:: compile
	mkdir -p ${CGI_BIN}/pyLib/
	rm -f ${CGI_BIN}/pyLib/hgLib.{py,pyc,pyo}
	cp -p hgLib.py ${CGI_BIN}/pyLib/hgLib.py
	cp -p hgLib.pyo ${CGI_BIN}/pyLib/hgLib.pyo
	
	mkdir -p ${CGI_BIN}/pyLib/MySQLdb
	#cp -R `python2.7 -c 'import MySQLdb, os.path; print os.path.dirname(MySQLdb.__file__)'`/* ${CGI_BIN}/pyLib/MySQLdb/

compile:
	# byte-compile and strip all python source code to .pyo files
	python2.7 -OO -m compileall ./

clean::
	rm -f *.pyc *.pyo

	# ? clean up the CGI-BIN directory, remove the MysqlDb library ?
	#rm -f ${CGI_BIN}/pyLib/MySQLdb/*
	#rm -f ${CGI_BIN}/pyLib/MySQLdb/constants/*
	#rmdir ${CGI_BIN}/pyLib/MySQLdb/constants ${CGI_BIN}/pyLib/MySQLdb/

lint::
	pylint -E hgLib.py

install::
