#!/bin/sh
#
#	mkProteinsDb.sh
#	- currently no arguments, but it should be modified to take
#	- a date stamp argument instead of creating one since you do
#	- want a consistent date stamp name between the proteins and
#	- the swissprot databases.
#
#	This script could be improved to do error checking for each step.
#
#	Thu Nov 20 11:31:51 PST 2003 - Created - Hiram
#
#	"$Id: mkProteinsDB.sh,v 1.1 2003/11/20 19:37:44 hiram Exp $"

TOP=/cluster/data/proteins
export TOP

type spToProteins > /dev/null 2> /dev/null

if [ "$?" -ne 0 ]; then
    echo "ERROR: can not find required program: spToProteins"
    echo -e "\tYou may need to build it in ~/kent/src/hg/protein/spToProteins"
    exit 255
fi

MACHINE=`uname -n`

if [ ${MACHINE} != "hgwdev" ]; then
	echo "ERROR: must run this script on hgwdev.  This is: ${MACHINE}"
	exit 255
fi

DATE=`date "+%y%m%d"`
PDB="proteins${DATE}"
SPDB="${DATE}"

echo "Creating Db: ${PDB}"

if [ -d "$DATE" ]; then
	echo "WARNING: ${PDB} already exists."
	echo -e "Do you want to remove it and recreate ? (ynq) \c"
	read YN
	if [ "${YN}" = "Y" -o "${YN}" = "y" ]; then
	echo "Recreating ${PDB}"
	rm -fr ./${DATE}
	hgsql -e "drop database ${PDB}" proteins072003
	else
	echo "Will not recreate at this time."
	exit 255
	fi
fi

mkdir ${TOP}/${DATE}
cd ${TOP}/${DATE}
echo hgsql -e "create database ${PDB};" proteins072003
hgsql -e "create database ${PDB};" proteins072003
hgsqldump -d proteins072003 | ${TOP}/bin/rmSQLIndex.pl > proteins.sql
echo "hgsql ${PDB} < proteins.sql"
hgsql ${PDB} < proteins.sql

#	Create and load tables in proteins
echo spToProteins ${SPDB}
spToProteins ${SPDB}
cd ${TOP}/${DATE}
hgsql -e 'LOAD DATA local INFILE "spXref2.tab" into table spXref2;' ${PDB}
hgsql -e 'create index i1 on spXref2(accession);' ${PDB}
hgsql -e 'create index i2 on spXref2(displayID);' ${PDB}
hgsql -e 'create index i3 on spXref2(extAC);' ${PDB}
hgsql -e 'create index i4 on spXref2(bioentryID);' ${PDB}
hgsql -e 'LOAD DATA local INFILE "spXref3.tab" into table spXref3;' ${PDB}
hgsql -e 'create index ii1 on spXref3(accession);' ${PDB}
hgsql -e 'create index ii2 on spXref3(displayID);' ${PDB}
hgsql -e 'create index ii3 on spXref3(hugoSymbol);' ${PDB}
hgsql -e 'LOAD DATA local INFILE "spOrganism.tab" into table spOrganism;' ${PDB}

#	build HUGO database
mkdir /cluster/store5/proteins/hugo/${DATE}
cd /cluster/store5/proteins/hugo/${DATE}
wget --timestamping "http://www.gene.ucl.ac.uk/public-files/nomen/nomeids.txt"
sed -e "1d" nomeids.txt > hugo.tab
hgsql -e 'LOAD DATA local INFILE "hugo.tab" into table hugo;' ${PDB}

#	Build spSecondaryID table
cd ${TOP}/${DATE}
hgsql -e "select displayId.val, displayId.acc, otherAcc.val from displayId, \
        otherAcc where otherAcc.acc = displayId.acc;" ${SPDB} \
	| sed -e "1d" > spSecondaryID.tab
hgsql -e \
	'LOAD DATA local INFILE "spSecondaryID.tab" into table spSecondaryID;' \
	${PDB}

#	Build pfamXref and pfamDesc tables
mkdir /cluster/store5/proteins/pfam/${DATE}
cd /cluster/store5/proteins/pfam/${DATE}
wget --timestamping "ftp://ftp.sanger.ac.uk/pub/databases/Pfam/Pfam-A.full.gz"
#	100 Mb compressed, over 700 Mb uncompressed
gunzip Pfam-A.full.gz
pfamXref ${PDB} Pfam-A.full pfamADesc.tab pfamAXref.tab

hgsql -e 'LOAD DATA local INFILE "pfamADesc.tab" into table pfamDesc;' ${PDB}
hgsql -e 'LOAD DATA local INFILE "pfamAXref.tab" into table pfamXref;' ${PDB}

#	Build the pdbSP table
cd ${TOP}/${DATE}
wget --timestamping "http://us.expasy.org/cgi-bin/lists?pdbtosp.txt" \
	-O pdbtosp.htm
pdbSP ${PDB}
hgsql -e 'LOAD DATA local INFILE "pdbSP.tab" into table pdbSP;' ${PDB}

#	Build the spDisease table
hgsql -e "select comment.acc, displayId.val, commentVal.val from \
	comment, commentVal, displayId where comment.commentType=19 \
	and commentVal.id=comment.commentVal and displayId.acc=comment.acc;" \
	${SPDB} | sed -e "1d" > spDisease.tab
hgsql -e 'LOAD DATA local INFILE "spDisease.tab" into table spDisease;' ${PDB}
