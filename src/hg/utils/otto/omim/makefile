SUBDIRS = doOmimGene2 doOmimGeneSymbols doOmimLocation

all: ${SUBDIRS:%=%.suball}

%.suball:
	${MAKE} -C $* all

# any changes must be committed (and this is checked).
install: check-prefix installSub
	cp omimWrapper.sh omim.tables checkOmim.sh validateOmim.sh parseGeneMap.pl omimUploadWrapper.sh checkOmimUpload.sh doOmimPhenotypeNew buildOmimTracks.sh flagOmimGene.py ${PREFIX}
	#cp ../../../lib/{omimGeneMap,omimGeneMap2,omim2gene,omimGeneSymbol,omimPhenotype,omimAv}.sql /hive/data/outside/otto/omim/sql/

installSub: ${SUBDIRS:%=%.install}

%.install:
	${MAKE} -C $* install

make-prefix: check-prefix
	mkdir -p ${PREFIX}

check-prefix:
	@test -n "${PREFIX}" || (echo "must specify PREFIX= to install"; exit 1)

clean:
	rm -f doOmimGene2/doOmimGene2
	rm -f doOmimGeneSymbols/doOmimGeneSymbols
	rm -f doOmimLocation/doOmimLocation
