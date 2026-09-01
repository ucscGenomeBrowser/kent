# The harness's own tests. Unlike a CGI's tests/ directory, `make test` here
# really runs: these parse strings and stat files, with no browser and no
# network, so they are safe on any machine and finish in about a second.
#
# Nothing in the tree sweeps this directory -- hg/utils/makefile tests only the
# directories named in its TEST_DIRS, and uiTest is not one of them, the same
# footing docent sits on. Run it by hand after touching lib/.

TESTS = t01-env.js t02-needs.js t03-pw.js t04-results.js

.PHONY: test clean

test:
	@fail=0; \
	for t in $(TESTS); do \
	  node $$t || fail=1; \
	done; \
	if [ $$fail -eq 0 ]; then echo "uiTest self-tests passed"; \
	else echo "uiTest self-tests FAILED"; exit 1; fi

clean:
	@:
