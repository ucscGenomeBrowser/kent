#!/bin/sh

# set -beEu -o pipefail

export TOP="/hive/data/outside/grc/incidentDb"
export ECHO="/bin/echo -e"
export failMail="hiram@soe.ucsc.edu"


if [[ $# == 0 || "$1" != "makeItSo" ]]; then
 echo "ERROR: ${TOP}/runUpdate.sh is being run without the argument: makeItSo" \
	| mail -s "ALERT: GRC Incident update" ${failMail} \
	    > /dev/null 2> /dev/null
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

for D in GRCh37 GRCh38 GRCm38 Zv9 MGSCv37 GRCz10 Gallus_gallus-5.0
do
  mkdir -p ${D}/log/${YM}
done

./grcUpdate.sh GRCh38 hg38 GRCh38.p11_issues human/GRC/Issue_Mapping \
  > GRCh38/log/${YM}/${DS}.txt 2>&1

./grcUpdate.sh GRCh37 hg19 GRCh37.p13_issues human/GRC/Issue_Mapping \
  > GRCh37/log/${YM}/${DS}.txt 2>&1

./grcUpdate.sh GRCm38 mm10 GRCm38.p5_issues mouse/GRC/Issue_Mapping \
  > GRCm38/log/${YM}/${DS}.txt 2>&1

./grcUpdate.sh Zv9 danRer7 Zv9_issues zebrafish/GRC/Issue_Mapping \
  > Zv9/log/${YM}/${DS}.txt 2>&1

./grcUpdate.sh MGSCv37 mm9 MGSCv37_issues mouse/GRC/Issue_Mapping \
  > MGSCv37/log/${YM}/${DS}.txt 2>&1

./grcUpdate.sh GRCz10 danRer10 GRCz10_issues zebrafish/GRC/Issue_Mapping \
  > GRCz10/log/${YM}/${DS}.txt 2>&1

./grcUpdate.sh Gallus_gallus-5.0 galGal5 Gallus_gallus-5.0_issues \
    chicken/GRC/Issue_Mapping > Gallus_gallus-5.0/log/${YM}/${DS}.txt 2>&1

WC=`tail --quiet --lines=1 ${TOP}/GRCh37/log/${YM}/${DS}.txt ${TOP}/GRCh38/log/${YM}/${DS}.txt ${TOP}/GRCm38/log/${YM}/${DS}.txt ${TOP}/MGSCv37/log/${YM}/${DS}.txt ${TOP}/Zv9/log/${YM}/${DS}.txt | grep SUCCESS | wc -l`
if [ "${WC}" -ne 5 ]; then
    ${ECHO} "incidentDb/runUpdate.sh failing" 1>&2
    ${ECHO} "WC: ${WC} <- should be five" 1>&2
    exit 255
fi

if [ 0 = 1 ]; then
### disabled ###
export update="/hive/data/outside/grc/incidentDb/update.sh"
mkdir -p ${TOP}/human
cd ${TOP}/human
# ${update} human hg19 Hg19 GRCh37 > human.hg19.log.${DS} 2>&1
# ${update} human hg38 Hg38 GRCh38 > human.hg38.log.${DS} 2>&1
mkdir -p ${TOP}/mouse
cd ${TOP}/mouse
${update} mouse mm9 Mm9 MGSCv37 > mouse.mm9.log.${DS} 2>&1
${update} mouse mm10 Mm10 GRCm38 > mouse.mm10.log.${DS} 2>&1
mkdir -p ${TOP}/zebrafish
cd ${TOP}/zebrafish
${update} zebrafish danRer7 DanRer7 Zv9 > zebrafish.log.${DS} 2>&1
cd "${TOP}"
# ./verifyTransfer.sh Hg19 Mm9 DanRer7
./verifyTransfer.sh Mm9 DanRer7
if [ $? -ne 0 ]; then
    ${ECHO} "incidentDb/runUpdate.sh failing verifyTransfer.sh" 1>&2
    exit 255
fi

WC=`tail --quiet --lines=1 ${TOP}/mouse/mouse.mm9.log.${DS} ${TOP}/mouse/mouse.mm10.log.${DS} ${TOP}/zebrafish/zebrafish.log.${DS} | grep SUCCESS | wc -l`
if [ "${WC}" -ne 3 ]; then
    ${ECHO} "incidentDb/runUpdate.sh failing" 1>&2
    ${ECHO} "WC: ${WC}" 1>&2
    exit 255
fi

### disabled ###
fi
