#!/bin/sh
#		KGprocess.sh
#	usage: KGprocess <DB> <RO_DB> <YYMMDD>
#		<DB> - database to load, can be temporary
#		<RO_DB> - actual organism database to read other data from
#		<YYMMDD> - date stamp used to find protYYMMDD database
#	use a temporary test <DB> to verify correct operation
#
#	This script is used AFTER a new swissprot and proteins database
#	are created.  See also, scripts:
#	mkSwissProtDB.sh and mkProteinsDB.sh
#
#	"$Id: KGRef3.sh,v 1.1 2006/02/28 18:44:08 fanhsu Exp $"
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
    echo "usage: KGprocess <DB> <RO_DB> <YYMMDD>"
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

echo "`date` KGprocess.sh $*"

foundALL=""
for i in hgsql kgXref rmKGPepRef \
	/cluster/data/genbank/bin/i386/gbGetSeqs wget \
	hgRefRefseq kgGetPep pslReps kgPrepBestRef2 spm3 spm7 \
	kgResultBestRef2 rmKGPepRef kgXref kgAliasM kgAliasP \
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

cd ${TOP}

cd kgBestRef

    rm -f iserverSetupOK
    echo "`date` Preparing iservers for kluster run"
    #	required destination format is: kgDB/${DB}/<whatever>
    ssh kkr1u00 ${ISERVER_SETUP} `pwd`/clusterRun kgDB/${DB}/kgBestRef
    RC=$?
    if [ "${RC}" -ne 0 ]; then
	echo "ERROR: iserver setup did not complete successfully"
	exit 255
    fi
    touch iserverSetupOK

if [ ! -f iserverSetupOK ]; then
    echo "ERROR: iserver setup is not correct."
    echo -e "\tsomething has failed in the kluster run setup"
    exit 255
fi

if [ ! -f klusterRunComplete ]; then
    rm -f klusterRunComplete
    ssh kk "${KLUSTER_RUN}"  `pwd`
    RC=$?
    if [ "${RC}" -ne 0 ]; then
	echo "ERROR: kluster batch did not complete successfully"
	exit 255
    fi
    touch klusterRunComplete
fi

if [ ! -f klusterRunComplete ]; then
    echo "ERROR: kluster run has failed."
    echo -e "\tneeds to be corrected before continuing"
    exit 255
fi
exit 0
#	About 45 minutes of processing time to here
