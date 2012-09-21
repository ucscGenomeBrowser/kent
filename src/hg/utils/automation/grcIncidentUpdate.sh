#!/bin/sh

set -beEu -o pipefail

export bbiInfo="/cluster/bin/x86_64/bigBedInfo"
export TOP="/hive/data/outside/grc/incidentDb"

export ECHO="/bin/echo -e"

if [ $# -ne 4 ]; then
    ${ECHO} "usage: update.sh <name> <db> <Db> <Id>" 1>&2
    ${ECHO} "where <name> is one of: human mouse zebrafish" 1>&2
    ${ECHO} "<db> is one of: hg19 mm9 mm10 danRer7" 1>&2
    ${ECHO} "<Db> is one of: Hg19 Mm9 Mm10 DanRer7" 1>&2
    ${ECHO} "<Id> is one of: GRCH37 MGSCv37 GRCm38 Zv9" 1>&2
    exit 255
fi

export failMail="hiram@soe.ucsc.edu"

export name=$1
export db=$2
export Db=$3
export Id=$4

export DS=`date "+%Y-%m-%d"`

# compare this directory contents with .listing and remove files that
#	may have disappeared
cleanDeleted() {
ls *.xml | sort > thisDir.list.txt
# to avoid the grep from failing, add this useless line, remove it with sed
${ECHO} "notThisOne.xml" >> thisDir.list.txt
grep "^\-" .listing | awk '{print $NF}' | grep ".xml" \
	| sed -e 's/\r//g' | sort > theirDir.list.txt
LC=`comm -23 thisDir.list.txt theirDir.list.txt | grep xml | sed -e "/notThisOne.xml/d" | wc -l`
if [ "${LC}" -gt 0 ]; then
    comm -23 thisDir.list.txt theirDir.list.txt | grep xml | sed -e "/notThisOne.xml/d" | while read F
    do
	${ECHO} "rm -f \"${F}\""
#	rm -f "${F}"
    done
else
    ${ECHO} "# no files to delete from: "`pwd`
fi
}

mkdir -p ${name}
wget --timestamping --level=2 -r --cut-dirs=2 --no-host-directories \
	--no-remove-listing ftp://ftp.ncbi.nih.gov/pub/grc/${name}
cd ${name}
cleanDeleted
cd ..

mkdir -p ${DS}
cd ${DS}

for F in `ls ../${name}/*.xml | sed -e "s/.xml//"`
do
    C=`basename ${F}`
    C=${C/_issues/}
    ${ECHO} $F ${C}
    rm -fr ${C}
    mkdir ${C}
    /cluster/bin/x86_64/autoDtd ${F}.xml ${C}/${C}.dtd ${C}/${C}.stats
    /cluster/bin/x86_64/xmlToSql ${F}.xml ${C}/${C}.dtd ${C}/${C}.stats ${C}
done

# the awk filters out bad coordinates in mm9 data

/cluster/bin/scripts/grcIncidentDb.pl \
    -${Id} chr[0-9XY]* 2> ${db}.dbg.txt | sort -k1,1 -k2,2n \
	| awk '$3>=$2' > ${db}.bed5

case "${db}" in
    mm9)
# illegal coordinates have crept into the data:
# sed -e "s/132060741/131737871/; s/132085098/131738871/" ${db}.bed5 > t$$.bed5
/cluster/bin/x86_64/bedToBigBed -type=bed4+1 -as=$HOME/kent/src/hg/lib/grcIncidentDb.as \
	${db}.bed5 /hive/data/genomes/${db}/chrom.sizes $db.grcIncidentDb.bb
#	rm -f t$$.bed5
	;;
    *)
/cluster/bin/x86_64/bedToBigBed -type=bed4+1 -as=$HOME/kent/src/hg/lib/grcIncidentDb.as \
	${db}.bed5 /hive/data/genomes/${db}/chrom.sizes $db.grcIncidentDb.bb
	;;
esac

export newSum=0
export oldSum=0
newSum=`md5sum -b $db.grcIncidentDb.bb | awk '{print $1}'`
if [ -s ../../${Db}.grcIncidentDb.bb ]; then
    oldSum=`md5sum -b ../../${Db}.grcIncidentDb.bb | awk '{print $1}'`
fi
${ECHO} $newSum
${ECHO} $oldSum
if [ "$newSum" != "$oldSum" ]; then
    rm -f ../../${Db}.grcIncidentDb.bb.prev
    if [ -s ../../${Db}.grcIncidentDb.bb ]; then
        mv ../../${Db}.grcIncidentDb.bb ../../${Db}.grcIncidentDb.bb.prev
    fi
    ln -s ${name}/${DS}/$db.grcIncidentDb.bb ../../${Db}.grcIncidentDb.bb
    cd "${TOP}"
    /cluster/bin/scripts/gwUploadFile ${Db}.grcIncidentDb.bb ${Db}.grcIncidentDb.bb
    url=`/cluster/bin/x86_64/hgsql -N -e "select * from grcIncidentDb;" $db`
    rm -fr ${TOP}/udcCache
    mkdir ${TOP}/udcCache
    ${bbiInfo} -udcDir=${TOP}/udcCache "${url}" 2>&1 \
	| mail -s "ALERT: GRC Incident update $Db" ${failMail} \
	    > /dev/null 2> /dev/null
else
    cd "${TOP}"
    rm -fr "${name}/${DS}"
fi

${ECHO} SUCCESS
exit 0
