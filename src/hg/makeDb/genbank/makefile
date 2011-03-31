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

etc-update-server:
	${MAKE} install-etc PREFIX=/cluster/data/genbank
etc-update: etc-update-server # alternative name

install-rr: ${GB_BINARCH}/hgsql ${GB_BINARCH}/hgLsSnpPdbLoad ${GB_BINARCH}/mrnaToGene ${GB_BINARCH}/hgLoadGenePred all
	${MAKE} install PREFIX=/genbank

etc-update-rr:
	${MAKE} install-etc PREFIX=/genbank

# standard options to use for rsync
rsyncOpts = --times --recursive --executability --omit-dir-times --relative

# Install built files.  The etc directory is maintained using CVS, so
# any changes must be committed (and this is checked).
install: install-bin install-etc

install-bin: check-prefix all
	${MKDIR} ${PREFIX}
	rsync ${rsyncOpts} bin ${PREFIX}
	rsync ${rsyncOpts} lib ${PREFIX}

# ensures that gitFiles is in PATH, otherwise a very bogus rsync results
install-etc: check-prefix
	@which gitFiles >/dev/null
	${MKDIR} ${PREFIX}/etc
	umask 002; rsync ${rsyncOpts} $$(gitFiles etc) ${PREFIX}

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

# need a copy of mrnaToGene for rr
mrnaToGeneDir = ../../mrnaToGene
${GB_BINARCH}/mrnaToGene: ${mrnaToGeneDir}/mrnaToGene.c
	${CC} ${CFLAGS} -o $@ $< $(LIBS)

# need a copy of hgLoadGenePred for rr
hgLoadGenePredDir = ../hgLoadGenePred
${GB_BINARCH}/hgLoadGenePred: ${hgLoadGenePredDir}/hgLoadGenePred.c
	${CC} ${CFLAGS} -o $@ $< $(LIBS)

# build emacs tags for all files
.PHONY: etags-all
etags-all:
	find . -name '*.[ch]' | etags 
