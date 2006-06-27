include ../../inc/common.mk

L = $(MYSQLLIBS) -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

A = phyloGif

O = ${A}.o

include ../../inc/cgi_build_rules.mk

compile: phyloForm.h $O
	${CC} $O ${MYLIBS} ${L}
	mv ${AOUT} $A${EXE}
	chmod a+rx $A${EXE}
	cp $A${EXE} ${HOME}/bin/${MACHTYPE}/

phyloForm.h: phyloForm.html
	echo "char phyloForm[] =" > phyloForm.h
	sed -e 's/\"/\\\"/g' phyloForm.html | gawk '{ print "\"" $$0 "\\n\"" }' >> phyloForm.h
	echo ";" >> phyloForm.h
			

test: compile
	${HOME}/bin/${MACHTYPE}/phyloGif -phyloGif_tree=17way.nh > test.gif

clean::
	rm -f test.gif phyloForm.h

