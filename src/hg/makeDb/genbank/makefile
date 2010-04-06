GBROOT = .
include ${GBROOT}/common.mk

SUBDIRS = src tests

all: ${SUBDIRS:%=%.suball}
%.suball:
	${MAKE} -C $* all

clean: ${SUBDIRS:%=%.subclean}
	rm -rf bin lib

%.subclean:
	${MAKE} -C $* clean

install-server: all
	${MAKE} install PREFIX=/cluster/data/genbank

install-rr: ${GB_BINARCH}/hgsql ${GB_BINARCH}/hgLsSnpPdbLoad all
	${MAKE} install PREFIX=/genbank

etc-update:
	${MAKE} install-etc PREFIX=/cluster/data/genbank

# Install built files.  The etc directory is maintained using CVS, so
# any changes must be committed (and this is checked).
install: install-bin install-etc

install-bin: check-prefix all
	${MKDIR} ${PREFIX}
	rsync -r bin ${PREFIX}
	rsync -r lib ${PREFIX}

install-etc: check-prefix
	@echo checking if etc/ is committed
	@(cvs stat etc | fgrep Locally) || exit 0 && (echo "Error: etc/ files need committed" && exit 1)
	${MKDIR} ${PREFIX}/etc
	test -e ${PREFIX}/etc/CVS || cp -r etc/CVS ${PREFIX}/etc
	(cd ${PREFIX}/etc && cvs update -d)

check-prefix:
	@test -n "${PREFIX}" || (echo "must specify PREFIX= to install"; exit 1)

# need a copy of hgsql for rr
hgsqlSrcDir = ../../hgsql
${GB_BINARCH}/hgsql: ${hgsqlSrcDir}/hgsql.c
	${CC} ${CFLAGS} -o $@ $< $(LIBS)

# need a copy of hgLsSnpPdbLoad for rr
hgLsSnpPdbLoadSrcDir = ../outside/hgLsSnpPdbLoad
${GB_BINARCH}/hgLsSnpPdbLoad: ${hgLsSnpPdbLoadSrcDir}/hgLsSnpPdbLoad.c
	${CC} ${CFLAGS} -o $@ $< $(LIBS)

# build emacs tags for all files
.PHONY: etags-all
etags-all:
	find . -name '*.[ch]' | etags -
