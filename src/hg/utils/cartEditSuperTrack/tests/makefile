kentSrc = ../../../..
include ../../../../inc/common.mk

cartEditSuperTrack = ${DESTBINDIR}/cartEditSuperTrack

all:: test

test:: test1
	${MAKE} clean

test1: mkout
	${cartEditSuperTrack} input/test1.txt 1 output/cartEdit1.c
	cmp expected/cartEdit1.c output/cartEdit1.c

mkout:
	@mkdir -p output

clean::
	rm -rf output
