#!/bin/tcsh -efx
rm -rf out
tabToTabDir in.tsv spec.txt out
diff -r expected out
echo "looks good"
