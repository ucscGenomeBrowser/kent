include ../../inc/common.mk

L = $(MYSQLLIBS) -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

A = phyloGif

O = ${A}.o

include ../../inc/cgi_build_rules.mk

my:: compile
	if [ -d "${BINDIR}" ]; then \
	    cp ${CGI_BIN}-${USER}/$A ${BINDIR}/; \
	fi

compile: $O
	${CC} $O ${MYLIBS} ${L}
	mv ${AOUT} $A${EXE}
	chmod a+rx $A${EXE}

test: compile
	${BINDIR}/phyloGif -phyloGif_tree=17way.nh > test.gif
	file test.gif

testWeb: compile
	wget -O testWeb.gif 'http://hgwdev.cse.ucsc.edu/cgi-bin/phyloGif?phyloGif_tree=(A:0.1,B:0.1);'
	file testWeb.gif

clean::
	rm -f test.gif testWeb.gif

