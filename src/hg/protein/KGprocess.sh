#!/bin/sh
#		KGprocess.sh
#	usage: KGprocess <DB> <RO_DB> <YYMMDD>
#		<DB> - database to load, can be temporary
#		<RO_DB> - actual organism database to read other data from
#		<YYMMDD> - date stamp used to find proteinsYYMMDD database
#	use a temporary test <DB> to verify correct operation
#
#	This script is used AFTER a new swissprot and proteins database
#	are created.  See also, scripts:
#	mkSwissProtDB.sh and mkProteinsDB.sh
#
#	"$Id: KGprocess.sh,v 1.5 2004/01/29 00:36:55 hiram Exp $"
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
#	Also a second argument should be added to specify the database to
#	work on.  Right now it is specified below as "DB=mm4"


###########################  subroutines  ############################

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
    echo -e "\t<YYMMDD> - date stamp used to find proteinsYYMMDD DB"
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

echo "`date` KGprocess.sh $*"

foundALL=""
for i in hgsql kgXref rmKGPepMrna \
	/cluster/data/genbank/bin/i386/gbGetSeqs wget \
	hgMrnaRefseq kgGetPep pslReps hgKgMrna kgPrepBestMrna spm3 spm7 \
	kgResultBestMrna dnaGene rmKGPepMrna kgXref kgAliasM kgAliasP \
	kgProtAlias kgAliasKgXref kgAliasRefseq kgProtAliasNCBI \
	$HOME/kent/src/hg/protein/getKeggList.pl hgKegg hgCGAP
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
TOP=/cluster/data/${DB}/bed/knownGenes
export DB RO_DB DATE PDB TOP

IS_THERE=`hgsql -e "show tables;" ${PDB} | wc -l`

if [ ${IS_THERE} -lt 10 ]; then
	echo "ERROR: can not find database: ${PDB}"
	echo -e "\tcurrently existing protein databases:"
	hgsql -e "show databases;" mysql | grep -y protein
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
	mkdir ${TOP}
fi

if [ ! -d ${TOP} ]; then
	echo "Can not create ${TOP}"
	exit 255
fi

cd ${TOP}

#	Fetch current mrna sequences from organism genbank system
if [ ! -s mrna.fa ]; then
    echo "`date` fetch mrna.fa sequences from ${RO_DB}"
    /cluster/data/genbank/bin/i386/gbGetSeqs -native -db=${RO_DB} \
	-gbRoot=/cluster/data/genbank genbank mrna mrna.fa
    rm -f mrna.lis
    rm -f ${DB}KgMrna.out
fi

if [ ! -s mrna.ra ]; then
    echo "`date` fetch mrna.ra sequences from ${RO_DB}"
    /cluster/data/genbank/bin/i386/gbGetSeqs -get=ra -native -db=${RO_DB} \
	-gbRoot=/cluster/data/genbank genbank mrna mrna.ra
    rm -f ${DB}KgMrna.out
fi

if [ ! -s all_mrna.psl ]; then
    echo "`date` fetch all_mrna.psl sequences from ${RO_DB}"
#    /cluster/data/genbank/bin/i386/gbGetSeqs -get=psl -native -db=${RO_DB} \
#	-gbRoot=/cluster/data/genbank genbank mrna all_mrna.psl
    hgsql -N -e 'select * from all_mrna' ${RO_DB} | cut -f 2-30 >all_mrna.psl
    rm -f tight_mrna.psl
fi

if [ ! -s mrna.fa -o ! -s mrna.ra -o ! -s all_mrna.psl ]; then
    echo "ERROR: can not find one of mrna.fa, mrna.ra or all_mrna.psl"
    echo -e "\tShould have been fetched by gbGetSeqs"
    exit 255;
fi

#	generate list of mrna accession numbers
if [ ! -f mrna.lis ]; then
    grep "^>" mrna.fa > mrna.lis
    rm -f mrnaPep.fa
fi

#	Fetch LocusLink dat to generate mrnaRefseq table
#	This step begins the ${DB}Temp database as a temporary working
#	database for intermediate tables during processing
if [ ! -d ll ]; then
    mkdir -p ll
    cd ll
    if [ ! -f loc2ref ]; then
	wget --timestamping "ftp://ftp.ncbi.nih.gov/refseq/LocusLink/loc2ref"
    fi
    if [ ! -f loc2acc ]; then
	wget --timestamping "ftp://ftp.ncbi.nih.gov/refseq/LocusLink/loc2acc"
    fi
    if [ ! -f mim2loc ]; then
	wget --timestamping "ftp://ftp.ncbi.nih.gov/refseq/LocusLink/mim2loc"
    fi

    hgsql -e "drop database ${DB}Temp;" ${DB} 2> /dev/null
    hgsql -e "create database ${DB}Temp;" ${DB}
    hgsql ${DB}Temp < ~/kent/src/hg/protein/Temp.sql
#	drop history table so that hgKgMrna will work properly
    hgsql -e "drop table history;" ${DB}Temp 2> /dev/null

    echo "`date` loading loc2acc and loc2ref into ${DB}Temp"

    hgsql -e 'LOAD DATA local INFILE "loc2acc" into table locus2Acc0;' ${DB}Temp
    hgsql -e 'LOAD DATA local INFILE "loc2ref" into table locus2Ref0;' ${DB}Temp
    cd ${TOP}
    rm -f mrnaRefseq.tab
    rm -f ${DB}KgMrna.out
fi

#	mrnaRefseq.tab is a two column table created by reading
#	${DB}Temp.locus2Ref0 and ${DB}Temp.locus2Acc0 to cross reference:
#	GenBank Accession number	RefSeq Accession number
#	e.g.:	AA001432        NM_000227
#	hgMrnaRefseq reads from ${DB}Temp.locus2Ref0 and ${DB}Temp.locus2Acc0
#	to create the mrnaRefseq.tab
if [ ! -f mrnaRefseq.tab ]; then
    echo "`date` running hgMrnaRefseq ${DB}"
    hgMrnaRefseq ${DB}
    echo "`date` loading mrnaRefseq.tab into ${DB}.mrnaRefseq"
    hgsql -e "drop table mrnaRefseq;" ${DB} 2> /dev/null

    hgsql -e 'CREATE TABLE mrnaRefseq (
	mrna varchar(40) NOT NULL default "",
	refseq varchar(40) NOT NULL default "",
	KEY mrna (mrna),
	KEY refseq (refseq)
	) TYPE=MyISAM;' ${DB}
 hgsql -e 'LOAD DATA local INFILE "mrnaRefseq.tab" into table mrnaRefseq;' ${DB}
fi

#	kgGetPep reads the mrna.lis (mrna accession numbers) to
#	extract from DBs proteins${DATE} and sp{DATE} the fasta records
#	for each mrna
#	reads from proteins${DATE}.spXref2 and sp${DATE}.protein
#	to create mrnaPep.tab and mrna.lis
if [ ! -f mrnaPep.fa ]; then
    echo "`date` running: kgGetPep ${DATE}"
    kgGetPep ${DATE} > mrnaPep.fa
    rm -f ${DB}KgMrna.out
fi

#	Filter the all_mrna's to create tight_mrna.psl
if [ ! -f tight_mrna.psl ]; then
    echo "`date` running: pslReps all_mrna.psl tight_mrna.psl"
    pslReps -minCover=0.40 -sizeMatters -minAli=0.97 -nearTop=0.002 \
	all_mrna.psl tight_mrna.psl /dev/null
    rm -f ${DB}KgMrna.out
fi

#	Load tables productName, geneName, refLink, refPep,
#	refGene, and refMrna tables, creates a history table too
#	into ${DB}Temp
if [ ! -f ${DB}KgMrna.out ]; then
    echo "`date` running: hgKgMrna ${DB}Temp ... ${PDB}"
    hgsql -e "delete from productName;" ${DB}Temp
    hgsql -e "delete from geneName;" ${DB}Temp
    hgsql -e "delete from refLink;" ${DB}Temp
    hgsql -e "delete from refPep;" ${DB}Temp
    hgsql -e "delete from refGene;" ${DB}Temp
    hgsql -e "delete from refMrna;" ${DB}Temp
    hgsql -e "delete from mrnaGene;" ${DB}Temp
    hgKgMrna ${DB}Temp mrna.fa mrna.ra tight_mrna.psl ll/loc2ref \
	mrnaPep.fa ll/mim2loc ${PDB} > ${DB}KgMrna.out 2> ${DB}KgMrna.err
    hgsql -e "drop table knownGenePep;" ${DB} 2> /dev/null
    hgsql -e "drop table knownGeneMrna;" ${DB} 2> /dev/null
fi

for T in refGene refMrna refPep refLink
do
    if [ ! -s ${T}.tab ]; then
	echo "ERROR: can not find file: ${T}.tab"
	echo -e "\tShould have been created by hgKgMrna"
	exit 255
    fi
done

#	Loading the tables created by hgKgMrna
#	I'm not so sure about this.  Temp.refMrna seems to have already
#	have been loaded by hgKgMrna itself.
#	mrnaGene is used by spm6
IS_THERE=`hgsql -e "select count(*) from mrnaGene;" ${DB}Temp | tail -1`
if [ "${IS_THERE}" -eq 0 ]; then
    hgsql -e "delete from mrnaGene;" ${DB}Temp
    echo "`date` loading refGene.tab into ${DB}Temp.mrnaGene"
    hgsql -e 'LOAD DATA local INFILE "refGene.tab" into table mrnaGene;' \
	${DB}Temp
    rm -f ${TOP}/kgBestMrna/knownGene0.tab
    rm -f proteinMrna.tab
fi

IS_THERE=`hgsql -e "select count(*) from refMrna;" ${DB}Temp | tail -1`
if [ "${IS_THERE}" -eq 0 ]; then
    echo "`date` loading refMrna.tab into ${DB}Temp.refMrna"
    hgsql -e 'LOAD DATA local INFILE "refMrna.tab" into table refMrna;' \
	${DB}Temp
fi

#	WARNING: .tab file names do not match DB table names
#	(I'm not sure how I decided on this backslash continued line
#	structure.  That shouldn't be necessary.  This should be
#	straightened out.)
TablePopulated "knownGenePep" ${DB} || { \
    echo "`date` loading refPep.tab into ${DB}.knownGenePep"; \
    hgsql -e "drop table knownGenePep;" ${DB} 2> /dev/null; \
    hgsql ${DB} < ~/kent/src/hg/lib/knownGenePep.sql; \
    hgsql -e \
	'LOAD DATA local INFILE "refPep.tab" into table knownGenePep;' ${DB}; \
}

TablePopulated "knownGeneMrna" ${DB} || { \
    echo "`date` loading refMrna.tab into ${DB}.knownGeneMrna"; \
    hgsql -e "drop table knownGeneMrna;" ${DB} 2> /dev/null; \
    hgsql ${DB} < ~/kent/src/hg/lib/knownGeneMrna.sql; \
    hgsql -e \
     'LOAD DATA local INFILE "refMrna.tab" into table knownGeneMrna;' ${DB}; \
}

#	spm3 reads from ${DB}Temp.refGene and proteins${DATE}.spXref2
#	to create proteinMrna.tab and protein.lis
if [ ! -f proteinMrna.tab ]; then
    echo "`date` running spm3 ${DATE} ${DB}"
    spm3 ${DATE} ${DB}
    hgsql -e "delete from spMrna;" ${DB}Temp
fi

if [ ! -f proteinMrna.tab ]; then
	echo "ERROR: can not find file: proteinMrna.tab"
	echo -e "\tShould have been created by spm3"
	exit 255
fi

TablePopulated "spMrna" ${DB}Temp || { \
    echo "`date` loading proteinMrna.tab into ${DB}Temp.spMrna" 2> /dev/null; \
    hgsql -e 'LOAD DATA local INFILE "proteinMrna.tab" into table spMrna;' \
	${DB}Temp; \
}

if [ ! -d kgBestMrna ]; then
	mkdir kgBestMrna
fi

cd kgBestMrna

ln -s ../protein.lis . 2> /dev/null

#	kgPrepBestMrna reads the protein.lis file and the
#	SwissProt tables sp${DATE}.displayId and sp${DATE}.protein
#	and the ${DB}Temp.spMrna table to generate a hierarchy
#	of data directories to be used in the cluster run.
if [ ! -d clusterRun ]; then
	echo "`date` Preparing cluster run data and jobList"
	kgPrepBestMrna ${DATE} ${DB} 2> jobList > Prep.out
	echo "`date` Cluster Run has been prepared."
	echo "on machine kk in: "`pwd`
	echo "perform:"
	echo "para create jobList"
	echo "para try"
	echo "para push ... etc."
	exit 255
fi

#	About 45 minutes of processing time to here

#	kgResultBestMrna processes the results of the cluster run
cd ${TOP}/kgBestMrna
if [ ! -f best.lis ]; then
	echo "`date` Assuming cluster run done, Running analysis of output."
	echo "`date` kgResultBestMrna ${DATE} ${DB} ${RO_DB}"
	$HOME/bin/i386/kgResultBestMrna ${DATE} ${DB} ${RO_DB} > ResultBest.out 2>&1
fi

BEST_LEN=`cat best.lis | wc -l`
if [ "${BEST_LEN}" -lt 1000 ]; then
	echo "ERROR: do not find correct results from kgResultBestMrna"
	echo "ERROR: best.lis length: $BEST_LEN"
	exit 255
fi


cd ${TOP}/kgBestMrna
TablePopulated "spMrna" ${DB} || { \
    hgsql -e "drop table spMrna;" ${DB} 2> /dev/null; \
    hgsql ${DB} < ~/kent/src/hg/lib/spMrna.sql; \
    echo "`date` loading best.lis into ${DB}.spMrna"; \
    hgsql -e 'LOAD DATA local INFILE "best.lis" into table spMrna;' ${DB}; \
}

#	spm6 reads best.lis and from ${DB}Temp.mrnaGene
#	creating knownGene0.tab and sorted.lis
if [ ! -s knownGene0.tab ]; then
    echo "`date` running spm6 ${DATE} ${DB} ${RO_DB}"
    spm6 ${DATE} ${DB} ${RO_DB}
fi

if [ ! -s knownGene0.tab ]; then
    echo "ERROR: can not find knownGene0.tab"
    echo -e "\tShould have been created by spm6 operation"
    exit 255
fi

TablePopulated "knownGene0" ${DB}Temp || { \
    echo "`date` loading knownGene0.tab into ${DB}Temp.knownGene0"; \
    hgsql -e 'LOAD DATA local INFILE "knownGene0.tab" into table knownGene0;' \
	${DB}Temp; \
}

#	spm7 reads sorted.lis and from ${DB}Temp.knownGene0 and
#	sp${DATE}.displayId sp${DATE}.protein
#	to create knownGene.tab and duplicate.tab
if [ ! -s knownGene.tab ]; then
    echo "`date` running spm7 ${DATE} ${DB}"
    spm7 ${DATE} ${DB} > spm7.out
fi

if [ ! -s knownGene.tab ]; then
    echo "ERROR: can not find knownGene.tab"
    echo -e "\tShould have been created by spm7 operation"
    exit 255
fi

if [ ! -s duplicate.tab ]; then
    echo "ERROR: can not find duplicate.tab"
    echo -e "\tShould have been created by spm7 operation"
    exit 255
fi

TablePopulated "dupSpMrna" ${DB} || { \
    hgsql -e "drop table dupSpMrna;" ${DB} 2> /dev/null; \
    hgsql ${DB} < ~/kent/src/hg/lib/dupSpMrna.sql; \
    echo "`date` loading duplicate.tab into ${DB}.dupSpMrna"; \
    hgsql -e 'LOAD DATA local INFILE "duplicate.tab" into table dupSpMrna;' \
	${DB}; \
}

TablePopulated "knownGene" ${DB} || {
    hgsql -e "drop table knownGene;" ${DB} 2> /dev/null; \
    hgsql ${DB} < ~/kent/src/hg/lib/knownGene.sql; \
    echo "`date` loading knownGene.tab into ${DB}.knownGene"; \
    hgsql -e 'LOAD DATA local INFILE "knownGene.tab" into table knownGene;' \
	${DB}; \
    rm -f loadedKnownDnaGene
}

#	dnaGne reads from ${DB}Temp.locus2Acc0, ${DB}Temp.locus2Ref0 and
#	${RO_DB}.refGene
#	to create dnaGene.tab and dnaLink.tab
if [ ! -s dnaLink.tab ]; then
    echo "`date` running dnaGene ${DB} ${PDB} ${RO_DB}"
    dnaGene ${DB} ${PDB} ${RO_DB}
    rm -f loadedKnownDnaGene
fi

if [ ! -s dnaGene.tab ]; then
    echo "ERROR: can not find dnaGene.tab"
    echo -e "\tShould have been created by dnaGene operation"
    exit 255
fi

if [ ! -s dnaLink.tab ]; then
    echo "ERROR: can not find dnaLink.tab"
    echo -e "\tShould have been created by dnaGene operation"
    exit 255
fi

TablePopulated "knownGeneLink" ${DB} || { \
    hgsql -e "drop table knownGeneLink;" ${DB} 2> /dev/null; \
    hgsql ${DB} < ~/kent/src/hg/lib/knownGeneLink.sql; \
    echo "`date` loading dnaLink.tab into ${DB}.knownGeneLink"; \
    hgsql -e 'LOAD DATA local INFILE "dnaLink.tab" into table knownGeneLink;' \
	${DB}; \
}

#	We need to add dnaGene.tab to knownGene
#	to make sure this is all done correctly, reload the entire
#	table
if [ ! -f loadedKnownDnaGene ]; then
    hgsql -e "drop table knownGene;" ${DB} 2> /dev/null
    hgsql ${DB} < ~/kent/src/hg/lib/knownGene.sql
    echo "`date` loading knownGene.tab into ${DB}.knownGene"
    hgsql -e 'LOAD DATA local INFILE "knownGene.tab" into table knownGene;' \
	${DB}
    hgsql -e "select count(*) from knownGene;" ${DB}
    echo "`date` loading dnaGene.tab into ${DB}.knownGene"
    hgsql -e 'LOAD DATA local INFILE "dnaGene.tab" into table knownGene;' \
	${DB}
    hgsql -e "select count(*) from knownGene;" ${DB}
    touch loadedKnownDnaGene
fi

cd ${TOP}

#	rmKGPepMrna reads from ${DB}.knownGene.proteinID,
#	${DB}.knownGenePep.seq, ${DB}.proteinID, sp${DATE}.displayId.acc,
#	sp${DATE}.protein.val, ${DB}.knownGeneMrna.seq, sp${DATE}.displayId,
#	sp${DATE}.protein
#	to create knownGenePep.tab and knownGeneMrna.tab
if [ ! -s knownGenePep.tab ]; then
    echo "`date` running rmKGPepMrna ${DB} ${DATE} ${RO_DB}"
    rmKGPepMrna ${DB} ${DATE} ${RO_DB}
    rm -f loadedPepMrna
fi

if [ ! -s knownGeneMrna.tab ]; then
    echo "ERROR: can not find knownGeneMrna.tab"
    echo -e "\tShould have been created by rmKGPepMrna operation"
    exit 255
fi

if [ ! -f loadedPepMrna ]; then
    hgsql -e "drop table knownGenePep;" ${DB} 2> /dev/null
    hgsql ${DB} < ~/kent/src/hg/lib/knownGenePep.sql
    echo "`date` loading knownGenePep into ${DB}.knownGenePep"
    hgsql -e \
    'LOAD DATA local INFILE "knownGenePep.tab" into table knownGenePep;' ${DB}

    hgsql -e "drop table knownGeneMrna;" ${DB} 2> /dev/null
    hgsql ${DB} < ~/kent/src/hg/lib/knownGeneMrna.sql
    echo "`date` loading knownGeneMrna into ${DB}.knownGeneMrna"
    hgsql -e \
    'LOAD DATA local INFILE "knownGeneMrna.tab" into table knownGeneMrna;' ${DB}
    touch loadedPepMrna
fi

#	kgXref reads from  ${DB}.knownGene.name, ${DB}.knownGene.proteinID
#	${PDB}.spXref3.accession, ${PDB}.spXref3.description
#	${PDB}.spXref3.hugoSymbol, ${PDB}.spXref3.hugoDesc
#	${DB}.knownGeneLink.seqType, ${DB}.refLink.name,
#	${DB}.refLink.product, ${DB}.refLink.protAcc,
#	${DB}.mrnaRefseq.refseq
#	to create kgXref.tab
if [ ! -s kgXref.tab ]; then
    echo "`date` running kgXref ${DB} ${PDB}"
    kgXref ${DB} ${PDB}
fi

if [ ! -s kgXref.tab ]; then
    echo "ERROR: can not find kgXref.tab"
    echo -e "\tShould have been created by kgXref operation"
    exit 255
fi

TablePopulated "kgXref" ${DB} || { \
    hgsql -e "drop table kgXref;" ${DB} 2> /dev/null; \
    hgsql ${DB} < ~/kent/src/hg/lib/kgXref.sql; \
    echo "`date` loading kgXref into ${DB}.kgXref"; \
    hgsql -e \
    'LOAD DATA local INFILE "kgXref.tab" into table kgXref;' ${DB}; \
    rm -f kgAliasM.tab
}

#	kgAliasM reads from ${PDB}.hugo.symbol, ${PDB}.hugo.aliases
#	${PDB}.hugo.withdraws, ${DB}.kgXref.kgID
#	to create kgAliasM.tab and geneAlias.tab
#	by picking out those kgID items from kgXref where
#	kgXref.geneSymbol == hugo.symbol
if [ ! -s kgAliasM.tab ]; then
    echo "`date` running kgAliasM ${DB} ${PDB}"
    kgAliasM ${DB} ${PDB}
    rm -f kgAlias.tab
fi

#	kgAliasKgXref reads from ${DB}.knownGene.proteinID,
#	${DB}.knownGene.name, ${DB}.kgXref.geneSymbol
#	to create kgAliasKgXref.tab
if [ ! -s kgAliasKgXref.tab ]; then
    echo "`date` running kgAliasKgXref ${DB}"
    kgAliasKgXref ${DB}
    rm -f kgAlias.tab
fi

#	kgAliasRefseq reads from ${DB}.knownGene.name,
#	${DB}.knownGene.proteinID, ${DB}.kgXref.refseq
#	to create kgAliasRefseq.tab
if [ ! -s kgAliasRefseq.tab ]; then
    echo "`date` running kgAliasRefseq ${DB}"
    kgAliasRefseq ${DB}
    rm -f kgAlias.tab
fi

#	kgAliasP reads the first file and from ${DB}.knownGene.name
#	to create the second .lis file
if [ ! -s kgAliasP.tab ]; then
    echo "`date` running kgAliasP ${DB} ... sp.lis"
    if [ -f /cluster/data/swissprot/${DATE}/build/sprot.dat ]; then
	kgAliasP ${DB} /cluster/data/swissprot/${DATE}/build/sprot.dat sp.lis
    elif [ -f /cluster/data/swissprot/${DATE}/build/sprot.dat.gz ]; then
	zcat /cluster/data/swissprot/${DATE}/build/sprot.dat.gz | \
	    kgAliasP ${DB} stdin sp.lis
    else
	echo \
"ERROR: Can not find /cluster/data/swissprot/${DATE}/build/sprot.dat[.gz]"
	exit 255
    fi
    echo "`date` running kgAliasP ${DB} ... tr.lis"
    if [ -f /cluster/data/swissprot/${DATE}/build/trembl.dat ]; then
	kgAliasP ${DB} /cluster/data/swissprot/${DATE}/build/trembl.dat tr.lis
    elif [ -f /cluster/data/swissprot/${DATE}/build/trembl.dat.gz ]; then
	zcat /cluster/data/swissprot/${DATE}/build/trembl.dat.gz | \
	    kgAliasP ${DB} stdin tr.lis
    else
	echo \
"ERROR: Can not find /cluster/data/swissprot/${DATE}/build/trembl.dat[.gz]"
	exit 255
    fi
    echo "`date` running kgAliasP ${DB} ... new.lis"
    if [ -f /cluster/data/swissprot/${DATE}/build/trembl_new.dat ]; then
	kgAliasP ${DB} \
	    /cluster/data/swissprot/${DATE}/build/trembl_new.dat new.lis
    elif [ -f /cluster/data/swissprot/${DATE}/build/trembl_new.dat.gz ]; then
	zcat /cluster/data/swissprot/${DATE}/build/trembl_new.dat.gz | \
	    kgAliasP ${DB} stdin new.lis
    else
	echo \
"ERROR: Can not find /cluster/data/swissprot/${DATE}/build/trembl_new.dat[.gz]"
	exit 255
    fi
    cat sp.lis tr.lis new.lis | sort | uniq > kgAliasP.tab
    rm -f kgAlias.tab
fi

if [ ! -s kgAlias.tab ]; then
    cat kgAliasM.tab kgAliasRefseq.tab kgAliasKgXref.tab kgAliasP.tab | \
	sort |uniq > kgAlias.tab
    hgsql -e "drop table kgAlias;" ${DB} 2> /dev/null
fi

TablePopulated "kgAlias" ${DB} || { \
    echo "`date` creating table kgAlias"; \
    hgsql -e "drop table kgAlias;" ${DB} 2> /dev/null; \
    hgsql ${DB} < ~/kent/src/hg/lib/kgAlias.sql; \
    hgsql -e \
    'LOAD DATA local INFILE "kgAlias.tab" into table kgAlias;' ${DB}; \
}

#	kgProtAlias reads from ${DB}.knownGene.name,
#	${DB}.knownGene.proteinID, ${DB}.knownGene.alignID,
#	${PDB}.spXref3.accession, ${PDB}.spSecondaryID, ${PDB}.pdbSP.pdb
#	to create kgProtAlias.tab
#
#	kgProtAliasNCBI reads from ${DB}.knownGene.name,
#	${DB}.knownGene.proteinID, ${DB}.kgXref.protAcc,
#	${DB}.refLink.protAcc, ${DB}Temp.locus2Acc0.proteinAC
#	to create kgProtAliasNCBI.tab
if [ ! -s kgProtAliasBoth.tab ]; then
    echo "`date` running kgProtAlias ${DB} ${PDB}"
    kgProtAlias ${DB} ${PDB}
    echo "`date` running kgProtAliasNCBI ${DB} ${RO_DB}"
    kgProtAliasNCBI ${DB} ${RO_DB}
    cat kgProtAliasNCBI.tab kgProtAlias.tab | sort | uniq > kgProtAliasBoth.tab
    rm kgProtAliasNCBI.tab kgProtAlias.tab
fi

TablePopulated "kgProtAlias" ${DB} || { \
    echo "`date` creating table kgProtAlias"; \
    hgsql -e "drop table kgProtAlias;" ${DB} 2> /dev/null; \
    hgsqldump --all -d -c hg16 kgProtAlias > ./kgProtAlias.sql; \
    hgsql ${DB} < ./kgProtAlias.sql; \
    hgsql -e \
    'LOAD DATA local INFILE "kgProtAliasBoth.tab" into table kgProtAlias;' \
	${DB}; \
}

#	This perl script getKeggList.pl is a tricky operation
#	It requires SOAP and XML modules to be installed
#	I see they are in /usr/lib/perl5/site_perl/5.6.1
#	But our perl in /usr/local/bin is 5.8.0
#	The location has been set in the script with the -I argument
#	This script uses the .lis file extracted from the html WEB page
#	to fetch via SOAP the data from the kegg site.
#
#	This business here is species specific:
#	mmu == mouse, rno == rat, hsa = human
SPECIES=hsa
case ${RO_DB} in
    mm*) SPECIES=mmu
	;;
    rn*) SPECIES=rno
	;;
    hg*) SPECIES=hsa
	;;
esac

if [ ! -f ${SPECIES}.lis ]; then
    wget --timestamping -O ${SPECIES}.html "http://www.genome.ad.jp/dbget-bin/www_bfind_sub?dbkey=pathway&keywords=${SPECIES}&mode=bfind&max_hit=1000&.cgifields=max_hit"
    grep HREF hsa.html | perl -wpe "s/<[^>]+>//g" > hsa.lis
    $HOME/kent/src/hg/protein/getKeggList.pl ${SPECIES} > keggList.tab
fi

if [ ! -f keggList.tab ]; then
    echo "ERROR: can not find keggList.tab"
    echo -e "\tShould have been created by getKeggList.pl operation"
    exit 255
fi

TablePopulated "keggList" ${DB}Temp || { \
    echo "`date` loading keggList"; \
    hgsql -e \
    'LOAD DATA local INFILE "keggList.tab" into table keggList;' ${DB}Temp; \
}

#	hgKegg reads from ${DB}Temp.locus2Ref0, ${DB}Temp.locus2Acc0locus2Ref0,
#	${DB}Temp.keggList, ${DB}.knownGene
#	to create keggPathway.tab and keggMapDesc.tab
if [ ! -f keggPathway.tab ]; then
    echo "`date` running hgKegg ${DB}"
    hgKegg ${DB}
fi

TablePopulated "keggPathway" ${DB} || { \
    echo "`date` creating table keggPathway"; \
    hgsql -e "drop table keggPathway;" ${DB} 2> /dev/null; \
    hgsql ${DB} < ~/kent/src/hg/protein/keggPathway.sql; \
    echo "`date` loading keggPathway"; \
    hgsql -e \
    'LOAD DATA local INFILE "keggPathway.tab" into table keggPathway;' ${DB}; \
}

TablePopulated "keggMapDesc" ${DB} || { \
    echo "`date` creating table keggMapDesc"; \
    hgsql -e "drop table keggMapDesc;" ${DB} 2> /dev/null; \
    hgsql ${DB} < ~/kent/src/hg/protein/keggMapDesc.sql; \
    echo "`date` loading keggMapDesc"; \
    hgsql -e \
    'LOAD DATA local INFILE "keggMapDesc.tab" into table keggMapDesc;' ${DB}; \
}

if [ ! -f Mm_GeneData.dat ]; then
    echo "`date` fetching Mm_GeneData.dat from nci.nih.gov"
    wget --timestamping -O Mm_GeneData.dat \
	"ftp://ftp1.nci.nih.gov/pub/CGAP/Mm_GeneData.dat"
fi

#	hgCGAP reads Mm_GeneData.dat and creates a bunch of cgap*.tab files
if [ ! -f cgapAlias.tab ]; then
    echo "`date` running hgCGAP Mm_GeneData.dat"
    hgCGAP Mm_GeneData.dat
    cat cgapSEQUENCE.tab cgapSYMBOL.tab cgapALIAS.tab > cgapAlias.tab
fi

TablePopulated "cgapBiocPathway" ${DB} || { \
    echo "`date` creating table cgapBiocPathway"; \
    hgsql -e "drop table cgapBiocPathway;" ${DB} 2> /dev/null; \
    hgsql ${DB} < ~/kent/src/hg/hgCGAP/cgapBiocPathway.sql; \
    echo "`date` loading cgapBiocPathway"; \
    hgsql -e \
    'LOAD DATA local INFILE "cgapBIOCARTA.tab" into table cgapBiocPathway;' \
	${DB}; \
}

TablePopulated "cgapBiocDesc" ${DB} || { \
    echo "`date` creating table cgapBiocDesc"; \
    hgsql -e "drop table cgapBiocDesc;" ${DB} 2> /dev/null; \
    hgsql ${DB} < ~/kent/src/hg/hgCGAP/cgapBiocDesc.sql; \
    echo "`date` loading cgapBiocDesc"; \
    hgsql -e \
    'LOAD DATA local INFILE "cgapBIOCARTAdesc.tab" into table cgapBiocDesc;' \
	${DB}; \
}

TablePopulated "cgapAlias" ${DB} || { \
    echo "`date` creating table cgapAlias"; \
    hgsql -e "drop table cgapAlias;" ${DB} 2> /dev/null; \
    hgsql ${DB} < ~/kent/src/hg/hgCGAP/cgapAlias.sql; \
    echo "`date` loading cgapAlias"; \
    hgsql -e \
    'LOAD DATA local INFILE "cgapAlias.tab" into table cgapAlias;' ${DB}; \
}

echo "`date` DONE ========================="
