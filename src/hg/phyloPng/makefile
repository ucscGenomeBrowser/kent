kentSrc = ../..
include ../../inc/common.mk

L += $(MYSQLLIBS)
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

A = phyloPng

O = ${A}.o

include ../../inc/cgi_build_rules.mk

# phyloPng is both a utilitiy that can be run from the commandline, and a CGI.

my:: compile
	if [ -d "${BINDIR}" ] && [ ! ${CGI_BIN_DEST}/$A -ef ${DESTDIR}${BINDIR}/$A ] ; then \
	    cp ${CGI_BIN_DEST}/$A ${DESTDIR}${BINDIR}/; \
	fi

compile: $O
	${CC} $O ${MYLIBS} ${L}
	mv ${AOUT} $A${EXE}
	chmod a+rx $A${EXE}

test: compile
	${DESTDIR}${BINDIR}/phyloPng -phyloPng_tree=17way.nh > test.png
	file test.png

testWeb: compile
	wget -O testWeb.png 'http://hgwdev.soe.ucsc.edu/cgi-bin/phyloPng?phyloPng_tree=(A:0.1,B:0.1);'
	file testWeb.png

clean::
	rm -f test.png testWeb.png
