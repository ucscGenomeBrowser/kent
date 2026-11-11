kentSrc = ../../..
A = regionsToChain

# cgiLoader.mk, not userApp.mk:  hgTracks runs this to build a multi-region coordinate
# space and looks for it at "loader/regionsToChain", beside the CGI, the same way
# customFactory.c reaches "loader/hgLoadBed".  userApp.mk installs only to ~/bin, which
# apache never sees, so `make cgi` has to put a copy in the loader directory.  This also
# needs hg/makefile's BROWSER_LOADERS to name the directory, or `make cgi` never enters
# it.  refs #37788
include $(kentSrc)/inc/cgiLoader.mk

clean::
	rm -rf tests/output

test::
	rm -rf tests/output
	mkdir -p tests/output
#	Regions on two source sequences lay into one synthetic sequence called "multi",
#	in the order given, with a new chain wherever the source sequence changes or a
#	region does not move forward on the source.
	${DESTBINDIR}/regionsToChain -keepTemp tests/input/regions.bed tests/input/chrom.sizes \
		tests/output/space.bb
	diff tests/expected/sizes tests/output/space.bb.sizes
	diff tests/expected/chain.bed tests/output/space.bb.chain.bed
	diff tests/expected/link.bed tests/output/space.bb.link.bed
	test -s tests/output/space.bb
	test -s tests/output/space.link.bb
#	Regions on ONE source sequence keep that sequence's name, which is what lets the
#	browser ask for the synthetic sequence by the name the user typed.
	${DESTBINDIR}/regionsToChain -keepTemp tests/input/oneChrom.bed tests/input/chrom.sizes \
		tests/output/one.bb
	diff tests/expected/one.sizes tests/output/one.bb.sizes
	diff tests/expected/one.chain.bed tests/output/one.bb.chain.bed
	test -s tests/output/one.bb
#	An alt shown in place:  the chromosome's flank, the alt, then the rest of the
#	chromosome.  Sorting this list would lay the alt after the end.
	${DESTBINDIR}/regionsToChain -keepTemp tests/input/altInPlace.bed tests/input/chrom.sizes \
		tests/output/alt.bb
	diff tests/expected/alt.chain.bed tests/output/alt.bb.chain.bed
	diff tests/expected/alt.link.bed tests/output/alt.bb.link.bed
	test -s tests/output/alt.bb
#	The same alt laid down reverse-complemented, because it is placed on its
#	chromosome in the opposite orientation.  It is a chain of its own on the minus
#	strand, and its query coordinates count from the end of chr2, as a chain's do.
	${DESTBINDIR}/regionsToChain -keepTemp tests/input/altReversed.bed tests/input/chrom.sizes \
		tests/output/altRev.bb
	diff tests/expected/altReversed.chain.bed tests/output/altRev.bb.chain.bed
	diff tests/expected/altReversed.link.bed tests/output/altRev.bb.link.bed
	test -s tests/output/altRev.bb
