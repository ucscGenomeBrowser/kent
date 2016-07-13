kentSrc = ../..
include ../../inc/common.mk

SUBDIRS = gisaidSample gisaidSubj gisaidTable

my:: $(SUBDIRS:%=%.my)
%.my:
	(cd $* && ${MAKE} my)

alpha:: $(SUBDIRS:%=%.alpha)
%.alpha:
	(cd $* && ${MAKE} alpha)

beta:: $(SUBDIRS:%=%.beta)
%.beta:
	(cd $* && ${MAKE} beta)

clean:: $(SUBDIRS:%=%.clean)
%.clean:
	(cd $* && ${MAKE} clean)
