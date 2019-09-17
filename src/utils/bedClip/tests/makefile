kentSrc = ../../..
include ../../../inc/common.mk

PROG = bedClip

test:
	@${PROG} -verbose=2 clipExample.bed.gz ce6.chrom.sizes stdout 2>&1 | grep -v "kent source version" > clip.bed
	@diff clip.bed clip.result
	@${PROG} -truncate -verbose=2 clipExample.bed.gz ce6.chrom.sizes stdout 2>&1 | grep -v "kent source version" > truncate.bed
	@diff truncate.bed truncate.result
	@${PROG} -truncate -verbose=2 twoFieldsExample.bed.gz ce6.chrom.sizes stdout 2>&1 | grep -v "kent source version" > twoFields.bed || true
	@diff twoFields.bed twoFields.result
	@${PROG} -verbose=2 badStartExample.bed.gz ce6.chrom.sizes stdout 2>&1 | grep -v "kent source version" > badStart.bed || true
	@diff badStart.bed badStart.result
	@${PROG} -verbose=2 badEndExample.bed.gz ce6.chrom.sizes stdout 2>&1 | grep -v "kent source version" > badEnd.bed || true
	@diff badEnd.bed badEnd.result
	@${PROG} -verbose=2 badNameExample.bed.gz ce6.chrom.sizes stdout 2>&1 | grep -v "kent source version" > badName.bed || true
	@diff badName.bed badName.result
	${MAKE} clean

clean:
	rm -f clip.bed truncate.bed twoFields.bed badStart.bed badEnd.bed badName.bed

