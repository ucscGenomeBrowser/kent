
SUBDIRS = src tests

all: ${SUBDIRS:%=%.suball}
%.suball:
	${MAKE} -C $* all

clean: ${SUBDIRS:%=%.subclean}
%.subclean:
	${MAKE} -C $* clean

#
# Install built files.  The etc directory is maintained using CVS, so
# any changes must be committed (and this is checked).
install: check-etc all
	@test -n "${PREFIX}" || (echo "must specify PREFIX= to install"; exit 1)
	mkdir -p ${PREFIX}/etc
	cp -fr bin ${PREFIX}
	cp -rf lib ${PREFIX}
	test -e ${PREFIX}/etc/CVS || cp -r etc/CVS ${PREFIX}/etc
	(cd ${PREFIX}/etc && cvs update -d)

check-etc:
	(cvs stat etc | fgrep Locally) || exit 0 && (echo "Error: etc/ files need committed" && exit 1)
