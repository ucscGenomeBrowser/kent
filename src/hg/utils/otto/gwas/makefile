SUBDIRS = 

all: ${SUBDIRS:%=%.suball}

FILES = gwasWrapper.sh checkGwas.sh validateGwas.sh perlParser.pl expectedColumns.txt

%.suball:
	${MAKE} -C $* all

install: check-prefix installSub
	cut -f 1-4 /cluster/data/hg18/bed/snp130/snp130.bed | sort -k4,4 > ${PREFIX}/snp130Coords.bed
	zcat /cluster/data/hg19/bed/snp151/snp151.bed.gz | cut -f 1-4,6,8,18,21-24   | sort -T ${PREFIX}  -k4,4  > ${PREFIX}/snp151Coords.bed
	zcat /cluster/data/hg38/bed/snp151/snp151.bed.gz | cut -f 1-4,6,8,18,21-24   | sort -T ${PREFIX}  -k4,4  > ${PREFIX}/hg38.snp151Coords.bed
	cp gwasWrapper.sh checkGwas.sh validateGwas.sh perlParser.pl expectedColumns.txt ${PREFIX}

installSub: ${SUBDIRS:%=%.install}

%.install:
	${MAKE} -C $* install

validate: check-prefix
	$(foreach i, $(FILES), echo $(i); diff ${PREFIX}/$(i) $(i);)

make-prefix: check-prefix
	mkdir -p ${PREFIX}

check-prefix:
	@test -n "${PREFIX}" || (echo "must specify PREFIX= to install"; exit 1)
