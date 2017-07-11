kentSrc = ../../..
include ../../../inc/common.mk

PROG = bedClip

test:
	@${PROG} -verbose=2 clipExample.bed.gz ce6.chrom.sizes stdout > clip.bed 2>&1
	@diff clip.bed clip.result
	@${PROG} -truncate -verbose=2 clipExample.bed.gz ce6.chrom.sizes stdout > truncate.bed 2>&1
	@diff truncate.bed truncate.result
	@${PROG} -truncate -verbose=2 twoFieldsExample.bed.gz ce6.chrom.sizes stdout > twoFields.bed 2>&1 || true
	@diff twoFields.bed twoFields.result
	@${PROG} -verbose=2 badStartExample.bed.gz ce6.chrom.sizes stdout > badStart.bed 2>&1 || true
	@diff badStart.bed badStart.result
	@${PROG} -verbose=2 badEndExample.bed.gz ce6.chrom.sizes stdout > badEnd.bed 2>&1 || true
	@diff badEnd.bed badEnd.result
	@${PROG} -verbose=2 badNameExample.bed.gz ce6.chrom.sizes stdout > badName.bed 2>&1 || true
	@diff badName.bed badName.result

clean:
	rm -f clip.bed truncate.bed twoFields.bed badStart.bed badEnd.bed badName.bed

