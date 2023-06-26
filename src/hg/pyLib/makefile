kentSrc = ../..
include ../../inc/common.mk

#cannot use common include rules, don't know how to overwrite :: targets
#include ../../inc/cgi_build_rules.mk

ifeq (${CGI_BIN_USER},)
    CGI_BIN_USER=${CGI_BIN}-${USER}
endif

ifeq (${CGI_BIN_BETA},)
    CGI_BIN_BETA=${CGI_BIN}-beta
endif

my:: compile 
	mkdir -p ${CGI_BIN_USER}/pyLib/
	rsync --whole-file --times --recursive --omit-dir-times --relative ./ ${CGI_BIN_USER}/pyLib/ --exclude=makefile --exclude=README.txt
	
beta:: compile
	mkdir -p ${CGI_BIN_BETA}/pyLib/
	rsync --whole-file --times --recursive --omit-dir-times --relative ./ ${CGI_BIN_BETA}/pyLib/ --exclude=makefile --exclude=README.txt
	
alpha:: compile
	mkdir -p ${CGI_BIN}/pyLib/
	rsync --whole-file --times --recursive --omit-dir-times --relative ./ ${CGI_BIN}/pyLib/ --exclude=makefile --exclude=README.txt
	
compile:
	# byte-compile and strip all python source code to .pyo files
	python3 -OO -m compileall ./

clean::
	rm -f *.pyc *.pyo
	rm -rf __pycache__ pymysql/__pycache__ pymysql/constants/__pycache__

lint::
	pylint -E hgLib3.py

install::
