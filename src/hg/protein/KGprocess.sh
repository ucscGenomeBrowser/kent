#!/bin/sh
#		KGprocess.sh
#	usage: KGprocess <YYMMDD>
#		<YYMMDD> - date stamp used to find proteinsYYMMDD database
#	(A second argument needs to be added to specify organism DB)
#
#	This script is used AFTER a new swissprot and proteins database
#	are created.  See also, scripts:
#	mkSwissProtDB.sh and mkProteinsDB.sh
#
#	"$Id: KGprocess.sh,v 1.1 2003/11/20 19:37:44 hiram Exp $"
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

echo "Add second argument or fixup DB name below to operate this script"
exit 0

DB=mm4
TOP=/cluster/data/${DB}/bed/knownGenes
export DB TOP

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

if [ "$#" -ne 1 ]; then
    echo "usage: KGprocess <YYMMDD>"
    echo -e "\t<YYMMDD> - date stamp used to find proteinsYYMMDD DB"
    exit 255
fi

#	check for all binaries that will be used here

foundALL=""
for i in hgsql kgXref rmKGPepMrna \
	/cluster/data/genbank/bin/i386/gbGetSeqs wget \
	hgMrnaRefseq kgGetPep pslReps hgKgMrna kgPrepBestMrna spm3 spm7 \
	kgResultBestMrna dnaGene rmKGPepMrna kgXref kgAliasM kgAliasM \
	kgProtAlias kgProtAliasNCBI ./getKeggList.pl hgKegg hgCGAP
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

DATE=$1
PDB=proteins${DATE}
export DATE PDB

IS_THERE=`hgsql -e "show tables;" ${PDB} | wc -l`

if [ ${IS_THERE} -lt 10 ]; then
	echo "ERROR: can not find database: ${PDB}"
	echo -e "\tcurrently existing protein databases:"
	hgsql -e "show databases;" mysql | grep -y protein
	exit 255
fi

echo "using protein database: ${PDB}"

if [ ! -d ${TOP} ]; then
	mkdir ${TOP}
fi

if [ ! -d ${TOP} ]; then
	exit "Can not create ${TOP}"
	exit 255
fi

cd ${TOP}

if [ ! -f mrna.fa ]; then
    /cluster/data/genbank/bin/i386/gbGetSeqs -native -db=${DB} \
	-gbRoot=/cluster/data/genbank genbank mrna mrna.fa
fi

if [ ! -f mrna.ra ]; then
    /cluster/data/genbank/bin/i386/gbGetSeqs -get=ra -native -db=${DB} \
	-gbRoot=/cluster/data/genbank genbank mrna mrna.ra
fi

if [ ! -f all_mrna.psl ]; then
    /cluster/data/genbank/bin/i386/gbGetSeqs -get=psl -native -db=${DB} \
	-gbRoot=/cluster/data/genbank genbank mrna all_mrna.psl
fi

if [ ! -f mrna.lis ]; then
    grep "^>" mrna.fa > mrna.lis
fi

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
    hgsql -e "drop table history;" ${DB}Temp 2> /dev/null

    echo "loading loc2acc and loc2ref into ${DB}Temp"

    hgsql -e 'LOAD DATA local INFILE "loc2acc" into table locus2Acc0;' ${DB}Temp
    hgsql -e 'LOAD DATA local INFILE "loc2ref" into table locus2Ref0;' ${DB}Temp
    cd ${TOP}
fi

if [ ! -f mrnaRefseq.tab ]; then
    hgMrnaRefseq ${DB}
    hgsql -e "drop table mrnaRefseq;" ${DB} 2> /dev/null

    hgsql -e 'CREATE TABLE mrnaRefseq (
	mrna varchar(40) NOT NULL default "",
	refseq varchar(40) NOT NULL default "",
	KEY mrna (mrna),
	KEY refseq (refseq)
	) TYPE=MyISAM;' ${DB}
 hgsql -e 'LOAD DATA local INFILE "mrnaRefseq.tab" into table mrnaRefseq;' ${DB}
fi

if [ ! -f mrnaPep.fa ]; then
    kgGetPep ${DATE} > mrnaPep.fa
fi

if [ ! -f tight_mrna.psl ]; then
    pslReps -minCover=0.40 -sizeMatters -minAli=0.97 -nearTop=0.002 \
	all_mrna.psl tight_mrna.psl /dev/null
fi

if [ ! -f ${DB}KgMrna.out ]; then
    echo "running: hgKgMrna"
    hgKgMrna ${DB}Temp mrna.fa mrna.ra tight_mrna.psl ll/loc2ref \
	mrnaPep.fa ll/mim2loc ${PDB} > ${DB}KgMrna.out 2> ${DB}KgMrna.err
fi

for T in refGene refMrna refPep
do
    if [ ! -f ${T}.tab ]; then
	echo "ERROR: can not find file: ${T}.tab"
	echo -e "\tShould have been created by hgKgMrna"
	exit 255
    fi
done


IS_THERE=`hgsql -e "select count(*) from refGene;" ${DB}Temp | tail -1`
if [ "${IS_THERE}" -eq 0 ]; then
    echo "loading refGene.tab"
    hgsql -e 'LOAD DATA local INFILE "refGene.tab" into table refGene;' \
	${DB}Temp
fi

IS_THERE=`hgsql -e "select count(*) from refMrna;" ${DB}Temp | tail -1`
if [ "${IS_THERE}" -eq 0 ]; then
    echo "loading refMrna.tab"
    hgsql -e 'LOAD DATA local INFILE "refMrna.tab" into table refMrna;' \
	${DB}Temp
fi

#	WARNING: .tab file names do not match DB table names
TablePopulated "knownGenePep" ${DB} || { \
    hgsql -e "drop table knownGenePep;" ${DB} 2> /dev/null; \
    hgsql ${DB} < ~/kent/src/hg/lib/knownGenePep.sql; \
    hgsql -e \
	'LOAD DATA local INFILE "refPep.tab" into table knownGenePep;' ${DB}; \
}

TablePopulated "knownGeneMrna" ${DB} || { \
    hgsql -e "drop table knownGeneMrna;" ${DB} 2> /dev/null; \
    hgsql ${DB} < ~/kent/src/hg/lib/knownGeneMrna.sql; \
    hgsql -e \
     'LOAD DATA local INFILE "refMrna.tab" into table knownGeneMrna;' ${DB}; \
}



if [ ! -f proteinMrna.tab ]; then
    spm3 ${DATE} ${DB}
fi

if [ ! -f proteinMrna.tab ]; then
	echo "ERROR: can not find file: proteinMrna.tab"
	echo -e "\tShould have been created by spm3"
	exit 255
fi

TablePopulated "spMrna" ${DB}Temp || { \
    echo "loading proteinMrna.tab into ${DB}Temp.spMrna" 2> /dev/null; \
    hgsql -e 'LOAD DATA local INFILE "proteinMrna.tab" into table spMrna;' \
	${DB}Temp; \
}

if [ ! -d kgBestMrna ]; then
	mkdir kgBestMrna
fi

cd kgBestMrna

ln -s ../protein.lis . 2> /dev/null

if [ ! -d clusterRun ]; then
	echo "Preparing cluster run data and jobList"
	kgPrepBestMrna ${DATE} ${DB} 2> jobList > Prep.out
	echo "Cluster Run has been prepared."
	echo "on machine kk in: "`pwd`
	echo "perform:"
	echo "para create jobList"
	echo "para try"
	echo "para push ... etc."
	exit 255
fi

if [ ! -f best.lis ]; then
	echo "Assuming cluster run done, Running analysis of output."
	kgResultBestMrna ${DATE} ${DB} > ResultBest.out 2>&1
fi

BEST_LEN=`cat best.lis | wc -l`
if [ "${BEST_LEN}" -lt 1000 ]; then
	echo "ERROR: do not find correct results from kgResultBestMrna"
	echo "ERROR: BEST_LEN: $BEST_LEN"
	exit 255
fi

TablePopulated "spMrna" ${DB} || { \
    hgsql -e "drop table spMrna;" ${DB} 2> /dev/null; \
    hgsql ${DB} < ~/kent/src/hg/lib/spMrna.sql; \
    echo "loading proteinMrna.tab into ${DB}.spMrna"; \
    hgsql -e 'LOAD DATA local INFILE "best.lis" into table spMrna;' ${DB}; \
}

if [ ! -f knownGene0.tab ]; then
    echo "running spm6 ${DATE} ${DB}"
    spm6 ${DATE} ${DB}
fi

TablePopulated "knownGene0" ${DB}Temp || { \
    echo "loading knownGene0.tab into ${DB}Temp.knownGene0"; \
    hgsql -e 'LOAD DATA local INFILE "knownGene0.tab" into table knownGene0;' \
	${DB}Temp; \
}

if [ ! -f knownGene.tab ]; then
    echo "running spm7 ${DATE} ${DB}"
    spm7 ${DATE} ${DB} > spm7.out
fi

TablePopulated "dupSpMrna" ${DB} || { \
    hgsql -e "drop table dupSpMrna;" ${DB} 2> /dev/null; \
    hgsql ${DB} < ~/kent/src/hg/lib/dupSpMrna.sql; \
    echo "loading duplicate.tab into ${DB}.dupSpMrna"; \
    hgsql -e 'LOAD DATA local INFILE "duplicate.tab" into table dupSpMrna;' \
	${DB}; \
}

if [ ! -f dnaLink.tab ]; then
    echo "running dnaGene ${DB} ${DATE}"
    dnaGene ${DB} proteins${DATE}
fi

if [ ! -f dnaGene.tab ]; then
    echo "ERROR: can not find dnaGene.tab"
    echo -e "\tShould have been created by dnaGene operation"
    exit 255
fi


TablePopulated "knownGeneLink" ${DB} || { \
    hgsql -e "drop table knownGeneLink;" ${DB} 2> /dev/null; \
    hgsql ${DB} < ~/kent/src/hg/lib/knownGeneLink.sql; \
    echo "loading dnaLink.tab into ${DB}.knownGeneLink"; \
    hgsql -e 'LOAD DATA local INFILE "dnaLink.tab" into table knownGeneLink;' \
	${DB}; \
}

TablePopulated "knownGene" ${DB} || {
    hgsql -e "drop table knownGene;" ${DB} 2> /dev/null; \
    hgsql ${DB} < ~/kent/src/hg/lib/knownGene.sql; \
    echo "loading knownGene.tab into ${DB}.knownGene"; \
    hgsql -e 'LOAD DATA local INFILE "knownGene.tab" into table knownGene;' \
	${DB}; \
    hgsql -e "select count(*) from knownGene;" ${DB}; \
    echo "loading dnaGene.tab into ${DB}.knownGene"; \
    hgsql -e 'LOAD DATA local INFILE "dnaGene.tab" into table knownGene;' \
	${DB}; \
    hgsql -e "select count(*) from knownGene;" ${DB}; \
}

cd ${TOP}

if [ ! -f knownGenePep.tab ]; then
    echo "running rmKGPepMrna ${DB} ${DATE}"
    rmKGPepMrna ${DB} ${DATE}
fi

if [ ! -f knownGeneMrna.tab ]; then
    echo "ERROR: can not find knownGeneMrna.tab"
    echo -e "\tShould have been created by rmKGPepMrna operation"
    exit 255
fi

if [ ! -f loadedPepMrna ]; then
    hgsql -e "drop table knownGenePep;" ${DB} 2> /dev/null
    hgsql ${DB} < ~/kent/src/hg/lib/knownGenePep.sql
    echo "loading knownGenePep"
    hgsql -e \
    'LOAD DATA local INFILE "knownGenePep.tab" into table knownGenePep;' ${DB}

    hgsql -e "drop table knownGeneMrna;" ${DB} 2> /dev/null
    hgsql ${DB} < ~/kent/src/hg/lib/knownGeneMrna.sql
    echo "loading knownGeneMrna"
    hgsql -e \
    'LOAD DATA local INFILE "knownGeneMrna.tab" into table knownGeneMrna;' ${DB}
    touch loadedPepMrna
fi

if [ ! -f kgXref.tab ]; then
    echo "running kgXref"
    kgXref ${DB} ${PDB}
fi

TablePopulated "kgXref" ${DB} || { \
    hgsql -e "drop table kgXref;" ${DB} 2> /dev/null; \
    hgsql ${DB} < ~/kent/src/hg/lib/kgXref.sql; \
    echo "loading kgXref"; \
    hgsql -e \
    'LOAD DATA local INFILE "kgXref.tab" into table kgXref;' ${DB}; \
}

if [ ! -f kgAliasM.tab ]; then
    echo "running kgAliasM"
    kgAliasM ${DB} ${PDB}
fi

if [ ! -f kgAlias.tab ]; then
    echo "running kgAliasP sp.lis"
    kgAliasP ${DB} /cluster/data/swissprot/${DATE}/build/sprot.dat sp.lis
    echo "running kgAliasP tr.lis"
    kgAliasP ${DB} /cluster/data/swissprot/${DATE}/build/trembl.dat tr.lis
    echo "running kgAliasP new.lis"
    kgAliasP ${DB} /cluster/data/swissprot/${DATE}/build/trembl_new.dat new.lis
    cat sp.lis tr.lis new.lis | sort | uniq > kgAliasP.tab
fi

TablePopulated "kgAlias" ${DB} || { \
    echo "creating table kgAlias"; \
    hgsql -e "drop table kgAlias;" ${DB} 2> /dev/null; \
    hgsql ${DB} < ~/kent/src/hg/lib/kgAlias.sql; \
    hgsql -e \
    'LOAD DATA local INFILE "kgAlias.tab" into table kgAlias;' ${DB}; \
}

if [ ! -f kgProtAliasBoth.tab ]; then
    echo "running kgProtAlias"
    kgProtAlias ${DB} ${PDB}
    echo "running kgProtAliasNCBI"
    kgProtAliasNCBI ${DB}
    cat kgProtAliasNCBI.tab kgProtAlias.tab | sort | uniq > kgProtAliasBoth.tab
    rm kgProtAliasNCBI.tab kgProtAlias.tab
fi

TablePopulated "kgProtAlias" ${DB} || { \
    echo "creating table kgProtAlias"; \
    hgsql -e "drop table kgProtAlias;" ${DB} 2> /dev/null; \
    hgsql ${DB} < ~/kent/src/hg/lib/kgProtAlias.sql; \
    hgsql -e \
    'LOAD DATA local INFILE "kgProtAliasBoth.tab" into table kgProtAlias;' \
	${DB}; \
}

if [ ! -f mmu.lis ]; then
    wget --timestamping -O mmu.lis "http://www.genome.ad.jp/dbget-bin/www_bfind_sub?dbkey=pathway&keywords=mmu&mode=bfind&max_hit=1000&.cgifields=max_hit"
    ./getKeggList.pl mmu
fi

TablePopulated "keggList" ${DB}Temp || { \
    echo "loading keggList"; \
    hgsql -e \
    'LOAD DATA local INFILE "keggList.tab" into table keggList;' ${DB}Temp; \
}

if [ ! -f keggPathway.tab ]; then
    echo "running hgKegg ${DB}"
    hgKegg ${DB}
fi

TablePopulated "keggPathway" ${DB} || { \
    echo "creating table keggPathway"; \
    hgsql -e "drop table keggPathway;" ${DB} 2> /dev/null; \
    hgsql ${DB} < ~/kent/src/hg/protein/keggPathway.sql; \
    echo "loading keggPathway"; \
    hgsql -e \
    'LOAD DATA local INFILE "keggPathway.tab" into table keggPathway;' ${DB}; \
}

TablePopulated "keggMapDesc" ${DB} || { \
    echo "creating table keggMapDesc"; \
    hgsql -e "drop table keggMapDesc;" ${DB} 2> /dev/null; \
    hgsql ${DB} < ~/kent/src/hg/protein/keggMapDesc.sql; \
    echo "loading keggMapDesc"; \
    hgsql -e \
    'LOAD DATA local INFILE "keggMapDesc.tab" into table keggMapDesc;' ${DB}; \
}

if [ ! -f Mm_GeneData.dat ]; then
    echo "fetching Mm_GeneData.dat from nci.nih.gov"
    wget --timestamping -O Mm_GeneData.dat \
	"ftp://ftp1.nci.nih.gov/pub/CGAP/Mm_GeneData.dat"
fi

if [ ! -f cgapAlias.tab ]; then
    echo "running hgCGAP Mm_GeneData.dat"
    hgCGAP Mm_GeneData.dat
    cat cgapSEQUENCE.tab cgapSYMBOL.tab cgapALIAS.tab > cgapAlias.tab
fi

TablePopulated "cgapBiocPathway" ${DB} || { \
    echo "creating table cgapBiocPathway"; \
    hgsql -e "drop table cgapBiocPathway;" ${DB} 2> /dev/null; \
    hgsql ${DB} < ~/kent/src/hg/hgCGAP/cgapBiocPathway.sql; \
    echo "loading cgapBiocPathway"; \
    hgsql -e \
    'LOAD DATA local INFILE "cgapBIOCARTA.tab" into table cgapBiocPathway;' \
	${DB}; \
}

TablePopulated "cgapBiocDesc" ${DB} || { \
    echo "creating table cgapBiocDesc"; \
    hgsql -e "drop table cgapBiocDesc;" ${DB} 2> /dev/null; \
    hgsql ${DB} < ~/kent/src/hg/hgCGAP/cgapBiocDesc.sql; \
    echo "loading cgapBiocDesc"; \
    hgsql -e \
    'LOAD DATA local INFILE "cgapBIOCARTAdesc.tab" into table cgapBiocDesc;' \
	${DB}; \
}

TablePopulated "cgapAlias" ${DB} || { \
    echo "creating table cgapAlias"; \
    hgsql -e "drop table cgapAlias;" ${DB} 2> /dev/null; \
    hgsql ${DB} < ~/kent/src/hg/hgCGAP/cgapAlias.sql; \
    echo "loading cgapAlias"; \
    hgsql -e \
    'LOAD DATA local INFILE "cgapAlias.tab" into table cgapAlias;' ${DB}; \
}

