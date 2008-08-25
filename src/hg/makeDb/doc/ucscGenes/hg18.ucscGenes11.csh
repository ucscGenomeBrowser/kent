#!/bin/tcsh -efx
# :vim nowrap
# for emacs: -*- mode: sh; -*-
# This describes how to make the UCSC genes on hg18, though
# hopefully by editing the variables that follow immediately
# this will work on other databases too.

# Directories
set dir = /cluster/data/hg18/bed/ucsc.11
set scratchDir = /san/sanvol1/scratch

# Databases
set db = hg18
set xdb = mm9
set Xdb = Mm9
set ydb = canFam2
set zdb = rheMac2
set spDb = sp080707
set pbDb = proteins080707
set ratDb = rn4
set RatDb = Rn4
set fishDb = danRer4
set flyDb = dm3
set wormDb = ce4
set yeastDb = sacCer1

# If rebuilding on an existing assembly make tempDb some bogus name like tmpFoo2, otherwise 
# make tempDb same as db.
set tempDb = tmpFoo2
set bioCycTempDb = tmpBioCyc$db

# Table for SNPs
set snpTable = snp129

# Public version number
set lastVer = 3
set curVer = 4


# Database to rebuild visiGene text from.  Should include recent mouse and human
# but not the one you're rebuilding if you're rebuilding. (Use tempDb instead).
set vgTextDbs = (mm8 mm9 hg17 $tempDb)

# Proteins in various species
set tempFa = $dir/ucscGenes.faa
set xdbFa = /cluster/data/$xdb/bed/ucsc.10/ucscGenes.faa
set ratFa = /cluster/data/$ratDb/bed/blastp/known.faa
set fishFa = /cluster/data/$fishDb/bed/blastp/ensembl.faa
set flyFa = /cluster/data/$flyDb/bed/flybase5.3/flyBasePep.fa
set wormFa = /cluster/data/$wormDb/bed/blastp/wormPep170.faa
set yeastFa = /cluster/data/$yeastDb/bed/blastp/sgdPep.faa

# Other files needed
  # For bioCyc pathways - best to update these following build instructions in
  # hg18.txt
set bioCycPathways = /cluster/data/hg18/bed/ucsc.10/bioCyc/pathways.col
set bioCycGenes = /cluster/data/hg18/bed/ucsc.10/bioCyc/genes.col
   # For KEGG - update this following build instructions in hg18.txt
set keggList = /cluster/data/hg18/bed/ucsc.10/kegg/keggList.tab


# Tracks
set multiz = multiz28way

# NCBI Taxon
set taxon = 9606

# Previous gene set
set oldGeneBed = /cluster/data/hg18/bed/ucsc.10/ucscGenes.bed

# Machines
set dbHost = hgwdev
set ramFarm = memk
set cpuFarm = pk

# Create initial dir
mkdir -p $dir
cd $dir

if (0) then  # BRACKET
endif # BRACKET


# Get Genbank info
txGenbankData $db

# process RA Files
txReadRa mrna.ra refSeq.ra .

# Get some other info from the database.  Best to get it about
# the same time so it is synced with other data. Takes 4 seconds.
hgsql -N $db -e 'select distinct name,sizePolyA from mrnaOrientInfo' | \
	subColumn -miss=sizePolyA.miss 1 stdin accVer.tab sizePolyA.tab

# Get CCDS for human (or else just an empty file)
if ($db =~ hg*) then
     hgsql -N $db -e "select name,chrom,strand,txStart,txEnd,cdsStart,cdsEnd,exonCount,exonStarts,exonEnds from ccdsGene" | genePredToBed > ccds.bed
     hgsql -N $db -e "select mrnaAcc,ccds from ccdsInfo where srcDb='N'" > refToCcds.tab
else
     echo -n "" > ccds.bed
     echo -n "" > refToCcds.tab
endif
   
# Create directories full of alignments split by chromosome.
mkdir -p est refSeq mrna
pslSplitOnTarget refSeq.psl refSeq
pslSplitOnTarget mrna.psl mrna
bedSplitOnChrom ccds.bed ccds
foreach c (`awk '{print $1;}' /cluster/data/$db/chrom.sizes`)
    if (! -e refSeq/$c.psl) then
	  echo creating empty refSeq/$c.psl
          echo -n "" >refSeq/$c.psl
    endif
    if (! -e mrna/$c.psl) then
	  echo creating empty mrna/$c.psl
          echo -n "" >mrna/$c.psl
    endif
    hgGetAnn -noMatchOk $db intronEst $c est/$c.psl
    if (! -e est/$c.psl) then
	  echo creating empty est/$c.psl
          echo -n "" >est/$c.psl
    endif
    if (! -e ccds/$c.bed) then
	  echo creating empty ccds/$c.bed
          echo -n "" >ccds/$c.bed
    endif
end

# Get list of accessions that are associated with antibodies from database.
# This will be a good list but not 100% complete.  Cluster these to get
# four or five antibody heavy regions.  Later we'll weed out input that
# falls primarily in these regions, and, include the regions themselves
# as special genes.  Takes 40 seconds
txAbFragFind $db antibodyAccs
pslCat mrna/*.psl -nohead | fgrep -w -f antibodyAccs > antibody.psl
clusterPsl -prefix=ab.ab.antibody. antibody.psl antibody.cluster
if ($db =~ mm*) then
    echo chr12 > abChrom.lst
    echo chr16 >> abChrom.lst
    echo chr6 >> abChrom.lst
    fgrep -w -f abChrom.lst antibody.cluster | cut -f 1-12 | bedOrBlocks stdin antibody.bed
else
    awk '$13 > 100' antibody.cluster | cut -f 1-12 > antibody.bed
endif

# Convert psls to bed, saving mapping info and weeding antibodies.  Takes 2.5 min
foreach c (`awk '{print $1;}' /cluster/data/$db/chrom.sizes`)
    if (-s refSeq/$c.psl) then
	txPslToBed refSeq/$c.psl -noFixStrand -cds=cds.tab /cluster/data/$db/$db.2bit refSeq/$c.bed -unusual=refSeq/$c.unusual
    else
	echo "creating empty refSeq/$c.bed"
	touch refSeq/$c.bed
    endif
    if (-s mrna/$c.psl) then
	txPslToBed mrna/$c.psl -cds=cds.tab /cluster/data/$db/$db.2bit \
		stdout -unusual=mrna/$c.unusual \
	    | bedWeedOverlapping antibody.bed maxOverlap=0.5 stdin mrna/$c.bed
    else
	echo "creating empty mrna/$c.bed"
	touch mrna/$c.bed
    endif
    if (-s est/$c.psl) then
	txPslToBed est/$c.psl /cluster/data/$db/$db.2bit stdout \
	    | bedWeedOverlapping antibody.bed maxOverlap=0.3 stdin est/$c.bed
    else
	echo "creating empty est/$c.bed"
	touch est/$c.bed
    endif
end



#  seven minutes to this point

# Create mrna splicing graphs.  Takes 10 seconds.
mkdir -p bedToGraph
foreach c (`awk '{print $1;}' /cluster/data/$db/chrom.sizes`)
    txBedToGraph -prefix=$c. ccds/$c.bed ccds refSeq/$c.bed refSeq mrna/$c.bed mrna \
	bedToGraph/$c.txg
end


# Create est splicing graphs.  Takes 6 minutes.
foreach c (`awk '{print $1;}' /cluster/data/$db/chrom.sizes`)
    txBedToGraph -prefix=e$c. est/$c.bed est est/$c.txg
end



# Create an evidence weight file
cat > trim.weights <<end
refSeq  100
ccds    50
mrna    2
txOrtho 1
exoniphy 1
est 1
end

# Make evidence file for EST graph edges supported by at least 2 
# ests.  Takes about 30 seconds.
foreach c (`awk '{print $1;}' /cluster/data/$db/chrom.sizes`)
    txgGoodEdges est/$c.txg  trim.weights 2 est est/$c.edges
end

# Setup other species dir
mkdir -p $xdb
cd $xdb

# Get other species mrna including ESTs.  Takes about three minutes
mkdir -p refSeq mrna est
foreach c (`awk '{print $1;}' /cluster/data/$xdb/chrom.sizes`)
    echo $c
    hgGetAnn -noMatchOk $xdb refSeqAli $c stdout | txPslToBed stdin /cluster/data/$xdb/$xdb.2bit refSeq/$c.bed 
    hgGetAnn -noMatchOk $xdb mrna $c stdout | txPslToBed stdin /cluster/data/$xdb/$xdb.2bit mrna/$c.bed
    hgGetAnn -noMatchOk $xdb intronEst $c stdout | txPslToBed stdin /cluster/data/$xdb/$xdb.2bit est/$c.bed
end
#ignore gripe about missing data in chrM

# Create other species splicing graphs.  Takes a minute and a half.
rm -f other.txg
foreach c (`awk '{print $1;}' /cluster/data/$xdb/chrom.sizes`)
    echo $c
    txBedToGraph refSeq/$c.bed refSeq mrna/$c.bed mrna est/$c.bed est stdout >> other.txg
end


# Clean up all but final other.txg
rm -r est mrna refSeq

# Unpack chains and nets, apply synteny filter and split by chromosome
# Takes 5 minutes.  Make up phony empty nets for ones that are empty after
# synteny filter.
cd $dir/$xdb
zcat /cluster/data/$db/bed/blastz.$xdb/axtChain/$db.$xdb.all.chain.gz | chainSplit chains stdin
zcat /cluster/data/$db/bed/blastz.$xdb/axtChain/$db.$xdb.net.gz | netFilter -syn stdin | netSplit stdin nets
cd nets
foreach c (`awk '{print $1;}' /cluster/data/$db/chrom.sizes`)
    if (! -e $c.net) then
        echo -n > $c.net
    endif
end

# Make txOrtho directory and a para spec file
cd $dir
mkdir -p txOrtho/edges
cd txOrtho
echo '#LOOP' > template
echo './runTxOrtho $(path1) '"$xdb $db" >> template
echo '#ENDLOOP' >> template
    
cat << '_EOF_' > runTxOrtho
#!/bin/csh -ef
set inAgx = ../bedToGraph/$1.txg
set inChain = ../$2/chains/$1.chain
set inNet = ../$2/nets/$1.net
set otherTxg = ../$2/other.txg
set tmpDir = /scratch/tmp/$3
set workDir = $tmpDir/${1}_${2}
mkdir -p $tmpDir
mkdir $workDir
txOrtho $inAgx $inChain $inNet $otherTxg $workDir/$1.edges
mv $workDir/$1.edges edges/$1.edges
rmdir $workDir
rmdir --ignore-fail-on-non-empty $tmpDir
'_EOF_'
    #	<< happy emacs
    chmod +x runTxOrtho
    touch toDoList
cd $dir/bedToGraph
foreach f (*.txg)
    set c=$f:r
    if ( -s $f ) then
	echo $c >> ../txOrtho/toDoList
    else
	echo "warning creating empty $c.edges result"
	touch ../txOrtho/edges/$c.edges
    endif
end
cd ..

# Do txOrtho parasol run on iServer (high RAM) cluster
ssh $ramFarm "cd $dir/txOrtho; gensub2 toDoList single template jobList"
ssh $ramFarm "cd $dir/txOrtho; para make jobList"
ssh $ramFarm "cd $dir/txOrtho; para time > run.time"
cat txOrtho/run.time
# Completed: 47 of 47 jobs
# CPU time in finished jobs:       1897s      31.62m     0.53h    0.02d  0.000 y
# IO & Wait Time:                   395s       6.58m     0.11h    0.00d  0.000 y
# Average job time:                  49s       0.81m     0.01h    0.00d
# Longest finished job:             296s       4.93m     0.08h    0.00d
# Submission to last job:           296s       4.93m     0.08h    0.00d

# Filter out some duplicate edges. These are legitimate from txOrtho's point
# of view, since they represent two different mouse edges both supporting
# a human edge. However, from the human point of view we want only one
# support from mouse orthology.  Just takes a second.
cd txOrtho
mkdir -p uniqEdges
foreach c (`awk '{print $1;}' /cluster/data/$db/chrom.sizes`)
    cut -f 1-9 edges/$c.edges | sort | uniq > uniqEdges/$c.edges
end
cd ..

# Clean up chains and nets since they are big
cd $dir
rm -r $xdb/chains $xdb/nets

# Get exonophy. Takes about 4 seconds.
hgsql -N $db -e "select chrom, txStart, txEnd, name, id, strand from exoniphy order by chrom, txStart;" \
    > exoniphy.bed
bedToTxEdges exoniphy.bed exoniphy.edges

# Add evidence from ests, orthologous other species transcripts, and exoniphy
# Takes 36 seconds.
mkdir -p graphWithEvidence
foreach c (`awk '{print $1;}' /cluster/data/$db/chrom.sizes`)
    echo adding evidence for $c
    txgAddEvidence -chrom=$c bedToGraph/$c.txg exoniphy.edges exoniphy stdout \
	   | txgAddEvidence stdin txOrtho/uniqEdges/$c.edges txOrtho stdout \
	   | txgAddEvidence stdin est/$c.edges est graphWithEvidence/$c.txg
end

# Do  txWalk  - takes 32 seconds (mostly loading the mrnaSize.tab again and
# again...)
mkdir -p txWalk
if ($db =~ mm*) then
    set sef = 0.6
else
    set sef = 0.75
endif
foreach c (`awk '{print $1;}' /cluster/data/$db/chrom.sizes`)
    txWalk graphWithEvidence/$c.txg trim.weights 3 txWalk/$c.bed \
	-evidence=txWalk/$c.ev -sizes=mrnaSize.tab -defrag=0.25 \
	    -singleExonFactor=$sef
end


# Make a file that lists the various categories of alt-splicing we see.
# Do this by making and analysing splicing graphs of just the transcripts
# that have passed our process so far.  The txgAnalyze program occassionally
# will make a duplicate, which is the reason for the sort/uniq run.
# Takes 7 seconds.
cat txWalk/*.bed > txWalk.bed
txBedToGraph txWalk.bed txWalk txWalk.txg
txgAnalyze txWalk.txg /cluster/data/$db/$db.2bit stdout | sort | uniq > altSplice.bed


# Get txWalk transcript sequences.  This'll take about 2 minutes
#	This appears to take a significant amount of time.  Each chrom
#	takes about 2 minutes or more
rm -f txWalk.fa
foreach c (`awk '{print $1;}' /cluster/data/$db/chrom.sizes`)
    sequenceForBed -db=$db -bedIn=txWalk/$c.bed -fastaOut=stdout -upCase -keepName >> txWalk.fa
end
rm -rf txFaSplit
mkdir -p txFaSplit
faSplit sequence txWalk.fa 200 txFaSplit/


# Fetch human protein set and table that describes if curated or not.
# Takes about a minute
hgsql -N $spDb -e \
  "select p.acc, p.val from protein p, accToTaxon x where x.taxon=$taxon and p.acc=x.acc" \
  | awk '{print ">" $1;print $2}' >uniProt.fa
hgsql -N $spDb -e "select i.acc,i.isCurated from info i,accToTaxon x where x.taxon=$taxon and i.acc=x.acc" > uniCurated.tab


echo "continuing after first kki job"
echo "preparing $cpuFarm job"

mkdir -p blat/rna/raw
echo '#LOOP' > blat/rna/template
echo './runTxBlats $(path1) '"$db"' $(root1) {check out line+ raw/ref_$(root1).psl}' >> blat/rna/template
echo '#ENDLOOP' >> blat/rna/template
 
cat << '_EOF_' > blat/rna/runTxBlats
#!/bin/csh -ef
set ooc = /cluster/data/$2/11.ooc
set target = ../../txFaSplit/$1
set out1 = raw/mrna_$3.psl
set out2 = raw/ref_$3.psl
set tmpDir = /scratch/tmp/$2/ucscGenes/rna
set workDir = $tmpDir/$3
mkdir -p $tmpDir
mkdir $workDir
blat -ooc=$ooc -minIdentity=95 $target ../../mrna.fa $workDir/mrna_$3.psl
blat -ooc=$ooc -minIdentity=97 $target ../../refSeq.fa $workDir/ref_$3.psl
mv $workDir/mrna_$3.psl raw/mrna_$3.psl
mv $workDir/ref_$3.psl raw/ref_$3.psl
if (-e $workDir) then
    rmdir $workDir
endif
rmdir --ignore-fail-on-non-empty $tmpDir
'_EOF_'
    #	<< happy emacs
chmod +x blat/rna/runTxBlats
cd txFaSplit
ls -1 *.fa > ../blat/rna/toDoList
cd ..

ssh $cpuFarm "cd $dir/blat/rna; gensub2 toDoList single template jobList"

ssh $cpuFarm "cd $dir/blat/rna; para make jobList"
ssh $cpuFarm "cd $dir/blat/rna; para time > run.time"
cat blat/rna/run.time

# Completed: 194 of 194 jobs
# CPU time in finished jobs:      23790s     396.50m     6.61h    0.28d  0.001 y
# IO & Wait Time:                   604s      10.06m     0.17h    0.01d  0.000 y
# Average job time:                 126s       2.10m     0.03h    0.00d
# Longest running job:                0s       0.00m     0.00h    0.00d
# Longest finished job:            1078s      17.97m     0.30h    0.01d
# Submission to last job:          1354s      22.57m     0.38h    0.02d

# Set up blat jobs for proteins vs. translated txWalk transcripts
mkdir -p blat/protein/raw
echo '#LOOP' > blat/protein/template
echo './runTxBlats $(path1) '"$db"' $(root1) {check out line+ raw/ref_$(root1).psl}' >> blat/protein/template
echo '#ENDLOOP' >> blat/protein/template

cat << '_EOF_' > blat/protein/runTxBlats
#!/bin/csh -ef
set ooc = /cluster/data/$2/11.ooc
set target = ../../txFaSplit/$1
set out1 = uni_$3.psl
set out2 = ref_$3.psl
set tmpDir = /scratch/tmp/$2/ucscGenes/prot
set workDir = $tmpDir/$3
mkdir -p $tmpDir
mkdir $workDir
blat -t=dnax -q=prot -minIdentity=90 $target ../../uniProt.fa $workDir/$out1
blat -t=dnax -q=prot -minIdentity=90 $target ../../refPep.fa $workDir/$out2
mv $workDir/$out1 raw/$out1
mv $workDir/$out2 raw/$out2
if (-e $workDir) then
    rmdir $workDir
endif
rmdir --ignore-fail-on-non-empty $tmpDir
'_EOF_'
    #	<< happy emacs
chmod +x blat/protein/runTxBlats
cd txFaSplit
ls -1 *.fa > ../blat/protein/toDoList
cd ..

# Run protein/transcript blat job on cluster
ssh $cpuFarm "cd $dir/blat/protein; gensub2 toDoList single template jobList"
ssh $cpuFarm "cd $dir/blat/protein; para make jobList"
ssh $cpuFarm "cd $dir/blat/protein; para time > run.time"
cat blat/protein/run.time
# Completed: 194 of 194 jobs
# CPU time in finished jobs:      18304s     305.07m     5.08h    0.21d  0.001 y
# IO & Wait Time:                   726s      12.10m     0.20h    0.01d  0.000 y
# Average job time:                  98s       1.63m     0.03h    0.00d
# Longest running job:                0s       0.00m     0.00h    0.00d
# Longest finished job:             275s       4.58m     0.08h    0.00d
# Submission to last job:           529s       8.82m     0.15h    0.01d



# Sort and select best alignments. Remove raw files for space. Takes 22
# seconds. Use pslReps not pslCdnaFilter because need -noIntrons flag,
# and also working on protein as well as rna alignments. The thresholds
# for the proteins in particular are quite loose, which is ok because
# they will be weighted against each other.  We lose some of the refSeq
# mappings at tighter thresholds.
cd $dir/blat
pslCat -nohead rna/raw/ref*.psl | sort -k 10 | \
	pslReps -noIntrons -nohead -minAli=0.90 -nearTop=0.005 stdin rna/refSeq.psl /dev/null
pslCat -nohead rna/raw/mrna*.psl | sort -k 10 | \
	pslReps -noIntrons -nohead -minAli=0.90 -nearTop=0.005  stdin rna/mrna.psl /dev/null
pslCat -nohead protein/raw/ref*.psl | sort -k 10 | \
	pslReps -noIntrons -nohead -nearTop=0.02  -ignoreSize -minAli=0.85 stdin protein/refSeq.psl /dev/null
pslCat -nohead protein/raw/uni*.psl | sort -k 10 | \
	pslReps -noIntrons -nohead -nearTop=0.02  -minAli=0.85 stdin protein/uniProt.psl /dev/null
rm -r protein/raw
cd ..
# Get parts of multiple alignments corresponding to transcripts.
# Takes 1 hour.
echo $db $xdb $ydb $zdb > ourOrgs.txt
foreach c (`awk '{print $1;}' /cluster/data/$db/chrom.sizes`)
    if (-s txWalk/$c.bed ) then
	mafFrags $db $multiz txWalk/$c.bed stdout -bed12 -meFirst \
	   | mafSpeciesSubset stdin ourOrgs.txt txWalk/$c.maf -keepFirst
    endif
end

# Create and populate directory with various CDS evidence
mkdir -p cdsEvidence
cat txWalk/*.maf | txCdsPredict txWalk.fa -nmd=txWalk.bed -maf=stdin cdsEvidence/txCdsPredict.tce
txCdsEvFromRna refSeq.fa cds.tab blat/rna/refSeq.psl txWalk.fa \
	cdsEvidence/refSeqTx.tce -refStatus=refSeqStatus.tab \
	-unmapped=cdsEvidence/refSeqTx.unmapped -exceptions=exceptions.tab
txCdsEvFromRna mrna.fa cds.tab blat/rna/mrna.psl txWalk.fa \
	cdsEvidence/mrnaTx.tce -mgcStatus=mgcStatus.tab \
	-unmapped=cdsEvidence/mrna.unmapped
txCdsEvFromProtein refPep.fa blat/protein/refSeq.psl txWalk.fa \
	cdsEvidence/refSeqProt.tce -refStatus=refPepStatus.tab \
	-unmapped=cdsEvidence/refSeqProt.unmapped \
	-exceptions=exceptions.tab -refToPep=refToPep.tab \
	-dodgeStop=3 -minCoverage=0.3
txCdsEvFromProtein uniProt.fa blat/protein/uniProt.psl txWalk.fa \
	cdsEvidence/uniProt.tce -uniStatus=uniCurated.tab \
	-unmapped=cdsEvidence/uniProt.unmapped -source=trembl
txCdsEvFromBed ccds.bed ccds txWalk.bed ../../$db.2bit cdsEvidence/ccds.tce
cat cdsEvidence/*.tce | sort  > unweighted.tce

# Merge back in antibodies
cat txWalk.bed antibody.bed > abWalk.bed
sequenceForBed -db=$db -bedIn=antibody.bed -fastaOut=stdout -upCase -keepName > antibody.fa
cat txWalk.fa antibody.fa > abWalk.fa



# Pick ORFs, make genes
cat refToPep.tab refToCcds.tab | \
        txCdsPick abWalk.bed unweighted.tce stdin pick.tce pick.picks \
	-exceptionsIn=exceptions.tab \
	-exceptionsOut=abWalk.exceptions 
txCdsToGene abWalk.bed abWalk.fa pick.tce pick.gtf pick.fa \
	-bedOut=pick.bed -exceptions=abWalk.exceptions
# Create gene info table. Takes 8 seconds
cat mrna/*.unusual refSeq/*.unusual | awk '$5=="flip" {print $6;}' > all.flip
cat mrna/*.psl refSeq/*.psl | txInfoAssemble pick.bed pick.tce cdsEvidence/txCdsPredict.tce \
	altSplice.bed abWalk.exceptions sizePolyA.tab stdin all.flip prelim.info

# Cluster purely based on CDS (in same frame). Takes 1 second
txCdsCluster pick.bed pick.cluster

# Flag suspicious CDS regions, and add this to info file. Weed out bad CDS.
# Map CDS to gene set.  Takes 10 seconds.  Might want to reconsider using
# txCdsWeed here.
txCdsSuspect pick.bed txWalk.txg pick.cluster prelim.info pick.suspect pick.info -niceProt=pick.nice
txCdsWeed pick.tce pick.info weededCds.tce weededCds.info
txCdsToGene abWalk.bed abWalk.fa weededCds.tce weededCds.gtf weededCds.faa \
	-bedOut=weededCds.bed -exceptions=abWalk.exceptions \
	-tweaked=weededCds.tweaked

# Separate out transcripts into coding and 4 uncoding categories.
# Generate new gene set that weeds out the junkiest. Takes 9 seconds.
txGeneSeparateNoncoding weededCds.bed weededCds.info \
	coding.bed nearCoding.bed nearCodingJunk.bed antisense.bed uncoding.bed separated.info
awk '$2 != "nearCodingJunk"' separated.info > weeded.info
awk '$2 == "nearCodingJunk" {print $1}' separated.info > weeds.lst
cat coding.bed nearCoding.bed antisense.bed uncoding.bed | sort -k1,1 -k2,3n >weeded.bed

# Make up a little alignment file for the ones that got tweaked.
sed -r 's/.*NM_//' weededCds.tweaked | awk '{printf("NM_%s\n", $1);}' > tweakedNm.lst
fgrep -f tweakedNm.lst refToPep.tab | cut -f 2 > tweakedNp.lst
fgrep -f weededCds.tweaked weededCds.bed > tweakedNm.bed
sequenceForBed -db=$db -bedIn=tweakedNm.bed -fastaOut=tweakedNm.fa -upCase -keepName
faSomeRecords refPep.fa tweakedNp.lst tweakedNp.fa
blat -q=prot -t=dnax -noHead tweakedNm.fa tweakedNp.fa stdout | sort -k 10 > tweaked.psl

# Make an alignment file for refSeqs that swaps in the tweaked ones
weedLines weededCds.tweaked blat/protein/refSeq.psl refTweaked.psl
cat tweaked.psl >> refTweaked.psl

# Make precursor to kgProtMap table.  This is a psl file that maps just the CDS of the gene
# to the genome.
txGeneCdsMap weeded.bed weeded.info pick.picks refTweaked.psl \
	refToPep.tab /cluster/data/$db/chrom.sizes cdsToRna.psl \
	rnaToGenome.psl
pslMap cdsToRna.psl rnaToGenome.psl cdsToGenome.psl

# Assign permanent accessions to each transcript, and make up a number
# of our files with this accession in place of the temporary IDs we've been
# using.  Takes 4 seconds

txGeneAccession $oldGeneBed ~kent/src/hg/txGene/txGeneAccession/txLastId \
	weeded.bed txToAcc.tab oldToNew.tab
subColumn 4 weeded.bed txToAcc.tab ucscGenes.bed
subColumn 1 weeded.info txToAcc.tab ucscGenes.info
weedLines weeds.lst pick.picks stdout | subColumn 1 stdin txToAcc.tab ucscGenes.picks
weedLines weeds.lst pick.nice stdout | subColumn 2 stdin txToAcc.tab ucscGenes.nice
subColumn 4 coding.bed txToAcc.tab ucscCoding.bed
subColumn 4 nearCoding.bed txToAcc.tab ucscNearCoding.bed
subColumn 4 antisense.bed txToAcc.tab ucscAntisense.bed
subColumn 4 uncoding.bed txToAcc.tab ucscUncoding.bed
subColumn 10 cdsToGenome.psl txToAcc.tab ucscProtMap.psl
cat txWalk/*.ev | weedLines weeds.lst stdin stdout | subColumn 1 stdin txToAcc.tab ucscGenes.ev

# Make files with protein and mrna accessions.  These will be taken from
# RefSeq for the RefSeq ones, and derived from our transcripts for the rest.
# Load these sequences into database. Takes 17 seconds.
txGeneProtAndRna weeded.bed weeded.info abWalk.fa weededCds.faa refSeq.fa \
    refToPep.tab refPep.fa txToAcc.tab ucscGenes.fa ucscGenes.faa

# Cluster the coding and the uncoding sets, and make up canonical and
# isoforms tables. Takes 3 seconds.
txCdsCluster ucscCoding.bed coding.cluster
txBedToGraph ucscUncoding.bed uncoding uncoding.txg -prefix=non
txBedToGraph ucscAntisense.bed antisense antisense.txg -prefix=anti
cat uncoding.txg antisense.txg > senseAnti.txg
txGeneCanonical coding.cluster ucscGenes.info senseAnti.txg ucscGenes.bed ucscNearCoding.bed \
	canonical.tab isoforms.tab txCluster.tab

# Make up final splicing graph just containing our genes, and final alt splice
# table.
txBedToGraph ucscGenes.bed ucscGenes ucscGenes.txg
txgAnalyze ucscGenes.txg /cluster/data/$db/$db.2bit stdout | sort | uniq > ucscSplice.bed


#####################################################################################
# Now the gene set is built.  Time to start loading it into the database,
# and generating all the many tables that go on top of known Genes.
# We do this initially in a temporary database.

# Create temporary database with a few small tables from main database
hgsqladmin create $tempDb
hgsqldump $db chromInfo | hgsql $tempDb
hgsqldump $db trackDb_$user | hgsql $tempDb

# Make up knownGenes table, adding uniProt ID. Load into database. Takes 3
# seconds.
txGeneFromBed ucscGenes.bed ucscGenes.picks ucscGenes.faa uniProt.fa refPep.fa ucscGenes.gp
hgLoadSqlTab $tempDb knownGene ~/kent/src/hg/lib/knownGene.sql ucscGenes.gp
hgLoadBed $tempDb knownAlt ucscSplice.bed

# Load in isoforms, canonical, and gene sequence tables
hgLoadSqlTab $tempDb knownIsoforms ~/kent/src/hg/lib/knownIsoforms.sql isoforms.tab
hgLoadSqlTab $tempDb knownCanonical ~/kent/src/hg/lib/knownCanonical.sql canonical.tab
hgPepPred $tempDb generic knownGenePep ucscGenes.faa
hgPepPred $tempDb generic knownGeneMrna ucscGenes.fa


# Make up kgXref table.  Takes about 3 minutes.
txGeneXref $db $spDb ucscGenes.gp ucscGenes.info ucscGenes.picks ucscGenes.ev ucscGenes.xref
hgLoadSqlTab $tempDb kgXref ~/kent/src/hg/lib/kgXref.sql ucscGenes.xref

# Make up and load kgColor table. Takes about a minute.
txGeneColor $spDb ucscGenes.info ucscGenes.picks ucscGenes.color
hgLoadSqlTab $tempDb kgColor ~/kent/src/hg/lib/kgColor.sql ucscGenes.color

# Load up kgTxInfo table. Takes 0.3 second
hgLoadSqlTab $tempDb kgTxInfo ~/kent/src/hg/lib/txInfo.sql ucscGenes.info

# Make up alias tables and load them. Takes a minute or so.
txGeneAlias $db $spDb ucscGenes.xref ucscGenes.ev foo.alias foo.protAlias
sort foo.alias | uniq > ucscGenes.alias
sort foo.protAlias | uniq > ucscGenes.protAlias
rm foo.alias foo.protAlias
hgLoadSqlTab $tempDb kgAlias ~/kent/src/hg/lib/kgAlias.sql ucscGenes.alias
hgLoadSqlTab $tempDb kgProtAlias ~/kent/src/hg/lib/kgProtAlias.sql ucscGenes.protAlias

# Load up kgProtMap2 table that says where exons are in terms of CDS
hgLoadPsl $tempDb ucscProtMap.psl -table=kgProtMap2

# Create a bunch of knownToXxx tables.  Takes about 3 minutes:
hgMapToGene $db -tempDb=$tempDb allenBrainAli -type=psl knownGene knownToAllenBrain
hgMapToGene $db -tempDb=$tempDb ensGene knownGene knownToEnsembl
hgMapToGene $db -tempDb=$tempDb gnfAtlas2 knownGene knownToGnfAtlas2 '-type=bed 12'
hgsql --skip-column-names -e "select mrnaAcc,locusLinkId from refLink" $db > refToLl.txt
hgMapToGene $db -tempDb=$tempDb refGene knownGene knownToLocusLink -lookup=refToLl.txt
hgMapToGene $db -tempDb=$tempDb refGene knownGene knownToRefSeq
     
if ($db =~ hg*) then
    hgMapToGene $db -tempDb=$tempDb affyGnf1h knownGene knownToGnf1h
    hgMapToGene $db -tempDb=$tempDb HInvGeneMrna knownGene knownToHInv
    hgMapToGene $db -tempDb=$tempDb affyU133Plus2 knownGene knownToU133Plus2
    hgMapToGene $db -tempDb=$tempDb affyUclaNorm knownGene knownToU133
    hgMapToGene $db -tempDb=$tempDb affyU95 knownGene knownToU95
    hgMapToGene $db -tempDb=$tempDb $snpTable knownGene knownToCdsSnp -all -cds
    knownToHprd $tempDb /cluster/data/$db/p2p/hprd/FLAT_FILES/HPRD_ID_MAPPINGS.txt
    time hgExpDistance $tempDb hgFixed.gnfHumanU95MedianRatio \
	    hgFixed.gnfHumanU95Exps gnfU95Distance  -lookup=knownToU95
    time hgExpDistance $tempDb hgFixed.gnfHumanAtlas2MedianRatio \
	hgFixed.gnfHumanAtlas2MedianExps gnfAtlas2Distance \
	-lookup=knownToGnfAtlas2
    # All exon array.
    mkdir affyAllExon
    cd affyAllExon
    overlapSelect -inFmt=genePred -selectFmt=bed -idOutput \
       ../../affyHumanExon/affyHuEx1.bed ../ucscGenes.gp ids.txt
    echo "select * from affyHumanExon" | hgsql hgFixed | tail +2 > expData.txt
    affyAllExonGSColumn expData.txt ids.txt column.txt
    hgLoadSqlTab $tempDb affyHumanExonGs ~/kent/src/hg/lib/expData.sql column.txt
    ln -s ~/kent/src/hg/makeDb/hgRatioMicroarray/affyHumanExon.ra .
    hgRatioMicroarray -database=$tempDb -clump=affyHumanExon.ra -minAbsVal=0.01 \
       affyHumanExonGs affyHumanExonGsRatio
    hgMedianMicroarray $tempDb affyHumanExonGs affyHumanExonExps \
       affyHumanExon.ra affyHumanExonGsMedian affyHumanExonMedianExps
    hgMedianMicroarray $tempDb affyHumanExonGsRatio \
       affyHumanExonExps affyHumanExon.ra affyHumanExonGsRatioMedian \
       affyHumanExonMedianExps
    hgExpDistance tmpFoo2 affyHumanExonGsRatioMedian \
       affyHumanExonGsMedianExps affyHumanExonGsRatioMedianDist

endif
if ($db =~ mm*) then
    hgMapToGene $db -tempDb=$tempDb affyGnf1m knownGene knownToGnf1m
    hgMapToGene $db -tempDb=$tempDb gnfAtlas2 knownGene knownToGnfAtlas2 '-type=bed 12'
    hgMapToGene $db -tempDb=$tempDb affyU74  knownGene knownToU74
    hgMapToGene $db -tempDb=$tempDb affyMOE430 knownGene knownToMOE430
    hgMapToGene $db -tempDb=$tempDb affyMOE430 -prefix=A: knownGene knownToMOE430A
    hgExpDistance $tempDb $db.affyGnfU74A affyGnfU74AExps affyGnfU74ADistance -lookup=knownToU74
    hgExpDistance $tempDb $db.affyGnfU74B affyGnfU74BExps affyGnfU74BDistance -lookup=knownToU74
    hgExpDistance $tempDb $db.affyGnfU74C affyGnfU74CExps affyGnfU74CDistance -lookup=knownToU74
    hgExpDistance $tempDb hgFixed.gnfMouseAtlas2MedianRatio \
	    hgFixed.gnfMouseAtlas2MedianExps gnfAtlas2Distance -lookup=knownToGnf1m
endif
knownToVisiGene $tempDb -probesDb=$db
vgGetText /usr/local/apache/cgi-bin/visiGeneData/visiGene.text $vgTextDbs

# Create Human P2P protein-interaction Gene Sorter columns
if ($db =~ hg*) then
hgLoadNetDist /cluster/data/$db/p2p/hprd/hprd.pathLengths $tempDb humanHprdP2P \
    -sqlRemap="select distinct value, name from knownToHprd"
hgLoadNetDist /cluster/data/$db/p2p/vidal/humanVidal.pathLengths $tempDb humanVidalP2P \
    -sqlRemap="select distinct locusLinkID, kgID from $db.refLink,kgXref where $db.refLink.mrnaAcc = kgXref.mRNA"
hgLoadNetDist /cluster/data/$db/p2p/wanker/humanWanker.pathLengths $tempDb humanWankerP2P \
    -sqlRemap="select distinct locusLinkID, kgID from $db.refLink,kgXref where $db.refLink.mrnaAcc = kgXref.mRNA"
endif

# Run nice Perl script to make all protein blast runs for
# Gene Sorter and Known Genes details page.  Takes about
# 45 minutes to run.
mkdir $dir/hgNearBlastp
cd $dir/hgNearBlastp
cat << _EOF_ > config.ra
# Latest human vs. other Gene Sorter orgs:
# mouse, rat, zebrafish, worm, yeast, fly

targetGenesetPrefix known
targetDb $tempDb
queryDbs $xdb $ratDb $fishDb $flyDb $wormDb $yeastDb

${db}Fa $tempFa
${xdb}Fa $xdbFa
${ratDb}Fa $ratFa
${fishDb}Fa $fishFa
${flyDb}Fa $flyFa
${wormDb}Fa $wormFa
${yeastDb}Fa $yeastFa

buildDir $dir/hgNearBlastp
scratchDir $scratchDir/jkgHgNearBlastp
_EOF_
doHgNearBlastp.pl config.ra |& tee do.log 


# Remove non-syntenic hits for mouse and rat
# Takes a few minutes
mkdir /gbdb/$tempDb/liftOver
ln -s /cluster/data/$db/bed/liftOver/${db}To$RatDb.over.chain.gz \
    /gbdb/$tempDb/liftOver/${tempDb}To$RatDb.over.chain.gz
ln -s /cluster/data/$db/bed/liftOver/${db}To${Xdb}.over.chain.gz \
    /gbdb/$tempDb/liftOver/${tempDb}To$Xdb.over.chain.gz
synBlastp.csh $tempDb $ratDb
synBlastp.csh $tempDb $xdb

# Make reciprocal best subset for the blastp pairs that are too
# Far for synteny to help

# Us vs. fish
cd $dir/hgNearBlastp
set aToB = run.$tempDb.$fishDb
set bToA = run.$fishDb.$tempDb
cat $aToB/out/*.tab > $aToB/all.tab
cat $bToA/out/*.tab > $bToA/all.tab
blastRecipBest $aToB/all.tab $bToA/all.tab $aToB/recipBest.tab $bToA/recipBest.tab
hgLoadBlastTab $tempDb drBlastTab $aToB/recipBest.tab
hgLoadBlastTab $fishDb tfBlastTab $bToA/recipBest.tab

# Us vs. fly
cd $dir/hgNearBlastp
set aToB = run.$tempDb.$flyDb
set bToA = run.$flyDb.$tempDb
cat $aToB/out/*.tab > $aToB/all.tab
cat $bToA/out/*.tab > $bToA/all.tab
blastRecipBest $aToB/all.tab $bToA/all.tab $aToB/recipBest.tab $bToA/recipBest.tab
hgLoadBlastTab $tempDb dmBlastTab $aToB/recipBest.tab
hgLoadBlastTab $flyDb tfBlastTab $bToA/recipBest.tab

# Us vs. worm
cd $dir/hgNearBlastp
set aToB = run.$tempDb.$wormDb
set bToA = run.$wormDb.$tempDb
cat $aToB/out/*.tab > $aToB/all.tab
cat $bToA/out/*.tab > $bToA/all.tab
blastRecipBest $aToB/all.tab $bToA/all.tab $aToB/recipBest.tab $bToA/recipBest.tab
hgLoadBlastTab $tempDb ceBlastTab $aToB/recipBest.tab
hgLoadBlastTab $wormDb tfBlastTab $bToA/recipBest.tab

# Us vs. yeast
cd $dir/hgNearBlastp
set aToB = run.$tempDb.$yeastDb
set bToA = run.$yeastDb.$tempDb
cat $aToB/out/*.tab > $aToB/all.tab
cat $bToA/out/*.tab > $bToA/all.tab
blastRecipBest $aToB/all.tab $bToA/all.tab $aToB/recipBest.tab $bToA/recipBest.tab
hgLoadBlastTab $tempDb scBlastTab $aToB/recipBest.tab
hgLoadBlastTab $yeastDb tfBlastTab $bToA/recipBest.tab

# Clean up
cat run.$tempDb.$tempDb/out/*.tab | gzip -c > run.$tempDb.$tempDb/all.tab.gz
rm -r run.*/out
gzip run.*/all.tab

# MAKE FOLDUTR TABLES 
# First set up directory structure and extract UTR sequence on hgwdev
cd $dir
mkdir -p rnaStruct
cd rnaStruct
mkdir -p utr3/split utr5/split utr3/fold utr5/fold
utrFa $db knownGene utr3 utr3/utr.fa
utrFa $db knownGene utr5 utr5/utr.fa

# Split up files and make files that define job.
faSplit sequence utr3/utr.fa 10000 utr3/split/s
faSplit sequence utr5/utr.fa 10000 utr5/split/s
ls -1 utr3/split > utr3/in.lst
ls -1 utr5/split > utr5/in.lst
cd utr3
cat > gsub <<end
#LOOP
rnaFoldBig split/\$(path1) fold
#ENDLOOP
end
gensub2 in.lst single gsub spec
cp gsub ../utr5
cd ../utr5
gensub2 in.lst single gsub spec

# Do cluster runs for UTRs
ssh $cpuFarm "cd $dir/rnaStruct/utr3; para make spec"
ssh $cpuFarm "cd $dir/rnaStruct/utr5; para make spec"

# Load database
    cd $dir/rnaStruct/utr5
    hgLoadRnaFold $tempDb foldUtr5 fold
    cd ../utr3
    hgLoadRnaFold -warnEmpty $tempDb foldUtr3 fold
# There are a five warnings on empty files.  Seems to be a problem in
# RNAfold, so not easy for us to fix. Consequence is not too bad, just a
# few 3' UTRs will be missing annotation.

# Clean up
    rm -r split fold err batch.bak
    cd ../utr5
    rm -r split fold err batch.bak

# Make pfam run.  Actual cluster run is about 6 hours.
# First get pfam global HMMs into /san/sanvol1/pfam somehow.
cd /san/sanvol1/scratch/$db
mkdir ucscGenes
cd ucscGenes
mkdir splitProt
faSplit sequence $dir/ucscGenes.faa 10000 splitProt/
mkdir pfam
cd pfam
mkdir out
ls -1 ../splitProt > in.lst
cat << _EOF_ > doPfam
#!/bin/tcsh -ef
/san/sanvol1/pfam/hmmpfam -E 0.1 /san/sanvol1/pfam/Pfam_fs \$1 > \$2
_EOF_
chmod a+x doPfam
cat << _EOF_ > gsub
#LOOP
doPfam ../splitProt/\$(path1) out/\$(root1).pf
#ENDLOOP
_EOF_
gensub2 in.lst single gsub spec
ssh pk "cd /san/sanvol1/scratch/$db/ucscGenes/pfam; para make spec"

# Make up pfamDesc.tab by converting pfam to a ra file first
cat << _EOF_ > makePfamRa.awk
/^NAME/ {print}
/^ACC/ {print}
/^DESC/ {print; printf("\n");}
_EOF_
awk -f makePfamRa.awk  /cluster/store12/pfam/Pfam_fs > pfamDesc.ra
raToTab -cols=ACC,NAME,DESC pfamDesc.ra stdout | \
   awk -F '\t' '{printf("%s\t%s\t%s\n", gensub(/\.[0-9]+/, "", "g", $1), $2, $3);}' > pfamDesc.tab

# Convert output to tab-separated file. 
cd $dir
catDir /san/sanvol1/scratch/$db/ucscGenes/pfam/out | hmmPfamToTab -eValCol stdin ucscPfam.tab

# Convert output to knownToPfam table
awk '{printf("%s\t%s\n", $2, gensub(/\.[0-9]+/, "", "g", $1));}' /san/sanvol1/scratch/$db/ucscGenes/pfam/pfamDesc.tab > sub.foo
cut -f 1,4 ucscPfam.tab | subColumn 2 stdin sub.foo knownToPfam.tab
hgLoadSqlTab $tempDb knownToPfam ~/kent/src/hg/lib/knownTo.sql knownToPfam.tab

# Do scop run. Takes about 6 hours
# First get pfam global HMMs into /san/sanvol1/scop somehow.
cd /san/sanvol1/scratch/$db/ucscGenes
mkdir scop
cd scop
mkdir out
ls -1 ../splitProt > in.lst
cat << _EOF_ > doScop
#!/bin/tcsh -ef
/san/sanvol1/pfam/hmmpfam -E 0.1 /san/sanvol1/scop/scop.hmm \$1 > \$2
_EOF_
chmod a+x doScop
cat << _EOF_ > gsub
#LOOP
doScop ../splitProt/\$(path1) out/\$(root1).pf
#ENDLOOP
_EOF_
gensub2 in.lst single gsub spec
ssh pk "cd /san/sanvol1/scratch/$db/ucscGenes/scop; para make spec"


# Convert scop output to tab-separated files
catDir /san/sanvol1/scratch/$db/ucscGenes/scop/out | \
	hmmPfamToTab -eValCol -scoreCol stdin scopPlusScore.tab
scopCollapse scopPlusScore.tab /cluster/store12/scop/model.tab ucscScop.tab \
	scopDesc.tab knownToSuper.tab
hgLoadSqlTab $tempDb knownToSuper ~/kent/src/hg/lib/knownToSuper.sql knownToSuper.tab

# Regenerate ccdsKgMap table
#/cluster/data/genbank/bin/x86_64/mkCcdsGeneMap  -db=$db -loadDb ccdsGene knownGene ccdsKgMap
/cluster/data/genbank/bin/x86_64/mkCcdsGeneMap  -db=$tempDb -loadDb $db.ccdsGene knownGene ccdsKgMap


# Map old to new mapping
hgsql $db -N -e 'select * from knownGene' > knownGeneOld.gp
genePredToBed knownGeneOld.gp >knownGeneOld.bed
cat refSeq/*.bed mrna/*.bed | txGeneExplainUpdate1 knownGeneOld.bed ucscGenes.bed stdin abWalk.bed kgOldToNew.bed
hgLoadSqlTab $tempDb kg${lastVer}ToKg${curVer} ~/kent/src/hg/lib/kg1ToKg2.sql kgOldToNew.bed

# Build kgSpAlias table, which combines content of both kgAlias and kgProtAlias tables.

hgsql $tempDb -N -e \
    'select kgXref.kgID, spID, alias from kgXref, kgAlias where kgXref.kgID=kgAlias.kgID' >j.tmp
         
hgsql $tempDb -N -e \
    'select kgXref.kgID, spID, alias from kgXref, kgProtAlias where kgXref.kgID=kgProtAlias.kgID'\
    >>j.tmp
cat j.tmp|sort -u  >kgSpAlias.tab
    rm j.tmp

hgLoadSqlTab $tempDb kgSpAlias ~/kent/src/hg/lib/kgSpAlias.sql ./kgSpAlias.tab

# RE-BUILD HG18 PROTEOME BROWSER TABLES (DONE, Fan, 4/2/07). 

# These are instructions for building tables 
# needed for the Proteome Browser. 
 
# DON'T START THESE UNTIL TABLES FOR KNOWN GENES AND kgProtMap table
# ARE REBUILT.  
# This build is based on proteins DBs dated 070202.

# Create the working directory

mkdir -p $dir/pb
cd $dir/pb


# Build the pepMwAa table

hgsql $spDb -N -e \
"select info.acc, molWeight, aaSize from info, accToTaxon where accToTaxon.taxon=$taxon and accToTaxon.acc = info.acc" > pepMwAa.tab

hgLoadSqlTab $tempDb pepMwAa ~/kent/src/hg/lib/pepMwAa.sql ./pepMwAa.tab

# Build the pepPi table

hgsql $spDb -e \
    "select info.acc from info, accToTaxon where accToTaxon.taxon=$taxon and accToTaxon.acc = info.acc" > protAcc.lis

hgsql $tempDb -N -e 'select proteinID from knownGene where proteinID like "%-%"' | sort -u >> protAcc.lis

pbCalPi protAcc.lis $spDb pepPi.tab
hgLoadSqlTab $tempDb pepPi ~/kent/src/hg/lib/pepPi.sql ./pepPi.tab

# Calculate and load pep distributions

pbCalDist $spDb $pbDb $taxon $tempDb 
hgLoadSqlTab $tempDb pepExonCntDist ~/kent/src/hg/lib/pepExonCntDist.sql ./pepExonCntDist.tab
hgLoadSqlTab $tempDb pepCCntDist ~/kent/src/hg/lib/pepCCntDist.sql ./pepCCntDist.tab
hgLoadSqlTab $tempDb pepHydroDist ~/kent/src/hg/lib/pepHydroDist.sql ./pepHydroDist.tab
hgLoadSqlTab $tempDb pepMolWtDist ~/kent/src/hg/lib/pepMolWtDist.sql ./pepMolWtDist.tab
hgLoadSqlTab $tempDb pepResDist ~/kent/src/hg/lib/pepResDist.sql ./pepResDist.tab
hgLoadSqlTab $tempDb pepIPCntDist ~/kent/src/hg/lib/pepIPCntDist.sql ./pepIPCntDist.tab
hgLoadSqlTab $tempDb pepPiDist ~/kent/src/hg/lib/pepPiDist.sql ./pepPiDist.tab


# Calculate frequency distributions

pbCalResStd $spDb $taxon $tempDb

# Create pbAnomLimit and pbResAvgStd tables

hgLoadSqlTab $tempDb pbAnomLimit ~/kent/src/hg/lib/pbAnomLimit.sql ./pbAnomLimit.tab

hgLoadSqlTab $tempDb pbResAvgStd ~/kent/src/hg/lib/pbResAvgStd.sql ./pbResAvgStd.tab

# The old pbStamp table seems OK, so no adjustment needed.

# Do BioCyc Pathways build
    mkdir $dir/bioCyc
    cd $dir/bioCyc
    grep -v '^#' $bioCycPathways > pathways.tab
    grep -v '^#' $bioCycGenes > genes.tab
    kgBioCyc1 genes.tab pathways.tab $db bioCycPathway.tab bioCycMapDesc.tab
    hgLoadSqlTab $tempDb bioCycPathway ~/kent/src/hg/lib/bioCycPathway.sql ./bioCycPathway.tab
    hgLoadSqlTab $tempDb bioCycMapDesc ~/kent/src/hg/lib/bioCycMapDesc.sql ./bioCycMapDesc.tab

# Do KEGG Pathways build
    mkdir $dir/kegg
    cd $dir/kegg
    kgAttachKegg $db $keggList keggPathway.tab
    hgLoadSqlTab $tempDb keggPathway ~/src/hg/lib/keggPathway.sql ./keggPathway.tab

# Make spMrna table (useful still?)
   cd $dir
   hgsql $db -N -e "select spDisplayID,kgID from kgXref where spDisplayID != ''" > spMrna.tab;
   hgLoadSqlTab $tempDb spMrna ~/kent/src/hg/lib/spMrna.sql spMrna.tab

# Do CGAP tables 

    mkdir $dir/cgap
    cd $dir/cgap
    
    wget --timestamping -O Hs_GeneData.dat "ftp://ftp1.nci.nih.gov/pub/CGAP/Hs_GeneData.dat"
    hgCGAP Hs_GeneData.dat
        
    cat cgapSEQUENCE.tab cgapSYMBOL.tab cgapALIAS.tab|sort -u > cgapAlias.tab
    hgLoadSqlTab $tempDb cgapAlias ~/kent/src/hg/lib/cgapAlias.sql ./cgapAlias.tab

    hgLoadSqlTab $tempDb cgapBiocPathway ~/kent/src/hg/lib/cgapBiocPathway.sql ./cgapBIOCARTA.tab

    cat cgapBIOCARTAdesc.tab|sort -u > cgapBIOCARTAdescSorted.tab
    hgLoadSqlTab $tempDb cgapBiocDesc ~/kent/src/hg/lib/cgapBiocDesc.sql cgapBIOCARTAdescSorted.tab
			    

exit # BRACKET

# NOW SWAP IN TABLES FROM TEMP DATABASE TO MAIN DATABASE.
# You'll need superuser powers for this step.....

# Save old known genes and kgXref tables
sudo ~kent/bin/copyMysqlTable $db knownGene $tempDb knownGeneOld$lastVer
sudo ~kent/bin/copyMysqlTable $db kgXref $tempDb kxXrefOld$lastVer

# Create backup database
hgsqladmin create ${db}Backup

# Drop tempDb history table, we don't want to swap it in!
hgsql -e "drop table history" $tempDb

# Swap in new tables, moving old tables to backup database.
sudo ~kent/bin/swapInMysqlTempDb $tempDb $db ${db}Backup

# Update database links.
sudo rm /var/lib/mysql/uniProt
sudo ln -s /var/lib/mysql/$spDb /var/lib/mysql/uniProt
sudo rm /var/lib/mysql/proteome
sudo ln -s /var/lib/mysql/$pbDb /var/lib/mysql/proteome
hgsqladmin flush-tables

# Make full text index.  Takes a minute or so.  After this the genome browser
# tracks display will work including the position search.  The genes details
# page, gene sorter, and proteome browser still need more tables.
mkdir $dir/index
cd $dir/index
hgKgGetText $db knownGene.text 
ixIxx knownGene.text knownGene.ix knownGene.ixx
rm -f /gbdb/$db/knownGene.ix /gbdb/$db/knownGene.ixx
ln -s $dir/index/knownGene.ix  /gbdb/$db/knownGene.ix
ln -s $dir/index/knownGene.ixx /gbdb/$db/knownGene.ixx

# Build known genes list for google
 make knownGeneLists.html ${db}GeneList.html mm5GeneList.html rm3GeneList.html

    cd /cluster/data/$db/bed
    rm -rf knownGeneList/$db

# Run hgKnownGeneList to generate the tree of HTML pages
# under ./knownGeneList/hg18 

    hgKnownGeneList $db

# copy over to /usr/local/apache/htdocs
    
    rm -rf /usr/local/apache/htdocs/knownGeneList/$db
    mkdir -p /usr/local/apache/htdocs/knownGeneList/$db
    cp -Rfp knownGeneList/$db/* /usr/local/apache/htdocs/knownGeneList/$db

#
