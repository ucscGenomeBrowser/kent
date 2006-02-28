#!/bin/sh
#		KGRef2.sh
#	usage: KGRef2 <DB> <RO_DB> <YYMMDD>
#		<DB> - database to load, can be temporary
#		<RO_DB> - actual organism database to read other data from
#		<YYMMDD> - date stamp used to find protYYMMDD database
#	use a temporary test <DB> to verify correct operation
#
#	This script is used AFTER a new swissprot and proteins database
#	are created.  See also, scripts:
#	mkSwissProtDB.sh and mkProteinsDB.sh
#
#	"$Id: KGRef2.sh,v 1.2 2006/02/28 18:55:40 angie Exp $"
#
#	Thu Nov 20 11:16:16 PST 2003 - Created - Hiram
#		Initial version is a translation of makeKgMm3.doc
#		into a shell script.  For future use on other
#		organisms, the process below should have added
#		verification checks after each step to make sure the
#		step worked properly.  This first time through it was
#		all done one step at a time and existence and contents
#		of database loads were verified and the input files were
#		examined to see if they had data.  There are already a
#		few instances of this below at critical steps.
#	The script runs to a point where a cluster run needs to be done.
#	It prepares the file inputs, output directory and jobList.
#	The cluster run is then just a
#		para create jobList; try, push ...etc"
#	on kk will take about 5 minutes.
#	It will continue after the cluster run has produced the output files
#

###########################  subroutines  ############################

#	ensure usage of latest binaries no matter what PATH the user may have
PATH=/cluster/bin/i386:$PATH
export PATH

#	see if a table exists and it has rows
#	returns 1 for NOT EXISTS 0 for EXISTS
TablePopulated() {
T=$1
D=$2
T_EXISTS=`hgsql -e "describe ${T};" ${D} 2> /dev/null | wc -l`
if [ "${T_EXISTS}" -gt 0 ]; then
    IS_THERE=`hgsql -e "select count(*) from ${T};" ${D} | tail -1`
    if [ "${IS_THERE}" -eq 0 ]; then
	return 1
    else
	return 0
    fi
else
    return 1
fi
}

###########################  MAIN  ###################################

if [ "$#" -ne 3 ]; then
    echo "usage: KGRef2 <DB> <RO_DB> <YYMMDD>"
    echo -e "\t<DB> - organism database to load"
    echo -e "\t<RO_DB> - read only from this database (the target)"
    echo -e "\t<YYMMDD> - date stamp used to find protYYMMDD DB"
    echo -e "\tFor the DB, instead of loading directly into an organism"
    echo -e "\tDB, you can load into a new DB to see if everything is going"
    echo -e "\tto work out."
    exit 255
fi

#	check for all binaries that will be used here. Source locations:
#	src/hg/protein/kgGetPep
#	src/hg/protein/kgBestRef
#	src/hg/protein/spm3
#	src/hg/protein/spm6
#	src/hg/protein/spm7
#	src/hg/protein/rmKGPepRef
#	src/hg/protein/kgXref
#	src/hg/protein/kgAliasM
#	src/hg/protein/kgAliasP
#	src/hg/protein/kgProtAlias
#	src/hg/protein/kgAliasKgXref
#	src/hg/protein/kgAliasRefseq
#	src/hg/protein/kgProtAliasNCBI
#	src/hg/dnaGene
#	src/hg/makeDb/hgRefRefseq
#	src/hg/makeDb/hgKgRef
#	src/hg/makeDb/hgKegg

echo "`date` KGRef2.sh $*"

foundALL=""
for i in hgsql kgXref rmKGPepRef \
	/cluster/data/genbank/bin/i386/gbGetSeqs wget \
	kgGetPep pslReps kgPrepBestRef2 spm3 spm7 \
	kgResultBestRef2 kgXref kgAliasM kgAliasP \
	kgProtAlias kgAliasKgXref kgAliasRefseq kgProtAliasNCBI \
	$HOME/kent/src/hg/protein/getKeggList.pl hgKegg hgCGAP
do
    type ${i} > /dev/null 2> /dev/null
    if [ "$?" -ne 0 ]; then
	echo "ERROR: can not find required program: ${i}"
#	foundALL="NOT"
    fi
done

if [ -n "${foundALL}" ]; then
    echo "ERROR: most of these programs are in kent/src/hg/protein"
    exit 255
fi

DB=$1
RO_DB=$2
DATE=$3
PDB=proteins${DATE}
TOP=/cluster/data/kgDB/bed/${DB}
export DB RO_DB DATE PDB TOP
echo DB=$DB
echo RO_DB=$RO_DB
echo DATE=$DATE
echo PDB=$PDB
echo TOP=$TOP

ISERVER_SETUP=/cluster/bin/scripts/setupIserver
KLUSTER_RUN=/cluster/bin/scripts/runKlusterBatch
echo before testConnection 
ssh kkr1u00 "${ISERVER_SETUP}"  testConnection
RC=$?
if [ "${RC}" -ne 42 ]; then
    echo "ERROR: can not run setupIserver on kkr1u00 via ssh"
    echo -e "\tplease correct to continue.  This command:"
    echo -e "\tssh kkr1u00 ${ISERVER_SETUP} testConnection"
    echo -e "\tneeds to function OK with a return code of 42"
    exit 255
fi

echo before testConnection KLUSTER_RUN
ssh kk "${KLUSTER_RUN}"  testConnection
RC=$?
if [ "${RC}" -ne 42 ]; then
    echo "ERROR: can not access kluster run script on kk via ssh"
    echo -e "\tplease correct to continue.  This command:"
    echo -e "\tssh kk ${KLUSTER_RUN} testConnection"
    echo -e "\tneeds to function OK with a return code of 42"
    exit 255
fi


echo before testConnection KLUSTER_RUN
IS_THERE=`hgsql -e "show tables;" ${PDB} | wc -l`

if [ ${IS_THERE} -lt 10 ]; then
	echo "ERROR: can not find database: ${PDB}"
	echo -e "\tcurrently existing protein databases:"
	hgsql -e "show databases;" mysql | grep -y prot
	exit 255
fi
echo after testConnection KLUSTER_RUN

hgsql -e "show table status;" ${DB}Temp 2>&1 | grep Unknown > /dev/null 2> /dev/null

if [ $? -ne 1 ]; then
	echo "ERROR: can not find database: ${DB}"
	exit 255
fi

IS_THERE=`hgsql -e "show tables;" ${RO_DB} | wc -l`

if [ ${IS_THERE} -lt 10 ]; then
	echo "ERROR: can not find database: ${RO_DB}"
	exit 255
fi

echo "`date` using protein database: ${PDB}"

if [ ! -d ${TOP} ]; then
	echo "`date` mkdir ${TOP}"
	mkdir ${TOP}
fi

if [ ! -d ${TOP} ]; then
	echo "Can not create ${TOP}"
	exit 255
fi

cd ${TOP}

if [ ! -d kgBestRef ]; then
	mkdir kgBestRef
fi

cd kgBestRef
ln -s ../protRef.lis . 2> /dev/null

#	kgPrepBestRef reads the protRef.lis file and the
#	SwissProt tables sp${DATE}.displayId and sp${DATE}.protein
#	and the ${DB}Temp.spRef table to generate a hierarchy
#	of data directories to be used in the cluster run.
if [ ! -d clusterRun ]; then
    echo "`date` Preparing kgPrepBestRef2 cluster run data and jobList"
    kgPrepBestRef2 ${DATE} ${DB}Temp ${RO_DB} > Prep.out
    sed -e \
	"s#./clusterRun#/iscratch/i/kgDB/${DB}/kgBestRef/clusterRun#g" \
	rawJobList > jobList
fi
echo KGRef2 done.

exit 0

