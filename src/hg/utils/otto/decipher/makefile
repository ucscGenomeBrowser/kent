SUBDIRS = 

all: ${SUBDIRS:%=%.suball}

%.suball:
	${MAKE} -C $* all

FILES = decipher.tables buildDecipher validateDecipher.sh checkDecipher.sh decipherWrapper.sh login.perl ../../../lib/decipherRaw.sql ../../../lib/decipherSnvsRaw.sql processDecipher.py

install: make-prefix installSub ${PREFIX}
	cp ${FILES} ${PREFIX}

installSub: ${SUBDIRS:%=%.install}

%.install:
	${MAKE} -C $* install

validate: check-prefix
	$(foreach i, $(FILES), echo $(i); diff ${PREFIX}/$(i) $(i);)

make-prefix: check-prefix
	mkdir -p ${PREFIX}

check-prefix:
	@test -n "${PREFIX}" || (echo "must specify PREFIX= to install"; exit 1)
