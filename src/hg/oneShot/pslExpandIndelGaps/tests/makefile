kentSrc = ../../../..
include ${kentSrc}/inc/common.mk

pslExpandIndelGaps = ${DESTBINDIR}/pslExpandIndelGaps

test:: ncbiBogusGapsNM ncbiBogusGapsXM
	${MAKE} clean

ncbiBogusGapsNM: mkdirs
	${pslExpandIndelGaps} -verbose=2 galGal6 input/galGal6.NM_001030566.1.psl output/$@.psl 2>output/$@.stderr
	diff -u expected/$@.psl output/$@.psl
	grep -v 'kent source version' output/$@.stderr | diff -u expected/$@.stderr -

ncbiBogusGapsXM: mkdirs
	${pslExpandIndelGaps} -verbose=2 galGal6 input/galGal6.XM_025152810.1.psl output/$@.psl 2>output/$@.stderr
	diff -u expected/$@.psl output/$@.psl
	grep -v 'kent source version' output/$@.stderr | diff -u expected/$@.stderr -

clean::
	rm -rf output

mkdirs:
	@${MKDIR} output
