#!/bin/bash

set -beEu -o pipefail

if [ $# -ne 1 ]; then
  echo "usage: ./quickDbdbBuild.sh <db>" 1>&2
  echo "where <db> is the UCSC database name." 1>&2
  echo "Will verify that the dbDb entry does yet exist before continuing." 1>&2
  exit 255
fi

export DB=$1

export tableCount=`(hgsql -N -e 'select * from dbDb where name="'$DB'";' hgcentraltest 2> /dev/null || /bin/true) | wc -l`

if [ ${tableCount} -gt 0 ]; then
  echo "=== INFO: already done, found dbDb info in hgcentraltest for '${DB}'" 1>&2
  hgsql -e 'select * from dbDb where name="'$DB'"\G' hgcentraltest 1>&2
  exit 0
fi

export dataDir="/hive/data/genomes/${DB}"
if [ -d ${dataDir}/dbDb ]; then
  cd ${dataDir}/dbDb
  echo "==== already done  $dataDir/dbDb ======" 1>&2
  cat *.sql 1>&2
  exit 0
fi

export fileCount=0
for F in chrom.sizes genbank/ASSEMBLY_INFO
do
   if [ -s ${dataDir}/${F} ]; then
      let "fileCount = fileCount+1"
   else
      echo "ERROR: can not find required file: ${dataDir}/${F}" 1>&2
   fi
done

if [ "$fileCount" -ne 2 ]; then
   echo "ERROR: cannot find required file(s) as indicated" 1>&2
   exit 255
fi

mkdir -p /hive/data/genomes/${DB}/dbDb
cd /hive/data/genomes/${DB}/dbDb

export asmInfoPath="/hive/data/genomes/${DB}/genbank/ASSEMBLY_INFO"
export dbDbInfo="${HOME}/kent/src/hg/utils/phyloTrees/dbCommonScientificCladeOrderkey.tab"
export chromSizes="/hive/data/genomes/${DB}/chrom.sizes"
export asmDate=`grep "^DATE:" ${asmInfoPath} | cut -f2 | sed -e 's/^[0-9]+-//; s/-/ /g;' | awk '{print $2,$3}'`
export asmShortLabel=`grep "^ASSEMBLY SHORT NAME:" ${asmInfoPath} | cut -f2`;
export asmSciName=`grep "^ORGANISM:" ${asmInfoPath} | cut -f2`;
export asmTaxID=`grep "^TAXID:" ${asmInfoPath} | cut -f2`;
export asmSourceName=`grep "^ASSEMBLY SUBMITTER:" ${asmInfoPath} | cut -f2`;
export asmAccession=`grep "^ASSEMBLY ACCESSION:" ${asmInfoPath} | cut -f2`;
export orderKey=`grep "^$DB" $dbDbInfo | cut -f6`;
if [ "x${orderKey}y" = "xy" ]; then
  echo "ERROR: missing orderKey for $DB from $dbDbInfo" 1>&2
  exit 255
fi
export clade=`grep "^$DB" $dbDbInfo | cut -f4`;
export commonName=`grep "^$DB" $dbDbInfo | cut -f2`;
export chrName=`head -1 $chromSizes | cut -f1`;
export chrLength=`head -1 $chromSizes | cut -f2`;
export chrStart=`echo $chrLength | awk '{printf "%d", ($1/2)-($1/10)}'`
export chrEnd=`echo $chrLength | awk '{printf "%d", ($1/2)+($1/10)}'`
export defaultPos="${chrName}:${chrStart}-${chrEnd}"

echo "DELETE from dbDb where name = \"$DB\";
INSERT INTO dbDb
    (name, description, nibPath, organism,
     defaultPos, active, orderKey, genome, scientificName,
     htmlPath, hgNearOk, hgPbOk, sourceName, taxId)
VALUES
    (\"$DB\", \"$asmDate ($asmShortLabel/$DB)\", \"/gbdb/$DB\",
     \"$commonName\",
     \"$defaultPos\", 1, $orderKey, \"$commonName\", \"$asmSciName\",
     \"/gbdb/$DB/html/description.html\", 0, 0,
     \"$asmSourceName $asmAccession\", $asmTaxID);" > ${DB}.dbDb.sql

echo \
"INSERT INTO defaultDb (genome, name) VALUES (\"${commonName}\", \"$DB\");" \
    > ${DB}.defaultDb.sql

case $clade in
  "primate")
echo "INSERT INTO genomeClade (genome, clade, priority) VALUES (\"${commonName}\", \"$clade\", \"16\");"
    ;;
  "mammal")
echo "INSERT INTO genomeClade (genome, clade, priority) VALUES (\"${commonName}\", \"$clade\", \"35\");"
    ;;
  "rodent")
echo "INSERT INTO genomeClade (genome, clade, priority) VALUES (\"${commonName}\", \"$clade\", \"40\");"
    ;;
  "vertebrate")
echo "INSERT INTO genomeClade (genome, clade, priority) VALUES (\"${commonName}\", \"$clade\", \"70\");"
    ;;
  "insect")
echo "INSERT INTO genomeClade (genome, clade, priority) VALUES (\"${commonName}\", \"$clade\", \"70\");"
    ;;
esac > ${DB}.genomeClade.sql

cat *.sql | hgsql hgcentraltest

echo "=== ADDED $DB ==="

echo hgsql -e "'"'select * from dbDb where name="'$DB'"\G'"'" hgcentraltest
hgsql -e 'select * from dbDb where name="'$DB'"\G' hgcentraltest
echo hgsql -e "'"'select * from defaultDb where name="'$DB'"\G'"'" hgcentraltest
hgsql -e 'select * from defaultDb where name="'$DB'"\G' hgcentraltest
echo hgsql -e "'"'select * from genomeClade where genome="'$commonName'"\G'"'" hgcentraltest
hgsql -e "select * from genomeClade where genome=\"$commonName\"\G" hgcentraltest

exit $?
