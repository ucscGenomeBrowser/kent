kentSrc = ../..
include ../../inc/common.mk

L += -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

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

all: ${DIRS:%=%_all}
%_all:
	${MAKE} -C $*

clean:: ${DIRS:%=%_clean}
%_clean:
	${MAKE} -C $* clean

compile: ${DIRS:%=%_compile}
%_compile:
	${MAKE} -C $* compile
