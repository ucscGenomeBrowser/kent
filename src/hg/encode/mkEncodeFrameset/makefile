include ../../../inc/common.mk

A = mkEncodeFrameset

my: compile
	mv $A ${DESTDIR}${CGI_BIN}-${USER}/$A

compile:
	cp $A.pl $A

install: compile
	mv $A ${DESTDIR}${CGI_BIN}/$A

alpha: install

clean:
	echo "nothing to clean here"
