#!/bin/bash
#############################################################################
###  this script lives in the source tree at:
###      kent/src/hg/utils/otto/genArk/whatToRemove.sh
###  edit that copy instead of the in-use copy at:
###      /hive/data/inside/GenArk/pushRR/whatToRemove.sh
###
###  makes removal listings only, nothing is removed here:
###    to.be.removed.from.hgwbeta.txt - on hgwbeta, not on hgwdev
###    to.be.removed.from.RR.txt      - on hgw1, not on hgwbeta
#############################################################################

export TOP="/hive/data/inside/GenArk/pushRR"

cd "${TOP}"

export doNotCount="public.hub.txt|beta.hub.txt|alpha.hub.txt|user.hub.txt|hub.txt|/contrib/"

### a missing or empty listing would put every file on the other machine
### onto a removal list, refuse to make any lists in that case
for M in dev hgwbeta hgw1
do
  for T in todayList today.quickLiftList today.liftOverList
  do
    if [ -z "$(zcat "${M}.${T}.gz" 2> /dev/null | head -1)" ]; then
      printf "ERROR: missing or empty listing: %s/%s.%s.gz\n" "${TOP}" "${M}" "${T}" 1>&2
      exit 255
    fi
  done
done

##############################################################################
### file names in listing ${2} that are not in listing ${1},
### with ${3} added to the front of each name
##############################################################################
function onlyInSecond() {
  local first="${1}"
  local second="${2}"
  local prefix="${3}"
  zegrep -v "${doNotCount}" "${first}" | cut -f2 | sort \
    | join -v2 -t$'\t' - <(zegrep -v "${doNotCount}" "${second}" | cut -f2 | sort) \
      | sort -u | sed -e "s#^#${prefix}#;"
}

rm -f to.be.removed.from.hgwbeta.txt to.be.removed.from.RR.txt

# on hgwbeta, not on hgwdev
onlyInSecond dev.todayList.gz hgwbeta.todayList.gz "/gbdb/genark/" \
  > to.be.removed.from.hgwbeta.txt
for T in quickLiftList liftOverList
do
  onlyInSecond dev.today.${T}.gz hgwbeta.today.${T}.gz "/gbdb/" \
    >> to.be.removed.from.hgwbeta.txt
done

# on hgw1, not on hgwbeta
onlyInSecond hgwbeta.todayList.gz hgw1.todayList.gz "/gbdb/genark/" \
  > to.be.removed.from.RR.txt
for T in quickLiftList liftOverList
do
  onlyInSecond hgwbeta.today.${T}.gz hgw1.today.${T}.gz "/gbdb/" \
    >> to.be.removed.from.RR.txt
done

printf "### %d files on hgwbeta, not on hgwdev, to be removed from hgwbeta\n" \
  "$(grep -c . to.be.removed.from.hgwbeta.txt)"
printf "### %d files on hgw1, not on hgwbeta, to be removed from RR\n" \
  "$(grep -c . to.be.removed.from.RR.txt)"
