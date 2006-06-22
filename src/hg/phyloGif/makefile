include ../../inc/common.mk

L = $(MYSQLLIBS) -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

A = phyloGif

O = phyloGif.o

include ../../inc/cgi_build_rules.mk

compile: $O 
	${CC} $O ${MYLIBS} ${L}
	mv ${AOUT} $A${EXE}
	chmod a+rx $A${EXE}
	cp $A${EXE} ${HOME}/bin/${MACHTYPE}/

test: compile
	${HOME}/bin/${MACHTYPE}/phyloGif -phyloGif_tree=17way.nh > test.gif

clean::
	rm -f test.gif

