include ../../../inc/common.mk

A = mkEncodeFrameset

my: compile
	rm -f ${DESTDIR}${CGI_BIN}-${USER}/$A
	mv $A ${DESTDIR}${CGI_BIN}-${USER}/$A

compile:
	cp $A.pl $A

install: compile
	rm -f ${DESTDIR}${CGI_BIN}/$A
	mv $A ${DESTDIR}${CGI_BIN}/$A

alpha: install

clean:
	echo "nothing to clean here"
