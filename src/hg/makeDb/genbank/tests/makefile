
SUBDIRS = index subset

all: ${SUBDIRS:%=%.suball}
%.suball:
	${MAKE} -C $* all

clean: ${SUBDIRS:%=%.subclean}
%.subclean:
	${MAKE} -C $* clean
