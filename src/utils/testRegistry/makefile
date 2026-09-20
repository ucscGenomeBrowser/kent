kentSrc = ../..

# Nothing is compiled here.  testRegistry is a python3 script and registry.tsv is the
# data it reads, so the only target that does anything is test, and utils/makefile calls
# it from its own test target.  That is what puts the registry's check into `make test`
# from src, which is the whole reason the check is worth having.

all:
	@echo "testRegistry: nothing to build, see README"

test:
	cd tests && ${MAKE} test

clean:
	cd tests && ${MAKE} clean
