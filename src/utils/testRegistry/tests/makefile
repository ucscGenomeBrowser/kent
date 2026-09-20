kentSrc = ../../..

# Two different things are being tested here, and only one of them is about this tool.
#
# `make test` first runs check against the REAL registry.tsv.  That is the rot check: a
# test that was renamed or deleted leaves a row naming a file nobody has, and a registry
# that claims a bug is covered when it is not is worse than no registry at all.  This is
# the target that fails the nightly, and the fix is to edit registry.tsv.
#
# The rest run the tool against small fixtures under input/ and diff the output, which is
# what catches a change to the tool itself.  bad.tsv holds one row per complaint check
# can make, so a complaint that stops being made shows up as a diff rather than as
# silence.

REG = ../testRegistry

test: rotCheck listTest ticketTest testTest releaseTest neededTest unwatchedTest whyTest ticketsTest checkTest
	rm -rf output
	@echo tested all

mkdirs:
	@mkdir -p output

# The real table, against the real tree.
rotCheck:
	${REG} check

listTest: mkdirs
	${REG} --registry input/good.tsv list > output/list.out
	diff expected/list.out output/list.out

ticketTest: mkdirs
	${REG} --registry input/good.tsv ticket 38340 > output/ticket.out
	${REG} --registry input/good.tsv ticket 38303 >> output/ticket.out
	${REG} --registry input/good.tsv ticket 99999 >> output/ticket.out; \
	    echo "  exit $$?" >> output/ticket.out
	diff expected/ticket.out output/ticket.out

testTest: mkdirs
	${REG} --registry input/good.tsv test lib/tests > output/test.out
	diff expected/test.out output/test.out

releaseTest: mkdirs
	${REG} --registry input/good.tsv release 504 > output/release.out
	diff expected/release.out output/release.out

neededTest: mkdirs
	${REG} --registry input/good.tsv needed > output/needed.out
	diff expected/needed.out output/needed.out

unwatchedTest: mkdirs
	${REG} --registry input/good.tsv unwatched > output/unwatched.out
	diff expected/unwatched.out output/unwatched.out

whyTest: mkdirs
	${REG} --registry input/good.tsv why invisible > output/why.out
	diff expected/why.out output/why.out

ticketsTest: mkdirs
	${REG} --registry input/good.tsv tickets > output/tickets.out
	diff expected/tickets.out output/tickets.out

# check must FAIL on bad.tsv.  Its exit status is written into the output and diffed like
# everything else, so the two commands are one shell line: a status read on the next line
# would be the status of that line, which is how a test like this quietly stops testing.
checkTest: mkdirs
	${REG} --registry input/bad.tsv check > output/check.out; \
	    echo "  exit $$?" >> output/check.out
	diff expected/check.out output/check.out

clean:
	rm -rf output
