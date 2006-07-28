#!/bin/sh
#		KGpath.sh
#	usage: KGpath <DB> <RO_DB> <YYMMDD>
#		<DB> - database to load, can be temporary
#		<RO_DB> - actual organism database to read other data from
#		<YYMMDD> - date stamp used to find protYYMMDD database
#	use a temporary test <DB> to verify correct operation
#
#	This script is used AFTER a new swissprot and proteins database
#	are created.  See also, scripts:
#	mkSwissProtDB.sh and mkProteinsDB.sh
#
#	"$Id: KGpath.sh,v 1.3 2006/07/28 01:49:32 baertsch Exp $"
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
    echo "usage: KGpath <DB> <RO_DB> <YYMMDD>"
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
#	src/hg/makeDb/hgKegg3

echo "`date` KGpath.sh $*"

foundALL=""
for i in hgsql kgXref rmKGPepMrna \
	/cluster/data/genbank/bin/i386/gbGetSeqs wget \
	hgMrnaRefseq kgGetPep pslReps hgKgMrna kgPrepBestMrna spm3 spm7 \
	kgResultBestMrna dnaGene rmKGPepMrna kgXref kgAliasM kgAliasP \
	kgProtAlias kgAliasKgXref kgAliasRefseq kgProtAliasNCBI \
	$HOME/kent/src/hg/protein/getKeggList.pl hgKegg3 hgCGAP
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
    grep -i HREF ${SPECIES}.html | perl -wpe "s/<[^>]+>//g" |sed -e 's/path:d/path:/g' > ${SPECIES}.lis
    $HOME/kent/src/hg/protein/getKeggList2.pl ${SPECIES} > keggList.tab
fi

if [ ! -s keggList.tab ]; then
    echo "ERROR: can not find keggList.tab"
    echo -e "\tShould have been created by getKeggList.pl operation"
    exit 255
fi

TablePopulated "keggList" ${DB}Temp || { \
    echo "`date` creating table keggList"; \
    hgsql -e "drop table keggList;" ${DB} 2> /dev/null; \
    hgsql ${DB} < ~/kent/src/hg/lib/keggList.sql; \
    echo "`date` loading keggList"; \
    hgsql -e \
    'LOAD DATA local INFILE "keggList.tab" into table keggList;' ${DB}; \
}

#	hgKegg3 reads from ${DB}Temp.locus2Ref0, ${DB}Temp.locus2Acc0locus2Ref0,
#	${DB}Temp.keggList, ${DB}.knownGene
#	to create keggPathway.tab and keggMapDesc.tab
if [ ! -s keggPathway.tab ]; then
    echo "`date` running hgKegg3 ${DB}"
    hgKegg3 ${DB} ${RO_DB}
fi

TablePopulated "keggPathway" ${DB} || { \
    echo "`date` creating table keggPathway"; \
    hgsql -e "drop table keggPathway;" ${DB} 2> /dev/null; \
    hgsql ${DB} < ~/kent/src/hg/lib/keggPathway.sql; \
    echo "`date` loading keggPathway"; \
    hgsql -e \
    'LOAD DATA local INFILE "keggPathway.tab" into table keggPathway;' ${DB}; \
}

TablePopulated "keggMapDesc" ${DB} || { \
    echo "`date` creating table keggMapDesc"; \
    hgsql -e "drop table keggMapDesc;" ${DB} 2> /dev/null; \
    hgsql ${DB} < ~/kent/src/hg/lib/keggMapDesc.sql; \
    echo "`date` loading keggMapDesc"; \
    hgsql -e \
    'LOAD DATA local INFILE "keggMapDesc.tab" into table keggMapDesc;' ${DB}; \
}
echo "KEGG pathways done."
exit 0
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
