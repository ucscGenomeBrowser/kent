#!/bin/bash

for C in bacteria birds fish fungi invertebrate mammals plants primates vertebrate viral legacy
do
  cat /mirrordata/hubs/${C}/assemblyList.json | python -mjson.tool \
    | grep '"asmId":' | awk '{print $NF}' | tr -d '"' | cut -d'_' -f1-2 \
      | sort > /dev/shm/clade.${C}.list
done

# # odd man out, no longer in primates hub here
# (cat /dev/shm/clade.primates.list; printf "GCA_009914755.4\n") \
#   | sort > /dev/shm/clade.primates0.list
# rm -f /dev/shm/clade.primates.list
# mv /dev/shm/clade.primates0.list /dev/shm/clade.primates.list

wc -l /dev/shm/clade*.list

exit $?

GCA                          fish             index.html    neuroDiffCrispr
GCF                          fungi            invertebrate  plants
UCSC_GI.assemblyHubList.txt  globalReference  js            primates
VGP                          gtex             legacy        vertebrate
bacteria                     gtexAnalysis     mammals       viral
birds                        inc              mouseStrains  zoonomia

