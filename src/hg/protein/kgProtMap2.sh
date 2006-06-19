#!/bin/sh
#		kgProtMap.sh
#	usage: kgProtMap.sh <DB> <RO_DB> <YYMMDD>
#		<DB> - database to load, can be temporary
#		<RO_DB> - actual organism database to read other data from
#		<YYMMDD> - date stamp used to find protYYMMDD database
#	use a temporary test <DB> to verify correct operation
#
#	This script is used AFTER a new swissprot, proteins database,
#	and Known Genes tables are created.
# 	See also, scripts: 
#	mkSwissProtDB.sh, mkProteinsDB.sh, and KGprocess.sh
#
#	"$Id: kgProtMap2.sh,v 1.3 2006/06/19 17:12:11 fanhsu Exp $"
#
#	April 2004 - Separted the kgProtMap build process part from
#		     KGprocess.sh
#

###########################  subroutines  ############################

#	ensure usage of latest binaries no matter what PATH the user may have
PATH=/cluster/bin/$MACHTYPE:$PATH
export PATH
# If BLAST_DIR is already defined in the environment, use that, otherwise 
# provide a default value:
echo BLAST_DIR is ${BLAST_DIR:=/scratch/blast}

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
    echo "usage: kgProtMap <DB> <RO_DB> <YYMMDD>"
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
#	src/hg/protein/kgBestMrna
#	src/hg/protein/spm3
#	src/hg/protein/spm6
#	src/hg/protein/spm7
#	src/hg/protein/rmKGPepMrna
#	src/hg/protein/kgXref
#	src/hg/protein/kgAliasM
#	src/hg/protein/kgAliasP
#	src/hg/protein/kgProtAlias
#	src/hg/protein/kgAliasKgXref
#	src/hg/protein/kgAliasRefseq
#	src/hg/protein/kgProtAliasNCBI
#	src/hg/dnaGene
#	src/hg/makeDb/hgMrnaRefseq
#	src/hg/makeDb/hgKgMrna
#	src/hg/makeDb/hgKegg

echo "`date` kgProtMap.sh $*"

foundALL=""
for i in hgsql kgXref rmKGPepMrna \
	/cluster/data/genbank/bin/$MACHTYPE/gbGetSeqs wget \
	hgMrnaRefseq kgGetPep pslReps hgKgMrna kgPrepBestMrna spm3 spm7 \
	kgResultBestMrna rmKGPepMrna kgXref kgAliasM kgAliasP \
	kgProtAlias kgAliasKgXref kgAliasRefseq kgProtAliasNCBI \
	$HOME/kent/src/hg/protein/getKeggList.pl hgKegg hgCGAP \
	${BLAST_DIR}/formatdb
do
    type ${i} > /dev/null 2> /dev/null
    if [ "$?" -ne 0 ]; then
	echo "ERROR: can not find required program: ${i}"
	foundALL="NOT"
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

ISERVER_SETUP=/cluster/bin/scripts/setupIserver
KLUSTER_RUN=/cluster/bin/scripts/runKlusterBatch

ssh kkr1u00 "${ISERVER_SETUP}"  testConnection
RC=$?
if [ "${RC}" -ne 42 ]; then
    echo "ERROR: can not run setupIserver on kkr1u00 via ssh"
    echo -e "\tplease correct to continue.  This command:"
    echo -e "\tssh kkr1u00 ${ISERVER_SETUP} testConnection"
    echo -e "\tneeds to function OK with a return code of 42"
    exit 255
fi

ssh kk "${KLUSTER_RUN}"  testConnection
RC=$?
if [ "${RC}" -ne 42 ]; then
    echo "ERROR: can not access kluster run script on kk via ssh"
    echo -e "\tplease correct to continue.  This command:"
    echo -e "\tssh kk ${KLUSTER_RUN} testConnection"
    echo -e "\tneeds to function OK with a return code of 42"
    exit 255
fi

IS_THERE=`hgsql -e "show tables;" ${PDB} | wc -l`

if [ ${IS_THERE} -lt 10 ]; then
	echo "ERROR: can not find database: ${PDB}"
	echo -e "\tcurrently existing protein databases:"
	hgsql -e "show databases;" mysql | grep -y prot
	exit 255
fi

hgsql -e "show table status;" ${DB} 2>&1 | grep Unknown > /dev/null 2> /dev/null

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

#	next cluster run requires a lot of I/O, use the bluearc
#	to alleviate the stress
if [ ! -d /cluster/bluearc/kgDB/${DB}/kgProtMap ]; then
    mkdir -p /cluster/bluearc/kgDB/${DB}/kgProtMap
    ln -s /cluster/bluearc/kgDB/${DB}/kgProtMap ${TOP}/kgProtMap
fi

if [ ! -d ${TOP}/kgProtMap ]; then
    echo "ERROR: directory does not exist: ${TOP}/kgProtMap"
    echo -e "\tmay be due to pre-existing bluearc directory:"
    echo -e "\t/cluster/bluearc/kgDB/${DB}/kgProtMap"
    echo -e "\tCorrect this before continuing"
    exit 255
fi

cd ${TOP}/kgProtMap

if [ ! -s kgMrna.fa ]; then
    echo "`date` creating kgMrna.fa"
    hgsql -N -e "select * from ${RO_DB}.knownGeneMrna" >kgMrna.tab
    awk '{print ">" $1;print $2}' kgMrna.tab > kgMrna.fa
    rm -f formatdb.log kgMrna.fa.nsq kgMrna.fa.nin kgMrna.fa.nhr
fi
if [ ! -s formatdb.log ]; then
    echo "`date` creating blast database"
    ${BLAST_DIR}/formatdb -i kgMrna.fa -p F
fi

if [ ! -s kgPep.fa ]; then
    echo "`date` creating kgPep.fa"
hgsql -N -e 'select spID,seq from kgXref,knownGenePep where kgID=name' ${RO_DB} \
	| awk '{print ">" $1;print $2}' >kgPep.fa
    rm -fr kgPep
    rm -f rawJobList
fi

if [ ! -d kgPep ]; then
    echo "`date` splitting kgPep.fa"
    mkdir kgPep
    faSplit sequence kgPep.fa 8000 kgPep/kgPep
    rm -f rawJobList
fi

if [ ! -s kgProtMrna.pairs ]; then
    echo "`date` creating kgProtMrna.pairs"
    awk '{printf "%s\t%s\n", $3,$2}' ${TOP}/kgXref.tab > kgProtMrna.pairs
fi

cp -p ~/kent/src/hg/protein/kgProtBlast.csh .

if [ ! -s rawJobList ]; then
    echo "`date` creating rawJobList"
    for f in kgPep/*.fa
    do
      echo ./kgProtBlast.csh $f >> rawJobList
    done

fi

if [ ! -s rawJobList ]; then
    echo "ERROR: job list for kgProtMap cluster run has not been created"
    echo -e "\tmust fix to continue"
    exit 255
fi

if [ ! -f iserverSetupOK ]; then
    sed -e \
	"s# kgPep/# /iscratch/i/kgDB/${DB}/kgProtMap/kgPep/#g" \
	rawJobList > jobList

    rm -f iserverSetupOK
    echo "`date` Preparing iservers for kluster run"
    #	required destination format is: kgDB/${DB}/<whatever>
    ssh kkr1u00 ${ISERVER_SETUP} `pwd`/kgPep kgDB/${DB}/kgProtMap
    RC=$?
    if [ "${RC}" -ne 0 ]; then
	echo "ERROR: iserver setup did not complete successfully"
	exit 255
    fi
    touch iserverSetupOK
fi

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

if [ ! -s psl.tmp/kgProtMrna.psl ]; then
    echo "`date` Assuming kgProtMap cluster run done."
    echo "`date` creating psl.tmp/kgProtMrna.psl"
    find ./psl.tmp -name '*.psl.gz' | xargs zcat | \
	pslReps -nohead -singleHit -minAli=0.9 stdin \
		psl.tmp/kgProtMrna.psl /dev/null
    rm -f psl.tmp/kgProtMap.psl
fi

if [ ! -s psl.tmp/refSeqAli.psl ]; then
    hgsql -N -e 'select * from refSeqAli' ${RO_DB} | cut -f 2-30 > \
	psl.tmp/refSeqAli.psl
    rm -f psl.tmp/kgProtMap.psl
fi

if [ ! -s psl.tmp/kgProtMap.psl ]; then
    echo "`date` creating psl.tmp/kgProtMap.psl"
    cd ${TOP}/kgProtMap/psl.tmp
    cat ${TOP}/tight_mrna.psl refSeqAli.psl > both.psl
    (pslMap kgProtMrna.psl both.psl stdout | sort -u| \
	sort -k 14,14 -k 16,16n -k 17,17n > kgProtMap.psl) > kgProtMap.out 2>&1
    hgsql -e "drop table kgProtMap;" ${DB} 2> /dev/null
fi

cd ${TOP}/kgProtMap

TablePopulated "kgProtMap" ${DB} || { \
    echo "`date` creating table kgProtMap"; \
    hgsql -e "drop table kgProtMap;" ${DB} 2> /dev/null; \
    echo "`date` hgLoadPsl -tNameIx ${DB} psl.tmp/kgProtMap.psl"; \
    hgLoadPsl -tNameIx ${RO_DB} psl.tmp/kgProtMap.psl; \
}

echo "`date` kgProtMap DONE ========================="
