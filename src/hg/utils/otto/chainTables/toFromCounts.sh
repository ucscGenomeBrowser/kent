#!/bin/bash

### from source tree: kent/src/hg/utils/otto/chainTables/toFromCounts.sh
### otto cron job:
### 11 3 * * * /hive/data/inside/GenArk/checkChainTables/toFromCounts.sh
###

cd /hive/data/inside/GenArk/checkChainTables

hgsql -N -e 'select fromDb,toDb from liftOverChain;' hgcentraltest \
   | sort -u > liftOverChain.fromDb.toDb.txt

hgsql -N -e 'select fromDb,toDb from quickLiftChain;' hgcentraltest \
   | sort -u > quickLiftChain.fromDb.toDb.txt

loC=`md5sum liftOverChain.fromDb.toDb.txt | cut -d' ' -f1`
qlC=`md5sum quickLiftChain.fromDb.toDb.txt | cut -d' ' -f1`

if [ "${loC}" != "${qlC}" ]; then

  printf "ERROR from: /hive/data/inside/GenArk/checkChainTables/toFromCounts.sh\n" 1>&2
  printf "ERROR: liftOverChain from,toDb is not equal to quickLiftChain\n" 1>&2
  printf "perhaps missing from liftOverChain:\n" 1>&2
  comm -13 liftOverChain.fromDb.toDb.txt quickLiftChain.fromDb.toDb.txt 1>&2
  printf "perhaps missing from quickLiftChain:\n" 1>&2
  comm -23 liftOverChain.fromDb.toDb.txt quickLiftChain.fromDb.toDb.txt 1>&2
  exit 255
fi
