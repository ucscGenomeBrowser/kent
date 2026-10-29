PREFIX = /hive/data/outside/otto/mavemd

SCRIPTS = mavemdOtto.py mavemdFetch.py mavemdBuild.py mavemdLib.py \
	makeMaveMdVariants.py makeMaveMdHeatmap.py
AS = mavemdVariants.as mavemdHeatmap.as
# The trackDb stanza is installed too, so mavemdBuild.filterDrift() can tell when the
# data has filter values the committed menus do not offer. otto cannot reach the kent
# tree; ottoCompareGitVsHiveFiles.py keeps this copy honest.
TRACKDB = ../../../makeDb/trackDb/human/hg38/mavemd.ra

# pylint is broken on hgwdev (2026-10-14), so install only byte-compiles with the cron
# python 3.9, which catches newer syntax.

install: check-prefix test
	/usr/bin/python3 -m py_compile ${SCRIPTS}
	mkdir -p ${PREFIX}/cache ${PREFIX}/releases ${PREFIX}/logs
	cp ${SCRIPTS} ${AS} ${PREFIX}
	cp ${TRACKDB} ${PREFIX}/mavemd.ra
	chmod 775 ${PREFIX}/mavemdOtto.py ${PREFIX}/mavemdFetch.py ${PREFIX}/mavemdBuild.py

test:
	/usr/bin/python3 tests/testMavemdOtto.py
	/usr/bin/python3 tests/testMavemdFetch.py
	/usr/bin/python3 tests/testMavemdBuild.py

# ottoCompareGitVsHiveFiles.py mails us when the two copies drift apart; this is the same
# check by hand
validate: check-prefix
	for f in ${SCRIPTS} ${AS}; do diff ${PREFIX}/$$f $$f || exit 1; done
	diff ${PREFIX}/mavemd.ra ${TRACKDB} || exit 1

check-prefix:
	@test -n "${PREFIX}" || (echo "must specify PREFIX= to install"; exit 1)
