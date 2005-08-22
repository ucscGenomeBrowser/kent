GBROOT = .
include ../../../inc/common.mk
include ${GBROOT}/common.mk

SUBDIRS = src tests

all: ${SUBDIRS:%=%.suball} jkprogs
%.suball:
	${MAKE} -C $* all

# special version of these programs needed for now
jkprogs: ${BINARCH}/blat \
	 ${BINARCH}/polyInfo \
	 ${BINARCH}/pslIntronsOnly

${BINARCH}/blat:
	@${MKDIR} ${BINARCH}
	HOME=`pwd` ${MAKE} -C ${KENT}/blat

${BINARCH}/polyInfo:
	@${MKDIR} ${BINARCH}
	HOME=`pwd` ${MAKE} -C ${KENT}/hg/geneBounds/polyInfo

${BINARCH}/pslIntronsOnly:
	@${MKDIR} ${BINARCH}
	HOME=`pwd` ${MAKE} -C ${KENT}/hg/pslIntronsOnly

clean: ${SUBDIRS:%=%.subclean}
%.subclean:
	${MAKE} -C $* clean

install-server:
	${MAKE} install PREFIX=/cluster/data/genbank

install-rr:
	${MAKE} install PREFIX=/genbank

etc-update:
	${MAKE} install-etc PREFIX=/cluster/data/genbank

# Install built files.  The etc directory is maintained using CVS, so
# any changes must be committed (and this is checked).
install: install-bin install-etc

install-bin: check-prefix
	${MKDIR} ${PREFIX}
	rsync -a bin ${PREFIX}
	rsync -a lib ${PREFIX}

install-etc: check-prefix
	@echo checking if etc/ is committed
	@(cvs stat etc | fgrep Locally) || exit 0 && (echo "Error: etc/ files need committed" && exit 1)
	${MKDIR} ${PREFIX}/etc
	test -e ${PREFIX}/etc/CVS || cp -r etc/CVS ${PREFIX}/etc
	(cd ${PREFIX}/etc && cvs update -d)
	cp -f src/ccds/createTables.sql src/ccds/createKeys.sql ${PREFIX}/etc

check-prefix:
	@test -n "${PREFIX}" || (echo "must specify PREFIX= to install"; exit 1)


# build emacs tags for all files
.PHONY: etags-all
etags-all:
	find . -name '*.[ch]' | etags -
