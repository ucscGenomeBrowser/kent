PREFIX = /hive/data/outside/otto/mavedb

SCRIPTS = mavedbOtto.py mavedbFetch.py mavedbBuild.py mavedbPlace.py mavedbMaps.py \
	mavedbVariants.py mavedbAlign.py
AS = mavedbMaps.as mavedbVariants.as

# pylint is broken on hgwdev (2026-10-14), so install only byte-compiles with the cron
# python 3.9, which catches newer syntax.

install: check-prefix test
	/usr/bin/python3 -m py_compile ${SCRIPTS}
	mkdir -p ${PREFIX}/cache ${PREFIX}/releases ${PREFIX}/logs
	cp ${SCRIPTS} ${AS} ${PREFIX}
	cp ../../../lib/bigPsl.as ${PREFIX}
	chmod 775 ${PREFIX}/mavedbOtto.py ${PREFIX}/mavedbFetch.py

test:
	/usr/bin/python3 tests/testMavedbPlace.py
	/usr/bin/python3 tests/testMavedbFetch.py
	/usr/bin/python3 tests/testMavedbOtto.py

# ottoCompareGitVsHiveFiles.py mails us when the two copies drift apart; this is the same
# check by hand
validate: check-prefix
	for f in ${SCRIPTS} ${AS}; do diff ${PREFIX}/$$f $$f || exit 1; done

check-prefix:
	@test -n "${PREFIX}" || (echo "must specify PREFIX= to install"; exit 1)
