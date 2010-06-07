#!/bin/sh

A=$1

INDIR=tests/input
OUTDIR=tests/output
EXPDIR=tests/expected

rm -rf $OUTDIR
mkdir -p $OUTDIR

# Easy case: single sequence, one gap, consistent AGP and sequence.
$A $INDIR/test1.good.agp $INDIR/test1.fa \
    > $OUTDIR/test1.good.fa.out 2>$OUTDIR/test1.good.fa.err
$A $INDIR/test1.good.agp $INDIR/test1.2bit \
    > $OUTDIR/test1.good.2bit.out 2>$OUTDIR/test1.good.2bit.err

# Now the last line of AGP specifies a length too long or too short:
$A $INDIR/test1.agpTooLong.agp $INDIR/test1.fa \
    > $OUTDIR/test1.long.fa.out 2>$OUTDIR/test1.long.fa.err
$A $INDIR/test1.agpTooLong.agp $INDIR/test1.2bit \
    > $OUTDIR/test1.long.2bit.out 2>$OUTDIR/test1.long.2bit.err
$A $INDIR/test1.agpTooShort.agp $INDIR/test1.fa \
    > $OUTDIR/test1.short.fa.out 2>$OUTDIR/test1.short.fa.err
$A $INDIR/test1.agpTooShort.agp $INDIR/test1.2bit \
    > $OUTDIR/test1.short.2bit.out 2>$OUTDIR/test1.short.2bit.err

# AGP has one sequence, fa/2bit has two:
$A $INDIR/test1.good.agp $INDIR/test2.fa \
    > $OUTDIR/test1.extraSeq.fa.out 2>$OUTDIR/test1.extraSeq.fa.err
$A $INDIR/test1.good.agp $INDIR/test2.2bit \
    > $OUTDIR/test1.extraSeq.2bit.out 2>$OUTDIR/test1.extraSeq.2bit.err

# vice versa:
$A $INDIR/test2.good.agp $INDIR/test1.fa \
    > $OUTDIR/test2.missingSeq.fa.out 2> $OUTDIR/test2.missingSeq.fa.err
$A $INDIR/test2.good.agp $INDIR/test1.2bit \
    > $OUTDIR/test2.missingSeq.2bit.out 2> $OUTDIR/test2.missingSeq.2bit.err

# Sequences in different orders -- OK for 2bit but not for fa:
$A $INDIR/test2.goodFor2bit.agp $INDIR/test2.fa \
    > $OUTDIR/test2.goodFor2bit.fa.out 2> $OUTDIR/test2.goodFor2bit.fa.err
$A $INDIR/test2.goodFor2bit.agp $INDIR/test2.2bit \
    > $OUTDIR/test2.goodFor2bit.2bit.out 2> $OUTDIR/test2.goodFor2bit.2bit.err


diff -r $EXPDIR $OUTDIR
