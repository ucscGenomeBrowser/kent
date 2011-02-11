#!/bin/sh

set -beEu -o pipefail

if [ $# -ne 4 ]; then
    echo "usage: update.sh <name> <db> <Db> <Id>" 1>&2
    echo "where <name> is one of: human mouse zebrafish" 1>&2
    echo "<db> is one of: hg19 mm9 danRer7" 1>&2
    echo "<Db> is one of: Hg19 Mm9 DanRer7" 1>&2
    echo "<Id> is one of: GRCH37 MGSCv37 Zv9" 1>&2
    exit 255
fi

export failMail="someUser@domain.top"

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
echo "notThisOne.xml" >> thisDir.list.txt
grep "^\-" .listing | awk '{print $NF}' | grep ".xml" \
	| sed -e 's/\r//g' | sort > theirDir.list.txt
LC=`comm -23 thisDir.list.txt theirDir.list.txt | grep xml | sed -e "/notThisOne.xml/d" | wc -l`
if [ "${LC}" -gt 0 ]; then
    comm -23 thisDir.list.txt theirDir.list.txt | grep xml | sed -e "/notThisOne.xml/d" | while read F
    do
	echo "rm -f \"${F}\""
#	rm -f "${F}"
    done
else
    echo "# no files to delete from: "`pwd`
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
    echo $F ${C}
    rm -fr ${C}
    mkdir ${C}
    autoDtd ${F}.xml ${C}/${C}.dtd ${C}/${C}.stats
    xmlToSql ${F}.xml ${C}/${C}.dtd ${C}/${C}.stats ${C}
done

/cluster/bin/scripts/ncbiIncidentDb.pl \
    -${Id} chr[0-9XY]* 2> dbg.txt | sort -k1,1 -k2,2n > ${db}.bed5

bedToBigBed -bedFields=4 -as=$HOME/kent/src/hg/lib/ncbiIncidentDb.as \
	${db}.bed5 /hive/data/genomes/${db}/chrom.sizes ncbiIncidentDb.bb

newSum=`md5sum -b ncbiIncidentDb.bb | awk '{print $1}'`
oldSum=`md5sum -b ../../${Db}.ncbiIncidentDb.bb | awk '{print $1}'`
echo $newSum
echo $oldSum
if [ "$newSum" != "$oldSum" ]; then
    rm -f ../../${Db}.ncbiIncidentDb.bb.prev
    mv ../../${Db}.ncbiIncidentDb.bb ../../${Db}.ncbiIncidentDb.bb.prev
    ln -s ${name}/${DS}/ncbiIncidentDb.bb ../../${Db}.ncbiIncidentDb.bb
    echo "gwUploadFile ${Db}.ncbiIncidentDb.bb ${Db}.ncbiIncidentDb.bb" \
	| mail -s "ALERT: NCBI Incident update $Db" ${failMail} \
	    > /dev/null 2> /dev/null
fi

exit $?
