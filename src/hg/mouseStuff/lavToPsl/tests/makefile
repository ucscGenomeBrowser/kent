include ../../../../inc/common.mk

lavToPsl=${DESTBINDIR}/lavToPsl

all::

test: oldStyleBlastz newStyleLastz redmine12502

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

redmine12502:	mkdirs
	@${lavToPsl} input/$@.lav.gz output/$@.psl
	@zdiff expected/$@.psl.gz output/$@.psl
	@${lavToPsl} -bed input/$@.lav.gz output/$@.bed
	@zdiff expected/$@.bed.gz output/$@.bed

mkdirs:
	@${MKDIR} output

clean::
	@rm -fr output

