kentSrc = ../../../..
include ${kentSrc}/inc/common.mk

pyprogs = $(shell file -F $$'\t' bin/* | awk '/Python script/{print $$1}')


all::

test::
	cd tests && ${MAKE} test

clean::
	cd tests && ${MAKE} clean

lint:

	python3 -m flake8 --color=never ${pyprogs} lib
