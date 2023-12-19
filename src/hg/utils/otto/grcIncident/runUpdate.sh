#!/bin/sh

# set -beEu -o pipefail

export TOP="/hive/data/outside/otto/grcIncidentDb"
export ECHO="/bin/echo -e"
export failMail="hiram@soe.ucsc.edu,otto-group@ucsc.edu"

if [[ $# == 0 || "$1" != "makeItSo" ]]; then
  printf "To: $failMail\nFrom: $failMail\nSubject: ALERT: GRC Incident update\n\nERROR: ${TOP}/runUpdate.sh is being run without the argument: makeItSo\n" | /usr/sbin/sendmail -t -oi
# echo "ERROR: ${TOP}/runUpdate.sh is being run without the argument: makeItSo" \
#	| mail -s "ALERT: GRC Incident update" ${failMail} \
#	    > /dev/null 2> /dev/null
    ${ECHO} "usage: runUpdate.sh makeItSo"
    ${ECHO} "this script needs the argument: makeItSo"
    ${ECHO} "to make it run.  It will update the GRC incident database"
    ${ECHO} "tracks in the working directory:"
    ${ECHO} "${TOP}"
    ${ECHO} "activity logs can be found in:"
    ${ECHO} "${TOP}/*/*.log.YYYY-mm-dd"
    exit 255
fi

# echo "WARN: ${TOP}/runUpdate.sh is disabled" \
# 	| mail -s "ALERT: GRC Incident update" ${failMail} \
# 	    > /dev/null 2> /dev/null
# exit $?

cd ${TOP}

export DS=`date "+%Y-%m-%d"`
export YM=`date "+%Y/%m"`

for D in GRCh37 GRCh38 GRCm38 GRCm39 Zv9 MGSCv37 GRCz10 GRCz11 Gallus_gallus-5.0 GRCg6a
do
  mkdir -p ${D}/log/${YM}
done

./grcUpdate.sh GRCh38 hg38 GRCh38.p14_issues human/GRC/Issue_Mapping \
  > GRCh38/log/${YM}/${DS}.txt 2>&1

./grcUpdate.sh GRCh37 hg19 GRCh37.p13_issues human/GRC/Issue_Mapping \
  > GRCh37/log/${YM}/${DS}.txt 2>&1

./grcUpdate.sh GRCm38 mm10 GRCm38.p6_issues mouse/GRC/Issue_Mapping \
  > GRCm38/log/${YM}/${DS}.txt 2>&1

./grcUpdate.sh GRCm39 mm39 GRCm39_issues mouse/GRC/Issue_Mapping \
  > GRCm39/log/${YM}/${DS}.txt 2>&1

./grcUpdate.sh Zv9 danRer7 Zv9_issues zebrafish/GRC/Issue_Mapping \
  > Zv9/log/${YM}/${DS}.txt 2>&1

./grcUpdate.sh MGSCv37 mm9 MGSCv37_issues mouse/GRC/Issue_Mapping \
  > MGSCv37/log/${YM}/${DS}.txt 2>&1

./grcUpdate.sh GRCz10 danRer10 GRCz10_issues zebrafish/GRC/Issue_Mapping \
  > GRCz10/log/${YM}/${DS}.txt 2>&1

./grcUpdate.sh GRCz11 danRer11 GRCz11_issues zebrafish/GRC/Issue_Mapping \
  > GRCz11/log/${YM}/${DS}.txt 2>&1

./grcUpdate.sh GRCg6a galGal6 GRCg6a_issues chicken/GRC/Issue_Mapping \
  > GRCg6a/log/${YM}/${DS}.txt 2>&1

./grcUpdate.sh Gallus_gallus-5.0 galGal5 Gallus_gallus-5.0_issues \
    chicken/GRC/Issue_Mapping > Gallus_gallus-5.0/log/${YM}/${DS}.txt 2>&1

WC=`tail --quiet --lines=1 ${TOP}/GRCg6a/log/${YM}/${DS}.txt ${TOP}/GRCh37/log/${YM}/${DS}.txt ${TOP}/GRCh38/log/${YM}/${DS}.txt ${TOP}/GRCm38/log/${YM}/${DS}.txt ${TOP}/GRCm39/log/${YM}/${DS}.txt ${TOP}/GRCz10/log/${YM}/${DS}.txt ${TOP}/GRCz11/log/${YM}/${DS}.txt ${TOP}/Gallus_gallus-5.0/log/${YM}/${DS}.txt ${TOP}/MGSCv37/log/${YM}/${DS}.txt ${TOP}/Zv9/log/${YM}/${DS}.txt | grep SUCCESS | wc -l`
if [ "${WC}" -ne 10 ]; then
    ${ECHO} "incidentDb/runUpdate.sh failing" 1>&2
    ${ECHO} "WC: ${WC} <- should be ten" 1>&2
    for T in ${TOP}/GRCg6a/log/${YM}/${DS}.txt ${TOP}/GRCh37/log/${YM}/${DS}.txt ${TOP}/GRCh38/log/${YM}/${DS}.txt ${TOP}/GRCm38/log/${YM}/${DS}.txt ${TOP}/GRCm39/log/${YM}/${DS}.txt ${TOP}/GRCz10/log/${YM}/${DS}.txt ${TOP}/GRCz11/log/${YM}/${DS}.txt ${TOP}/Gallus_gallus-5.0/log/${YM}/${DS}.txt ${TOP}/MGSCv37/log/${YM}/${DS}.txt ${TOP}/Zv9/log/${YM}/${DS}.txt
    do
       c=`tail --lines=1 "${T}" | grep SUCCESS | wc -l`
       if [ "${c}" -ne 1 ]; then
          printf "# failing %s\n" "${T}" 1>&2
       fi
    done
    exit 255
fi
