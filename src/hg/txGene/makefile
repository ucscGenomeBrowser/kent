include ../../inc/common.mk

L += -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

all:
	@for D in ${DIRS} x; do \
	  if test "$$D" != "x" ; then \
	    ( cd $$D && echo $$D && ${MAKE}  ) ;\
	    x=$$? ; if [ $$x -ne 0 ]; then exit $$x ; fi \
	    fi ;\
	done

DIRS = \
	txGenbankData \
	txGeneAccession \
	txGeneAlias \
	txGeneAltProt \
	txGeneCanonical \
	txGeneCdsMap \
	txGeneColor \
	txGeneExplainUpdate1 \
	txGeneFromBed \
	txGeneProtAndRna \
	txGeneSeparateNoncoding \
	txGeneValidation \
	txGeneXref

clean:
	@for D in ${DIRS} x; do \
	  if test "$$D" != "x" ; then \
	    ( cd $$D && echo $$D && ${MAKE} clean ) ;\
	    x=$$? ; if [ $$x -ne 0 ]; then exit $$x ; fi \
	    fi ;\
	done
