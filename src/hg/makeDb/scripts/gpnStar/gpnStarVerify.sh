#!/bin/bash
# Verify that a re-encoded GPN-Star bigWig holds exactly the same per-base values as the
# original: same chromosome set, identical bigWigToBedGraph output on every chromosome
# (compared per chromosome, since kent writes chromosomes in a different order), and no
# bad section headers.
# usage: gpnStarVerify.sh orig.bw rebuilt.bw
set -beEu -o pipefail
orig=$1; new=$2
chroms() { bigWigInfo -chroms $1 | awk 'NR>1 && /^\t/ {print $1}' | sort; }
if [ "$(chroms $orig)" != "$(chroms $new)" ]; then
    echo "FAIL $orig chromosome sets differ"; exit 1
fi
for c in $(chroms $orig); do
    a=$(bigWigToBedGraph -chrom=$c $orig stdout | md5sum | cut -d' ' -f1)
    b=$(bigWigToBedGraph -chrom=$c $new stdout | md5sum | cut -d' ' -f1)
    if [ "$a" != "$b" ]; then
        echo "FAIL $orig $c orig=$a rebuilt=$b"; exit 1
    fi
done
bad=$($(dirname $0)/gpnStarCheckSections.py $new 0 | awk -F': ' '/^sections=/{print $2}')
if [ "$bad" != "0" ]; then
    echo "FAIL $new bad sections $bad"; exit 1
fi
echo "OK $orig values identical on $(chroms $orig | wc -l) chromosomes, bad sections 0"
