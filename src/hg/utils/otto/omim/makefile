SUBDIRS = doOmimGene2 doOmimGeneSymbols doOmimLocation
PREFIX = /hive/data/outside/otto/omim

all: ${SUBDIRS:%=%.suball}
FILES = omimWrapper.sh omim.tables checkOmim.sh validateOmim.sh parseGeneMap.pl omimUploadWrapper.sh checkOmimUpload.sh doOmimPhenotypeNew buildOmimTracks.sh flagOmimGene.py

%.suball:
	${MAKE} -C $* all

# any changes must be committed (and this is checked).
install: check-prefix installSub
	cp doOmimGene2.sh omimWrapper.sh omim.tables checkOmim.sh validateOmim.sh parseGeneMap.pl omimUploadWrapper.sh checkOmimUpload.sh doOmimPhenotypeNew buildOmimTracks.sh flagOmimGene.py ${PREFIX}
	cp ../../../lib/{omimGeneMap,omimGeneMap2,omim2gene,omimGeneSymbol,omimPhenotype,omimAv}.sql ${PREFIX}

installSub: ${SUBDIRS:%=%.install}

%.install:
	${MAKE} -C $* install

validate: check-prefix
	$(foreach i, $(FILES), echo $(i); diff ${PREFIX}/$(i) $(i);)

make-prefix: check-prefix
	mkdir -p ${PREFIX}
	mkdir -p ${PREFIX}/sql

check-prefix:
	@test -n "${PREFIX}" || (echo "must specify PREFIX= to install"; exit 1)

clean:
	rm -f doOmimGene2/doOmimGene2
	rm -f doOmimGeneSymbols/doOmimGeneSymbols
	rm -f doOmimLocation/doOmimLocation
