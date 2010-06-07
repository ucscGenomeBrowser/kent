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
#	"$Id: KG3.sh,v 1.1 2006/02/24 00:24:57 fanhsu Exp $"
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
	hgMrnaRefseq kgGetPep pslReps hgKgMrna kgPrepBestMrna2 spm3 spm7 \
	kgResultBestMrna2 rmKGPepMrna kgXref kgAliasM kgAliasP \
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

cd kgBestMrna

    rm -f iserverSetupOK
    echo "`date` Preparing iservers for kluster run"
    #	required destination format is: kgDB/${DB}/<whatever>
    ssh kkr1u00 ${ISERVER_SETUP} `pwd`/clusterRun kgDB/${DB}/kgBestMrna
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
echo "klusterRun completed."
exit 0
#	About 45 minutes of processing time to here

#	kgResultBestMrna processes the results of the cluster run
cd ${TOP}/kgBestMrna
if [ ! -s best.lis ]; then
	echo "`date` Assuming kgPrepBestMrna cluster run done."
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
    hgsql -e "ALTER TABLE spMrna ADD INDEX im2 (mrnaID);" ${DB}
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

TablePopulated "knownGene" ${DB} || {
    hgsql -e "drop table knownGene;" ${DB} 2> /dev/null; \
    hgsql ${DB} < ~/kent/src/hg/lib/knownGene.sql; \
    echo "`date` loading knownGene.tab into ${DB}.knownGene"; \
    hgsql -e 'LOAD DATA local INFILE "knownGene.tab" into table knownGene;' \
	${DB}; \
    rm -f sortedKnownGene.tab
}

#	dnaGne reads from ${DB}Temp.locus2Acc0, ${DB}Temp.locus2Ref0 and
#	${RO_DB}.refGene
#	to create dnaGene.tab and dnaLink.tab
if [ ! -s dnaLink.tab ]; then
    echo "`date` running dnaGene ${DB} ${PDB} ${RO_DB}"
    dnaGene ${DB} ${PDB} ${RO_DB}
    rm -f sortedKnownGene.tab
    hgsql -e "drop table knownGeneLink;" ${DB} 2> /dev/null; \
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

#	We need to add dnaGene.tab to knownGene
#	to make sure this is all done correctly, reload the entire
#	table with both, now sorted
if [ ! -s sortedKnownGene.tab ]; then
    hgsql -e "drop table knownGene;" ${DB} 2> /dev/null
    hgsql ${DB} < ~/kent/src/hg/lib/knownGene.sql
    ~/kent/src/hg/protein/sortKg.pl knownGene.tab dnaGene.tab > \
	sortedKnownGene.tab
    echo "`date` loading knownGene and dnaGene into ${DB}.knownGene"
    hgsql -e \
	'LOAD DATA local INFILE "sortedKnownGene.tab" into table knownGene;' \
	${DB}
fi

TablePopulated "knownGeneLink" ${DB} || { \
    hgsql -e "drop table knownGeneLink;" ${DB} 2> /dev/null; \
    hgsql ${DB} < ~/kent/src/hg/lib/knownGeneLink.sql; \
    echo "`date` loading dnaLink.tab into ${DB}.knownGeneLink"; \
    hgsql -e 'LOAD DATA local INFILE "dnaLink.tab" into table knownGeneLink;' \
	${DB}; \
}

TablePopulated "dupSpMrna" ${DB} || { \
    hgsql -e "drop table dupSpMrna;" ${DB} 2> /dev/null; \
    hgsql ${DB} < ~/kent/src/hg/lib/dupSpMrna.sql; \
    echo "`date` loading duplicate.tab into ${DB}.dupSpMrna"; \
    hgsql -e 'LOAD DATA local INFILE "duplicate.tab" into table dupSpMrna;' \
	${DB}; \
}

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
    kgXref ${DB} ${PDB} ${RO_DB}
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
    kgProtAlias ${DB} ${DATE}
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

if [ ! -s ${SPECIES}.lis ]; then
    wget --timestamping -O ${SPECIES}.html "http://www.genome.ad.jp/dbget-bin/www_bfind_sub?dbkey=pathway&keywords=${SPECIES}&mode=bfind&max_hit=1000&.cgifields=max_hit"
    grep HREF ${SPECIES}.html | perl -wpe "s/<[^>]+>//g" > ${SPECIES}.lis
    $HOME/kent/src/hg/protein/getKeggList.pl ${SPECIES} > keggList.tab
fi

if [ ! -s keggList.tab ]; then
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
if [ ! -s keggPathway.tab ]; then
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

cd ${TOP}

SPECIES=Hs
case ${RO_DB} in
    mm*) SPECIES=Mm
	;;
    rn*) echo "ERROR: can not load CGAP data for Rat"
	exit 255
	;;
    hg*) SPECIES=Hs
	;;
esac

if [ ! -s ${SPECIES}_GeneData.dat ]; then
    echo "`date` fetching ${SPECIES}_GeneData.dat from nci.nih.gov"
    wget --timestamping -O ${SPECIES}_GeneData.dat \
	"ftp://ftp1.nci.nih.gov/pub/CGAP/${SPECIES}_GeneData.dat"
    rm -f cgapAlias.tab
fi

#	hgCGAP reads GeneData.dat and creates a bunch of cgap*.tab files
if [ ! -s cgapAlias.tab ]; then
    echo "`date` running hgCGAP ${SPECIES}_GeneData.dat"
    hgCGAP ${SPECIES}_GeneData.dat
    cat cgapSEQUENCE.tab cgapSYMBOL.tab cgapALIAS.tab > cgapAlias.tab
    hgsql -e "drop table cgapBiocPathway;" ${DB} 2> /dev/null
    hgsql -e "drop table cgapBiocDesc;" ${DB} 2> /dev/null
    hgsql -e "drop table cgapAlias;" ${DB} 2> /dev/null
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

    cat cgapBIOCARTAdesc.tab|sort -u > cgapBIOCARTAdescSorted.tab
    hgsql -e \
    'LOAD DATA local INFILE "cgapBIOCARTAdescSorted.tab" into table cgapBiocDesc;' \
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
