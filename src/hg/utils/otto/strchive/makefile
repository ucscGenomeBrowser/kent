PREFIX = /hive/data/outside/otto/strchive

install: check-prefix
	pylint -E strchiveOtto.py
	mkdir -p ${PREFIX}/releases
	cp strchiveOtto.py ${PREFIX}
	chmod 775 ${PREFIX}/strchiveOtto.py

# ottoCompareGitVsHiveFiles.py mails us when these two drift apart, this is the
# same check run by hand
validate: check-prefix
	diff ${PREFIX}/strchiveOtto.py strchiveOtto.py

check-prefix:
	@test -n "${PREFIX}" || (echo "must specify PREFIX= to install"; exit 1)

clean:
	rm -f *.bb *.bed
