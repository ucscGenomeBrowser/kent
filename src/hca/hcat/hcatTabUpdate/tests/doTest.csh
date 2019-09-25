#!/bin/tcsh -efx
rm -rf out
hcatTabUpdate in out
diff -r expected out
echo "looks good"
