SUBDIRS = 

all: ${SUBDIRS:%=%.suball}

%.suball:
	${MAKE} -C $* all

FILES= decipher.tables buildDecipher validateDecipher.sh checkDecipher.sh decipherWrapper.sh login.perl ../../../lib/decipherRaw.sql ../../../lib/decipherSnvsRaw.sql

install: make-prefix installSub ${PREFIX}
	cp ${FILES} ${PREFIX}

installSub: ${SUBDIRS:%=%.install}

%.install:
	${MAKE} -C $* install

validate: check-prefix
	for i in ${FILES} ;do diff \$i ${PREFIX}/\$i; done

make-prefix: check-prefix
	mkdir -p ${PREFIX}

check-prefix:
	@test -n "${PREFIX}" || (echo "must specify PREFIX= to install"; exit 1)
