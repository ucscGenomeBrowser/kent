#!/bin/bash

#############################################################################
###  This source is from the source tree:
###     ~/kent/src/hg/utils/otto/genArk/pushNewOnes.sh
###  do *not* edit this in the otto directory /hive/data/inside/GenArk/pushRR/
###  where this is used.
#############################################################################

# exit on any error
set -beEu -o pipefail

export TOP="/hive/data/inside/GenArk/pushRR"
export expectHost="hgwdev"

export hostName=$(hostname -s)
if [[ "${hostName}" != "${expectHost}" ]]; then
  printf "ERROR: must run this on %s !  This is: %s\n" "${expectHost}" "${hostName}" 1>&2
  exit 255
fi

cd "${TOP}"

### this script will create listings for new files in existing browsers:

./whatIsNew.sh

### example:
# -rw-rw-r-- 1   42974 Aug 15 12:14 new.files.ready.to.beta.txt
# -rw-rw-r-- 1      43 Aug 15 12:14 new.beta.timeStamps.txt


#############################################################################
### rsync out one given directory.  The excludes avoid:
###  1. the various alpha.hub.txt, beta.hub.txt, public.hub.txt and user.hub.txt
###  2. any contrib/ directories.
###  These things are taken care of via the quickPush.pl script
###  Note, a 'hub.txt' file will go out since it does exist and is not excluded.
###  It actually represents the 'public.hub.txt' file even though they are
###  are different files.  That should be fixed up with a symLink for those
###  sources so they can be the same identical file.
#############################################################################

function sendTo() {
   export D="${1}"
   for M in hgwbeta
   do
     printf "ssh qateam\@${M} \"umask 002; mkdir -p '/gbdb/genark/${D}'\"\n" 1>&2
     ssh qateam@${M} "umask 002; mkdir -p '/gbdb/genark/${D}'" < /dev/null
     printf "time (rsync --delete --stats -a -L --itemize-changes --exclude=\"alpha.hub.txt\" --exclude=\"beta.hub.txt\" --exclude=\"public.hub.txt\" --exclude=\"user.hub.txt\" --exclude=\"contrib/\" \"/gbdb/genark/${D}/\" \"qateam@${M}:/gbdb/genark/${D}/\") 2>&1\n" 1>&2
     time (rsync --delete --stats -a -L --itemize-changes --exclude="alpha.hub.txt" --exclude="beta.hub.txt" --exclude="public.hub.txt" --exclude="user.hub.txt" --exclude="contrib/" "/gbdb/genark/${D}/" "qateam@${M}:/gbdb/genark/${D}/") 2>&1
   done
}
#############################################################################

#############################################################################
### these listings: dev.todayList.gz and hgwbeta.todayList.gz are created by
###  cron jobs elsewhere before this script runs

printf "### starting pushNewOnes.sh %s\n" "`date '+%F %T'`" 1>&2

export inCommon=`comm -12 <(zgrep "/hub.txt" dev.todayList.gz | cut -f2 | sort)  <(zgrep "/hub.txt" hgwbeta.todayList.gz | cut -f2 | sort) | wc -l`
printf "# in common hgwdev to hgwbeta: %d\n" "${inCommon}" 1>&2

export onHgwBetaNotDev=`comm -13 <(zgrep "/hub.txt" dev.todayList.gz | cut -f2 | sort) <(zgrep "/hub.txt" hgwbeta.todayList.gz | cut -f2 | sort) | wc -l`
printf "# on hgwbeta not in hgwdev: %d\n" "${onHgwBetaNotDev}" 1>&2

export onDevNotHgwBeta=`comm -23 <(zgrep "/hub.txt" dev.todayList.gz | cut -f2 | sort) <(zgrep "/hub.txt" hgwbeta.todayList.gz | cut -f2 | sort) | wc -l`
printf "# on hgwdev not in hgwbeta: %d\n" "${onDevNotHgwBeta}" 1>&2

### this is only a check for brand new assemblies since just the 'hub.txt'
### file listings were compared
if [ "${onDevNotHgwBeta}" -gt 0 ]; then

comm -23 <(zgrep "/hub.txt" dev.todayList.gz | cut -f2 | sort) \
   <(zgrep "/hub.txt" hgwbeta.todayList.gz | cut -f2 | sort) \
     | sed -e 's#/hub.txt##;' | while read P
do	# make sure it really is a valid directory path
   validCount=`echo "${P}" | awk -F'/' '{print NF}'`
   if [ "${validCount}" -gt 4 ]; then
      if [ -d "/gbdb/genark/${P}" ]; then	# and it really is a directory
        sendTo "${P}"
      else
        printf "ERROR: invalid directory path '/gbdb/genark/${P}'\n" 1>&2
        exit 255
      fi
   else
      printf "ERROR: invalid directory path '${P}'\n" 1>&2
      exit 255
   fi
done

else
  printf "# nothing to push, all up to date.\n" 1>&2
fi

# check existing browsers that may have new or updated files to go out
if [ -s "new.files.ready.to.beta.txt" ]; then
   for M in hgwbeta
   do
     printf "time (rsync --stats -a -L --files-from=new.files.ready.to.beta.txt \"/gbdb/genark/\" \"qateam@${M}:/gbdb/genark/\") 2>&1\n" 1>&2
     time (rsync --stats -a -L --files-from=new.files.ready.to.beta.txt "/gbdb/genark/" "qateam@${M}:/gbdb/genark/") 2>&1
   done
fi

# and the /gbdb/*/quickLift files:
if [ -s "new.quickLift.ready.to.beta.txt" ]; then
   for M in hgwbeta
   do
     printf "time (rsync --stats -a -L --files-from=new.quickLift.ready.to.beta.txt \"/gbdb/\" \"qateam@${M}:/gbdb/\") 2>&1\n" 1>&2
     time (rsync --stats -a -L --files-from=new.quickLift.ready.to.beta.txt "/gbdb/" "qateam@${M}:/gbdb/") 2>&1
   done
fi

if [ -s "new.beta.timeStamps.txt" ]; then
   for M in hgwbeta
   do
     printf "time (rsync --stats -a -L --files-from=new.beta.timeStamps.txt \"/gbdb/genark/\" \"qateam@${M}:/gbdb/genark/\") 2>&1\n" 1>&2
     time (rsync --stats -a -L --files-from=new.beta.timeStamps.txt "/gbdb/genark/" "qateam@${M}:/gbdb/genark/") 2>&1
   done
fi

# and the /gbdb/*/quickLift files:
if [ -s "beta.quickLift.timeStamps.txt" ]; then
   for M in hgwbeta
   do
     printf "time (rsync --stats -a -L --files-from=beta.quickLift.timeStamps.txt \"/gbdb/\" \"qateam@${M}:/gbdb/\") 2>&1\n" 1>&2
     time (rsync --stats -a -L --files-from=beta.quickLift.timeStamps.txt "/gbdb/" "qateam@${M}:/gbdb/") 2>&1
   done
fi

printf "### finished pushNewOnes.sh %s\n" "`date '+%F %T'`" 1>&2
