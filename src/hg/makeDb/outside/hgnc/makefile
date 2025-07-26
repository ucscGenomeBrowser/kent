kentSrc = ../../../..
include ${kentSrc}/inc/common.mk

all::

test::
	cd tests && ${MAKE} test

clean::
	cd tests && ${MAKE} clean

ignore = E129,E302,E305,E402,E501,E741,W503,W504
lint:
	python3 -m flake8 --color=never --ignore=${ignore} hgncToBigBed
