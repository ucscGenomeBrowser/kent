#!/bin/bash

# exit on any error
set -beEu -o pipefail

export expectHost="hgwdev"

export hostName=$(hostname -s)
if [[ "${hostName}" != "${expectHost}" ]]; then
  printf "ERROR: must run this on %s !  This is: %s\n" "${expectHost}" "${hostName}" 1>&2
  exit 255
fi

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
   for M in hgwbeta hgw0 hgw1 hgw2 "Genome-Browser-Mirror-3.dhcp.uni-bielefeld.de"
   do
     printf "ssh qateam\@${M} \"umask 002; mkdir -p '/gbdb/genark/${D}'\"\n" 1>&2
     ssh qateam@${M} "umask 002; mkdir -p '/gbdb/genark/${D}'" < /dev/null
     printf "time (rsync --delete --stats -a -L --itemize-changes --exclude=\"alpha.hub.txt\" --exclude=\"beta.hub.txt\" --exclude=\"public.hub.txt\" --exclude=\"user.hub.txt\" --exclude=\"contrib/\" \"/gbdb/genark/${D}/\" \"qateam@${M}:/gbdb/genark/${D}/\") 2>&1\n" 1>&2
     time (rsync --delete --stats -a -L --itemize-changes --exclude="alpha.hub.txt" --exclude="beta.hub.txt" --exclude="public.hub.txt" --exclude="user.hub.txt" --exclude="contrib/" "/gbdb/genark/${D}/" "qateam@${M}:/gbdb/genark/${D}/") 2>&1
   done
}
#############################################################################

#############################################################################
### these listings: dev.todayList.gz and hgw1.todayList.gz are created by
###  cron jobs elsewhere before this script runs

printf "### starting pushNewOnes.sh %s\n" "`date '+%F %T'`" 1>&2

export inCommon=`comm -12 <(zgrep "/hub.txt" dev.todayList.gz | cut -f2 | sort)  <(zgrep "/hub.txt" hgw1.todayList.gz | cut -f2 | sort) | wc -l`
printf "# in common hgwdev to hgw1: %d\n" "${inCommon}" 1>&2

export onHgw1NotDev=`comm -13 <(zgrep "/hub.txt" dev.todayList.gz | cut -f2 | sort) <(zgrep "/hub.txt" hgw1.todayList.gz | cut -f2 | sort) | wc -l`
printf "# on hgw1 not in hgwdev: %d\n" "${onHgw1NotDev}" 1>&2

export onDevNotHgw1=`comm -23 <(zgrep "/hub.txt" dev.todayList.gz | cut -f2 | sort) <(zgrep "/hub.txt" hgw1.todayList.gz | cut -f2 | sort) | wc -l`
printf "# on hgwdev not in hgw1: %d\n" "${onDevNotHgw1}" 1>&2

if [ "${onDevNotHgw1}" -gt 0 ]; then

comm -23 <(zgrep "/hub.txt" dev.todayList.gz | cut -f2 | sort) \
   <(zgrep "/hub.txt" hgw1.todayList.gz | cut -f2 | sort) \
     | sed -e 's#/hub.txt##;' | while read P
do
   sendTo "${P}"
done

else
  printf "# nothing to push, all up to date.\n" 1>&2
fi
