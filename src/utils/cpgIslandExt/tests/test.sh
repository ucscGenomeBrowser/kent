#!/bin/bash

set -beEu -o pipefail

zcat test.fa.gz | cpg_lh /dev/stdin \
 | awk '{$2 = $2 - 1; width = $3 - $2;
   printf("%s\t%d\t%s\t%s %s\t%s\t%s\t%0.0f\t%0.1f\t%s\t%s\n",
    $1, $2, $3, $5, $6, width, $6, width*$7*0.01, 100.0*2*$6/width, $7, $9);}' \
     | sort -k1,1 -k2,2n | gzip -c > testOutput.bed.gz

zdiff expected.bed.gz testOutput.bed.gz
