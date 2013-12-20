include ../../../../inc/common.mk

lavToPsl=${DESTBINDIR}/lavToPsl

all::

test: oldStyleBlastz newStyleLastz

newStyleLastz:	mkdirs
	@${lavToPsl} input/$@.lav output/$@.psl
	@diff expected/$@.psl output/$@.psl
	@${lavToPsl} -bed input/$@.lav output/$@.bed
	@diff expected/$@.bed output/$@.bed

oldStyleBlastz:	mkdirs
	@${lavToPsl} input/$@.lav output/$@.psl
	@diff expected/$@.psl output/$@.psl
	@${lavToPsl} -bed input/$@.lav output/$@.bed
	@diff expected/$@.bed output/$@.bed

mkdirs:
	@${MKDIR} output

clean::
	@rm -fr output

