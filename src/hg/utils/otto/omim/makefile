SUBDIRS = doOmimAv doOmimGene2 doOmimGeneSymbols doOmimLocation

all: ${SUBDIRS:%=%.suball}

%.suball:
	${MAKE} -C $* all

# any changes must be committed (and this is checked).
install: check-prefix installSub
	cp omimWrapper.sh omim.tables buildOmimTracks.csh checkOmim.sh validateOmim.sh parseGeneMap.pl omimUploadWrapper.sh checkOmimUpload.sh ${PREFIX}

installSub: ${SUBDIRS:%=%.install}

%.install:
	${MAKE} -C $* install

make-prefix: check-prefix
	mkdir -p ${PREFIX}

check-prefix:
	@test -n "${PREFIX}" || (echo "must specify PREFIX= to install"; exit 1)
