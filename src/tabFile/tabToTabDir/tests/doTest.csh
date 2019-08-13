#!/bin/tcsh -efx
rm -rf out
tabToTabDir input/in.tsv input/spec.txt out
diff -r expected out
echo "looks good"
