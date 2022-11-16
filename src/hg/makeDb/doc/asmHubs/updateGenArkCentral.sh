#!/bin/bash

if [ "xxx$1" != "xxxmakeItSo" ]; then
  printf "usage: ./updateGenArkCentral.sh makeItSo\n" 1>&2
  printf "updates hgcentral.genark with the latest hub list from hgdownload\n" 1>&2
  exit 255
fi

cd ~/kent/src/hg/makeDb/doc/asmHubs
printf "# row count in genark.hgcentraltest before update:\n"
hgsql hgcentraltest -e 'select count(*) from genark;'

scp -p qateam@hgdownload:/mirrordata/hubs/UCSC_GI.assemblyHubList.txt ./

./genArkListToSql.pl makeItSo > genark.tsv

hgsql hgcentraltest -e 'drop table genark;'
hgsql hgcentraltest < ~/kent/src/hg/lib/genark.sql

hgsql hgcentraltest -e "LOAD DATA LOCAL INFILE 'genark.tsv' INTO TABLE genark;"

printf "# row count in genark.hgcentraltest after update:\n"
hgsql hgcentraltest -e 'select count(*) from genark;'

# +----------------+--------------+------+-----+---------+-------+
# | Field          | Type         | Null | Key | Default | Extra |
# +----------------+--------------+------+-----+---------+-------+
# | gcAccession    | varchar(255) | NO   | PRI | NULL    |       |
# | hubUrl         | varchar(255) | NO   |     | NULL    |       |
# | asmName        | varchar(255) | NO   |     | NULL    |       |
# | scientificName | varchar(255) | NO   |     | NULL    |       |
# | commonName     | varchar(255) | NO   |     | NULL    |       |
# | taxId          | int(11)      | NO   |     | NULL    |       |
# +----------------+--------------+------+-----+---------+-------+
