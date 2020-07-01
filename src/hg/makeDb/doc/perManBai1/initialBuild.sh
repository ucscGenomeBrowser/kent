# for emacs: -*- mode: sh; -*-

# This file describes browser build for perManBai1

# GCF_000500345.1_Pman_1.0_assembly_report.txt

# Assembly name:  Pman_1.0
# Organism name:  Peromyscus maniculatus bairdii (prairie deer mouse)
# Taxid:          230844
# BioSample:      SAMN00848614
# BioProject:     PRJNA53563
# Submitter:      Baylor College of Medicine
# Date:           2013-12-3
# Assembly type:  haploid
# Release type:   major
# Assembly level: Scaffold
# Genome representation: full
# WGS project:    AYHN01
# Assembly method: Newbler v. 2.3 and 2.5; AllPaths v. 41070; ATLAS-gapfill v. 2.2; ATLAS-link v. 1.0
# Genome coverage: 110.0x
# Sequencing technology: FLX 454; Illumina HiSeq
# RefSeq category: Representative Genome
# GenBank assembly accession: GCA_000500345.1
# RefSeq assembly accession: GCF_000500345.1
# RefSeq assembly and GenBank assemblies identical: yes
#
## Assembly-Units:
## GenBank Unit Accession       RefSeq Unit Accession   Assembly-Unit name
## GCA_000500355.1      GCF_000500355.1 Primary Assembly

#############################################################################
# obtain photograph (DONE - 2017-02-16 - Hiram)
    mkdir -p /hive/data/genomes/perManBai1/photo
    cd /hive/data/genomes/perManBai1/photo

    wget -O photoFile.jpg 'https://upload.wikimedia.org/wikipedia/commons/1/18/Deer_Mouse_Portal_%28Peromyscus_maniculatus%29_%2815722414288%29.jpg'

    convert -quality 80 -sharpen 0 -normalize -geometry 400x400 \
	photoFile.jpg Peromyscus_maniculatus_bairdii.jpg

    printf "photoCreditURL  https://www.flickr.com/people/22170893@N06?rb=1
photoCreditName Gregory "Slobirdr" Smith (flickr)\n" > ../photoReference.txt

    cat ../photoReference.txt 
photoCreditURL  https://www.flickr.com/people/22170893@N06?rb=1
photoCreditName Gregory "Slobirdr" Smith (flickr)

#########################################################################
#  Initial steps (DONE - 2019-03-11 - Hiram)

# To start this initialBuild.txt document, from a previous assembly document:

mkdir ~/kent/src/hg/makeDb/doc/perManBai1
cd ~/kent/src/hg/makeDb/doc/perManBai1

# best to use a most recent document since it has the latest features and
# procedures:

sed -e 's/ambMex1/perManBai1/g; s/AmbMex1/PerManBai1/g; s/DONE/TBD/g;' ../ambMex1/initialBuild.txt > initialBuild.txt

#############################################################################
# fetch sequence from new style download directory (DONE - 2019-03-11 - Hiram)
    mkdir -p /hive/data/genomes/perManBai1/refseq
    cd /hive/data/genomes/perManBai1/refseq

    time rsync -L -a -P --stats \
rsync://ftp.ncbi.nlm.nih.gov/genomes/refseq/vertebrate_mammalian/Peromyscus_maniculatus/all_assembly_versions/GCF_000500345.1_Pman_1.0/ ./

    # real    0m51.781s

    # measure what we have here, this one is hefty:
    faSize GCF_000500345.1_Pman_1.0_genomic.fna.gz

# 2630541020 bases (157004803 N's 2473536217 real 1695858008 upper
#	777678209 lower) in 30921 sequences in 1 files
# Total size: mean 85073.0 sd 633290.8 min 201 (NW_006521020.1)
#	max 18898765 (NW_006501040.1) median 2048
# %29.56 masked total, %31.44 masked real

#############################################################################
# fixup to UCSC naming scheme (DONE - 2019-03-12 - Hiram)
    mkdir /hive/data/genomes/perManBai1/ucsc
    cd /hive/data/genomes/perManBai1/ucsc

    # verify no duplicate sequences:  note the use of the -long argument
    # on this gigantic amount of sequence
    time faToTwoBit ../refseq/*0_genomic.fna.gz refseq.2bit
    #	real    0m47.359s

    time twoBitDup refseq.2bit
    # real    0m10.911s
    # should be silent output, otherwise the duplicates need to be removed

    # since this is an unplaced contig assembly, verify all names are .1:
    twoBitInfo refseq.2bit  stdout | awk '{print $1}' \
	| sed -e 's/[0-9]\+//;' | sort | uniq -c
    #    30921 NW_.1

    # in this case, all the .1's can be changed to: v1
    time zcat ../refseq/G*0_genomic.fna.gz \
       | sed -e 's/.1 Peromyscus.*/v1/;' | gzip -c \
          > ucsc.perManBai1.fa.gz
# -rw-rw-r-- 1 810656363 Mar 12 15:11 ucsc.perManBai1.fa.gz

    zgrep -v "^#" ../refseq/GCF_000500345.1_Pman_1.0_assembly_structure/Primary_Assembly/unplaced_scaffolds/AGP/unplaced.scaf.agp.gz \
       | sed -e "s/.1\t/v1\t/;" > ucsc.perManBai1.agp

    # Just discovered there is a brand new RefSeq chrM sequence
    # available.  Will use this accession in the config.ra file:
    mitoAcc="NC_039921.1"

    # verify fasta and AGP match:
    time faToTwoBit ucsc.perManBai1.fa.gz test.2bit
    # real    0m53.987s

    # verify still silent:
    time twoBitDup test.2bit
    # real    0m10.005s

    # and check AGP vs. fasta correspondence:
    time checkAgpAndFa *.agp test.2bit 2>&1 | tail
    #  All AGP and FASTA entries agree - both files are valid
    # real    0m4.913s

    # verify nothing lost compared to genbank:
    time twoBitToFa test.2bit stdout | faSize stdin
# 2630541020 bases (157004803 N's 2473536217 real 1695858008 upper
#	777678209 lower) in 30921 sequences in 1 files
# Total size: mean 85073.0 sd 633290.8 min 201 (NW_006521020v1)
#	max 18898765 (NW_006501040v1) median 2048
# %29.56 masked total, %31.44 masked real
    # real    0m30.309s

    # the original refseq count is the same:
# 2630541020 bases (157004803 N's 2473536217 real 1695858008 upper
#	777678209 lower) in 30921 sequences in 1 files
# 32393605577 bases (4026911109 N's 28366694468 real 28365740082 upper
#	954386 lower) in 125724 sequences in 1 files

    # no longer needed:
    rm -f refseq.2bit test.2bit

#############################################################################
# idKeys for genbank assembly
    mkdir /hive/data/genomes/perManBai1/genbank/idKeys
    cd /hive/data/genomes/perManBai1/genbank/idKeys
    ln -s /hive/data/outside/ncbi/genomes/genbank/vertebrate_mammalian/Peromyscus_maniculatus/all_assembly_versions/GCA_000500345.1_Pman_1.0/GCA_000500345.1_Pman_1.0_genomic.fna.gz .

    faToTwoBit GCA_000500345.1_Pman_1.0_genomic.fna.gz genbankPerManBai1.2bit
    doIdKeys.pl -buildDir=`pwd` -twoBit=`pwd`/genbankPerManBai1.2bit \
        genbankPerManBai1

#############################################################################
#  Initial database build (DONE - 2019-03-13 - Hiram)

    cd /hive/data/genomes/perManBai1

    # establish the config.ra file:
    # usually would use this
    ~/kent/src/hg/utils/automation/prepConfig.pl perManBai1 mammal mouse \
       refseq/*_assembly_report.txt > perManBai1.config.ra
    # going to need a mitoAcc ?
    # add the mito acc: mitoAcc NC_039921.1

    # verify this looks OK:

    cat perManBai1.config.ra
# config parameters for makeGenomeDb.pl:
db perManBai1
clade mammal
scientificName Peromyscus maniculatus bairdii
commonName Prairie deer mouse
assemblyDate Dec. 2013
assemblyLabel Baylor College of Medicine
assemblyShortLabel Pman_1.0
orderKey 16755
mitoAcc NC_039921.1
fastaFiles /hive/data/genomes/perManBai1/ucsc/*.fa.gz
agpFiles /hive/data/genomes/perManBai1/ucsc/*.agp
# qualFiles none
dbDbSpeciesDir mouse
photoCreditURL  https://www.flickr.com/people/22170893@N06?rb=1
photoCreditName Gregory "Slobirdr" Smith (flickr)
ncbiGenomeId 11397
ncbiAssemblyId 392808
ncbiAssemblyName Pman_1.0
ncbiBioProject 53563
ncbiBioSample SAMN00848614
genBankAccessionID GCF_000500345.1
taxId 230844

    time (~/kent/src/hg/utils/automation/makeGenomeDb.pl \
	-workhorse=hgwdev -dbHost=hgwdev -fileServer=hgwdev \
         -stop=seq perManBai1.config.ra) > seq.log 2>&1
    # real    2m19.045s

    time (~/kent/src/hg/utils/automation/makeGenomeDb.pl \
	-workhorse=hgwdev -dbHost=hgwdev -fileServer=hgwdev \
         -continue=agp -stop=agp perManBai1.config.ra) > agp.log 2>&1
    # real    0m9.644s
    # verify it ran OK:
    #   *** All done!  (through the 'agp' step)
    time (~/kent/src/hg/utils/automation/makeGenomeDb.pl \
	-workhorse=hgwdev -dbHost=hgwdev -fileServer=hgwdev \
         -continue=db -stop=db perManBai1.config.ra) > db.log 2>&1
    # real    13m40.220s
    # real    0m21.249s

    time (~/kent/src/hg/utils/automation/makeGenomeDb.pl \
	-workhorse=hgwdev -dbHost=hgwdev -fileServer=hgwdev \
         -continue=dbDb perManBai1.config.ra) > dbDb.log 2>&1
    # real    0m6.977s

    time (~/kent/src/hg/utils/automation/makeGenomeDb.pl \
	-workhorse=hgwdev -dbHost=hgwdev -fileServer=hgwdev \
         -continue=trackDb perManBai1.config.ra) > trackDb.log 2>&1
    # real    0m7.158s

    # check in the trackDb files created and add to trackDb/makefile
    # temporary symlink until after masking
    ln -s `pwd`/perManBai1.unmasked.2bit /gbdb/perManBai1/perManBai1.2bit

#############################################################################
# cytoBandIdeo - (DONE - 2019-03-13 - Hiram)
    mkdir /hive/data/genomes/perManBai1/bed/cytoBand
    cd /hive/data/genomes/perManBai1/bed/cytoBand
    time makeCytoBandIdeo.csh perManBai1
    # real    0m0.804s

##############################################################################
# cpgIslands on UNMASKED sequence (DONE - 2019-03-13 - Hiram)
    mkdir /hive/data/genomes/perManBai1/bed/cpgIslandsUnmasked
    cd /hive/data/genomes/perManBai1/bed/cpgIslandsUnmasked

    time (doCpgIslands.pl -dbHost=hgwdev -bigClusterHub=ku -buildDir=`pwd` \
       -tableName=cpgIslandExtUnmasked \
          -maskedSeq=/hive/data/genomes/perManBai1/perManBai1.unmasked.2bit \
             -workhorse=hgwdev -smallClusterHub=ku perManBai1) > do.log 2>&1
    # real    11m18.290s

    cat fb.perManBai1.cpgIslandExtUnmasked.txt
    # 14676098 bases of 2473552766 (0.593%) in intersection

#############################################################################
# running repeat masker (DONE - 2019-03-13 - Hiram)
    mkdir /hive/data/genomes/perManBai1/bed/repeatMasker
    cd /hive/data/genomes/perManBai1/bed/repeatMasker
    time  (doRepeatMasker.pl -buildDir=`pwd` \
        -bigClusterHub=ku -dbHost=hgwdev -workhorse=hgwdev \
        -smallClusterHub=ku perManBai1) > do.log 2>&1 &
XXX - running - Wed Mar 13 10:45:49 PDT 2019
    # curiously, ran very fast ?  There are hardly any repeats in the templates
    # used by RM
# Completed: 71471 of 71471 jobs
# CPU time in finished jobs:    7334652s  122244.20m  2037.40h   84.89d  0.233 y
# IO & Wait Time:                217376s    3622.93m    60.38h    2.52d  0.007 y
# Average job time:                 106s       1.76m     0.03h    0.00d
# Longest finished job:             512s       8.53m     0.14h    0.01d
# Submission to last job:         11477s     191.28m     3.19h    0.13d

    cat faSize.rmsk.txt
# 2400839308 bases (53716773 N's 2347122535 real 1331243472 upper
#	1015879063 lower) in 7872 sequences in 1 files
# Total size: mean 304984.7 sd 3152710.5 min 1000 (NW_018732762v1)
#	max 84771923 (NW_018734349v1) median 1998
# %42.31 masked total, %43.28 masked real

    egrep -i "versi|relea" do.log
    # RepeatMasker version open-4.0.5
    #    January 31 2015 (open-4-0-5) version of RepeatMasker
    # CC   RELEASE 20140131;

    time featureBits -countGaps perManBai1 rmsk
    # 1016100984 bases of 2400839308 (42.323%) in intersection
    # real    0m44.536s

    # why is it different than the faSize above ?
    # because rmsk masks out some N's as well as bases, the count above
    #   separates out the N's from the bases, it doesn't show lower case N's

    # faster way to get the same result on high contig count assemblies::
    time hgsql -N -e 'select genoName,genoStart,genoEnd from rmsk;' perManBai1 \
        | bedSingleCover.pl stdin | ave -col=4 stdin | grep "^total"
    # total 1016100984.000000
    # real    0m36.665s

##########################################################################
# running simple repeat (DONE - 2019-03-13 - Hiram)

    mkdir /hive/data/genomes/perManBai1/bed/simpleRepeat
    cd /hive/data/genomes/perManBai1/bed/simpleRepeat
    time (doSimpleRepeat.pl -buildDir=`pwd` -bigClusterHub=ku \
        -dbHost=hgwdev -workhorse=hgwdev -smallClusterHub=ku \
        -trf409=5 perManBai1) > do.log 2>&1 &
XXX - running - Wed Mar 13 10:46:18 PDT 2019
    # real    26m16.578s

    cat fb.simpleRepeat
    # 1399128075 bases of 28366694468 (4.932%) in intersection

    # add to this simple repeat to windowMasker:
    cd /hive/data/genomes/perManBai1
    /cluster/home/hiram/kent/src/hg/utils/twoBitMask/twoBitMask -add \
       bed/windowMasker/splitFa/perManBai1.cleanWMSdust.2bit \
          bed/simpleRepeat/trfMask.bed perManBai1.2bit

    twoBitToFa perManBai1.2bit stdout | faSize stdin > faSize.perManBai1.2bit.txt

    # real    9m59.223s

    cat faSize.perManBai1.2bit.txt
    egrep "bases|Total|masked" faSize.perManBai1.2bit.txt \
	| fold -w 75 -s  | sed -e 's/^/# /;'
# 32393605577 bases (4026911109 N's 28366694468 real 18209209746 upper 
# 10157484722 lower) in 125724 sequences in 1 files
# Total size: mean 257656.5 sd 973486.9 min 1033 (PGSH01113832v1) max 
# 21669615 (PGSH01077959v1) median 47471
# %31.36 masked total, %35.81 masked real

    # reset symlink
    rm /gbdb/perManBai1/perManBai1.2bit
    ln -s `pwd`/perManBai1.2bit /gbdb/perManBai1/perManBai1.2bit

#############################################################################
# CREATE MICROSAT TRACK (TBD - 2018-07-08 - Hiram)
    ssh hgwdev
    mkdir /cluster/data/perManBai1/bed/microsat
    cd /cluster/data/perManBai1/bed/microsat

    awk '($5==2 || $5==3) && $6 >= 15 && $8 == 100 && $9 == 0 {printf("%s\t%s\t%s\t%dx%s\n", $1, $2, $3, $6, $16);}' \
       ../simpleRepeat/simpleRepeat.bed > microsat.bed

    hgLoadBed perManBai1 microsat microsat.bed
    # Read 56937 elements of size 4 from microsat.bed

#############################################################################
# ucscToINSDC table/track (TBD - 2018-07-10 - Hiram)
    # this is simple since there isn't any RefSeq assembly, and the
    # names used here are different from INSDC only due to the .1 -> v1 switch

    cd /hive/data/genomes/perManBai1/bed/ucscToINSDC

    cut -f1 ../../chrom.sizes | awk '{printf "%s\t%s\n", $1, $1};' \
       | sed -e 's/v1/.1/;' | awk '{printf "%s\t%s\n", $2, $1}' \
          | sort > ucsc.insdc.txt

    join -t$'\t' <(sort -k1,1 ../../chrom.sizes) ucsc.insdc.txt \
            | awk '{printf "%s\t0\t%d\t%s\n", $1, $2, $3}' \
               | sort -k1,1 -k2,2n > ucscToINSDC.bed

    cut -f1 ucscToINSDC.bed | awk '{print length($0)}' | sort -n | tail -1
    # 14
    # use the 14 in this sed
    sed -e "s/21/14/" $HOME/kent/src/hg/lib/ucscToINSDC.sql \
         | hgLoadSqlTab perManBai1 ucscToINSDC stdin ucscToINSDC.bed

    # should cover %100 entirely:
    time featureBits -countGaps perManBai1 ucscToINSDC
    # 32393605577 bases of 32393605577 (100.000%) in intersection
    # real    0m47.018s

##########################################################################
# run up idKeys files for chromAlias (TBD - 2018-07-08 - Hiram)
    mkdir /hive/data/genomes/perManBai1/bed/idKeys
    cd /hive/data/genomes/perManBai1/bed/idKeys

    time (doIdKeys.pl -twoBit=/hive/data/genomes/perManBai1/perManBai1.unmasked.2bit -buildDir=`pwd` perManBai1) > do.log 2>&1 &
    # real    60m51.086s

    cat perManBai1.keySignature.txt
    #   5c21fa448b2fcbcd8249a2b5d84bf460

##########################################################################
# add chromAlias table (TBD - 2018-07-10 - Hiram)

    mkdir /hive/data/genomes/perManBai1/bed/chromAlias
    cd /hive/data/genomes/perManBai1/bed/chromAlias

    hgsql -N -e 'select chrom,name,"genbank" from ucscToINSDC;' perManBai1 \
        > ucsc.genbank.tab

    ~/kent/src/hg/utils/automation/chromAlias.pl ucsc.*.tab \
	> perManBai1.chromAlias.tab

for t in genbank
do
  c0=`cat ucsc.$t.tab | wc -l`
  c1=`grep $t perManBai1.chromAlias.tab | wc -l`
  ok="OK"
  if [ "$c0" -ne "$c1" ]; then
     ok="ERROR"
  fi
  printf "# checking $t: $c0 =? $c1 $ok\n"
done
# checking genbank: 125724 =? 125724 OK


    hgLoadSqlTab perManBai1 chromAlias ~/kent/src/hg/lib/chromAlias.sql \
        perManBai1.chromAlias.tab

#########################################################################
# fixup search rule for assembly track/gold table (TBD - 2018-07-10 - Hiram)

    cd ~/kent/src/hg/makeDb/trackDb/axolotl/perManBai1
    # preview prefixes and suffixes:
    hgsql -N -e "select frag from gold;" perManBai1 \
      | sed -e 's/[0-9][0-9]*//;' | sort | uniq -c
    # sample of output:
#  114752 PGSHv1
#   10972 PGSHv1_1
#    8870 PGSHv1_10
    # ... etc ...

PGSH
    # implies a search rule of: 'PGSH[0-9]+(v1_[0-9]+)?'

    # verify this rule will find them all or eliminate them all:
    hgsql -N -e "select frag from gold;" perManBai1 | wc -l
    # 892108

    hgsql -N -e "select frag from gold;" perManBai1 \
       | egrep -e 'PGSH[0-9]+(v1_[0-9]+)?' | wc -l
    # 892108

    hgsql -N -e "select frag from gold;" perManBai1 \
       | egrep -v -e 'PGSH[0-9]+(v1_[0-9]+)?' | wc -l
    # 0

    # hence, add to trackDb/rhesus/perManBai1/trackDb.ra
searchTable gold
shortCircuit 1
termRegex PGSH[0-9]+(v1_[0-9]+)?
query select chrom,chromStart,chromEnd,frag from %s where frag like '%s%%'
searchPriority 8

    git commit -m 'add gold table search rule refs #19859' trackDb.ra

    # verify searches work in the position box

##########################################################################
## WINDOWMASKER (TBD - 2019-03-13 - Hiram)

    mkdir /hive/data/genomes/perManBai1/bed/windowMasker
    cd /hive/data/genomes/perManBai1/bed/windowMasker
    time (doWindowMasker.pl -buildDir=`pwd` -workhorse=hgwdev \
        -dbHost=hgwdev perManBai1) > do.log 2>&1
    # real    2857m15.802s

    # broke down due to broken twoBitMask situation
    # this procedure was carried out on a split setup of sequences
    mkdir  /hive/data/genomes/perManBai1/bed/windowMasker/splitFa
    cd  /hive/data/genomes/perManBai1/bed/windowMasker/splitFa
    # generate lists of contigs up to 2Gb in size
    ~/kent/src/hg/makeDb/doc/perManBai1/twoGbChunks.pl
    # turning to fasta done in a small cluster job on hgwdev
    printf '#!/bin/bash
# input is one of chunkN.list
export c=$1
export chunk=`echo $c | sed -e 's/.list//;'`

fa="$chunk/$chunk.fa.gz"
mkdir -p $chunk
# echo $c $fa
faSomeRecords perManBai1.fa $c stdout | gzip -c > ${fa}
' > mkChunkFa.sh
    chmod +x mkChunkFa.sh

    printf '#LOOP
./mkChunkFa.sh $(path1)
#ENDLOOP
' > split.template
    ls chunk*.list > run.list
    gensub2 run.list single split.template jobList
    para create jobList
    para push
    cat run.time
# Completed: 16 of 16 jobs
# CPU time in finished jobs:      11256s     187.59m     3.13h    0.13d  0.000 y
# IO & Wait Time:                     0s       0.00m     0.00h    0.00d  0.000 y
# Average job time:                 679s      11.31m     0.19h    0.01d
# Longest finished job:             790s      13.17m     0.22h    0.01d
# Submission to last job:           795s      13.25m     0.22h    0.01d
    # then turn those fasta into 2bit files:
    ls -d chunk[0-9] chunk1[0-9] | while read D
do
  ls -ld ${D}/*.fa.gz
  echo faToTwoBit ${D}/${D}.fa.gz ${D}/${D}.2bit
  faToTwoBit ${D}/${D}.fa.gz ${D}/${D}.2bit &
done
wait
    #  Then, a second batch to run WM on each chunk
    printf '#!/bin/bash

set -beEu -o pipefail

export chunk=$1

cd /hive/data/genomes/perManBai1/bed/windowMasker/splitFa/${chunk}
time (doWindowMasker.pl -buildDir=`pwd` -unmaskedSeq=`pwd`/${chunk}.2bit \
   -stop=twobit -workhorse=hgwdev perManBai1) > do.log 2>&1
' > runWM.sh
    chmod +x runWM.sh

    printf '#LOOP
runWM.sh $(path1)
#ENDLOOP
' > wm.template

    ls -d chunk[0-9] chunk1[0-9] > sm.list
    gensub2 wm.list single wm.template wm.jobList
    para create jobList
    para push
# Completed: 16 of 16 jobs
# CPU time in finished jobs:          3s       0.04m     0.00h    0.00d  0.000 y
# IO & Wait Time:                173439s    2890.66m    48.18h    2.01d  0.005 y
# Average job time:               10840s     180.67m     3.01h    0.13d
# Longest finished job:           13596s     226.60m     3.78h    0.16d
# Submission to last job:         13601s     226.68m     3.78h    0.16d

    # all results together:
    cat chunk*/*.sdust.bed > /dev/shm/perManBai1.sdust.bed
    $HOME/bin/x86_64/gnusort -S100G --parallel=32 -k1,1 -k2,2n \
         /dev/shm/perManBai1.sdust.bed > perManBai1.windowmasker.sdust.bed

    # temporary fixed twoBitMask
    /cluster/home/hiram/kent/src/hg/utils/twoBitMask/twoBitMask \
        /hive/data/genomes/perManBai1/perManBai1.unmasked.2bit \
            perManBai1.windowmasker.sdust.bed perManBai1.wmsk.sdust.2bit
    twoBitToFa perManBai1.wmsk.sdust.2bit stdout | faSize stdin \
        > faSize.perManBai1.wmsk.sdust.txt 2>&1

    # Masking statistics
    cat faSize.perManBai1.cleanWMSdust.txt
# 32393605577 bases (4026911109 N's 28366694468 real 18214135230
#	upper 10152559238 lower) in 125724 sequences in 1 files
# Total size: mean 257656.5 sd 973486.9 min 1033 (PGSH01113832v1)
#	max 21669615 (PGSH01077959v1) median 47471
# %31.34 masked total, %35.79 masked real

    hgLoadBed perManBai1 windowmaskerSdust perManBai1.windowmasker.sdust.bed
    featureBits -countGaps perManBai1 windowmaskerSdust \
	> fb.perManBai1.windowmaskerSdust.beforeClean.txt 2>&1
    featureBits perManBai1 -not gap -bed=notGap.bed
    featureBits perManBai1 windowmaskerSdust notGap.bed -bed=stdout \
	| gzip -c > cleanWMask.bed.gz
    hgLoadBed perManBai1 windowmaskerSdust cleanWMask.bed.gz
    featureBits -countGaps perManBai1 windowmaskerSdust \
	> fb.perManBai1.windowmaskerSdust.clean.txt 2>&1

    zcat cleanWMask.bed.gz \
	| /cluster/home/hiram/kent/src/hg/utils/twoBitMask/twoBitMask \
	    /hive/data/genomes/perManBai1/perManBai1.unmasked.2bit stdin \
		-type=.bed perManBai1.cleanWMSdust.2bit

    twoBitToFa perManBai1.cleanWMSdust.2bit stdout \
	| faSize stdin > faSize.perManBai1.cleanWMSdust.txt 2>&1
    featureBits -countGaps perManBai1 rmsk windowmaskerSdust \
	> fb.perManBai1.rmsk.windowmaskerSdust.txt 2>&1

    cat fb.perManBai1.windowmaskerSdust.clean.txt
    # 10152559238 bases of 32393605577 (31.341%) in intersection

    # rmsk is so sparse, it doesn't even overlap any of this
    cat fb.perManBai1.rmsk.windowmaskerSdust.txt
    # 0 bases of 32393605577 (0.000%) in intersection

##########################################################################
# cpgIslands - (TBD - 2018-07-08 - Hiram)
    mkdir /hive/data/genomes/perManBai1/bed/cpgIslands
    cd /hive/data/genomes/perManBai1/bed/cpgIslands
    time (doCpgIslands.pl -dbHost=hgwdev -bigClusterHub=ku \
      -workhorse=hgwdev -smallClusterHub=ku perManBai1) > do.log 2>&1
    # real    100m4.369s

    cat fb.perManBai1.cpgIslandExt.txt
    # 352400331 bases of 28366694468 (1.242%) in intersection

##############################################################################
# ncbiRefSeq gene track (TBD - 2018-03-15 - Hiram)
    mkdir /hive/data/genomes/perManBai1/bed/ncbiRefSeq
    cd /hive/data/genomes/perManBai1/bed/ncbiRefSeq

    time (~/kent/src/hg/utils/automation/doNcbiRefSeq.pl \
  -buildDir=`pwd` -bigClusterHub=ku \
  -fileServer=hgwdev -workhorse=hgwdev -smallClusterHub=ku -dbHost=hgwdev \
      refseq vertebrate_mammalian Neomonachus_schauinslandi \
         GCF_002201575.1_ASM220157v1 perManBai1) > do.log 2>&1
    # real    4m37.395s

    cat fb.ncbiRefSeq.perManBai1.txt
    # 45781082 bases of 2400839308 (1.907%) in intersection

##############################################################################
# genscan - (TBD - 2018-07-08 - Hiram)
    mkdir /hive/data/genomes/perManBai1/bed/genscan
    cd /hive/data/genomes/perManBai1/bed/genscan
    time (doGenscan.pl -buildDir=`pwd` -workhorse=hgwdev -dbHost=hgwdev \
      -bigClusterHub=ku perManBai1) > do.log 2>&1
    # fifty-six were broken at window size of 2400000,
    #     they all worked at 2000000
    # real    162m46.438s

    # continuing:
    time (doGenscan.pl -buildDir=`pwd` -workhorse=hgwdev -dbHost=hgwdev \
      -continue=makeBed -bigClusterHub=ku perManBai1) > makeBed.log 2>&1
    # real    75m28.038s

    cat fb.perManBai1.genscan.txt
    #   896393757 bases of 28366694468 (3.160%) in intersection

    cat fb.perManBai1.genscanSubopt.txt
    #   978384685 bases of 28366694468 (3.449%) in intersection

#############################################################################
# augustus gene track (TBD - 2018-07-08 - Hiram)

    mkdir /hive/data/genomes/perManBai1/bed/augustus
    cd /hive/data/genomes/perManBai1/bed/augustus
    time (doAugustus.pl -buildDir=`pwd` -bigClusterHub=ku \
        -species=zebrafish -dbHost=hgwdev \
           -workhorse=hgwdev perManBai1) > do.log 2>&1
    # real    443m58.621s

    # continuing after removing some broken items:
    time (doAugustus.pl -buildDir=`pwd` -bigClusterHub=ku \
        -species=zebrafish -dbHost=hgwdev \
           -continue=load -workhorse=hgwdev perManBai1) > load.log 2>&1

    cat fb.perManBai1.augustusGene.txt
    # 828245083 bases of 28366694468 (2.920%) in intersection

#############################################################################
# Create kluster run files (TBD - 2018-07-10 - Hiram)

    # obtain 'real' numbers of this assembly:
    cd /hive/data/genomes/perManBai1
    head -1 faSize.perManBai1.2bit.txt
# 32393605577 bases (4026911109 N's 28366694468 real 18209209746 upper
#	10157484722 lower) in 125724 sequences in 1 files

    # numerator is perManBai1 gapless bases "real"

    # denominator is hg19 gapless bases as reported by:
    #   featureBits -noRandom -noHap hg19 gap
    #     234344806 bases of 2861349177 (8.190%) in intersection
    # 1024 is threshold used for human -repMatch:
    calc \( 28366694468 / 2861349177 \) \* 1024
    # ( 28366694468 / 2861349177 ) * 1024 = 10151.677876

    # tried repMatch=10000, but it didn't make many
    # ==> use -repMatch=10000 according to size scaled down from 1024 for human.
    #   and rounded down to nearest 100
    cd /hive/data/genomes/perManBai1
    time blat perManBai1.2bit \
         /dev/null /dev/null -tileSize=11 -makeOoc=jkStuff/perManBai1.11.ooc \
        -repMatch=10000
    # Wrote 6435 overused 11-mers to jkStuff/perManBai1.11.ooc
    # real    9m44.626s

    # at repMatch=1000 it makes many more:
    # Wrote 1885128 overused 11-mers to jkStuff/perManBai1.11.ooc

    # at repMatch=2000
    # real    9m18.200s
    # Wrote 740741 overused 11-mers to jkStuff/perManBai1.11.ooc

    # at repMatch=4000
    # Wrote 125201 overused 11-mers to jkStuff/perManBai1.11.ooc
    # real    8m38.816s

    # going to use 3000 since this makes about 10X more than in human
    # at repMatch=3000
    # Wrote 286064 overused 11-mers to jkStuff/perManBai1.11.ooc
    # real    9m7.973s

    #	there are no non-bridged gaps since these are all fake gaps,
    #   However, there are near 100,000 gaps over 10,000 bases,
    # going to use those gaps as 'non-bridged' gaps
    # Size survey:
    hgsql -N -e 'select size from gap;' perManBai1 | ave stdin
# Q1 1701.000000
# median 3331.000000
# Q3 6422.000000
# average 5254.430036
# min 1.000000
# max 289789.000000
# count 766384
# total 4026911109.000000

    #   check non-bridged gaps to see what the typical size is:
#     hgsql -N \
#         -e 'select * from gap where bridge="no" order by size;' perManBai1 \
#         | sort -k7,7nr
    #   minimum size is 10000, added allowBridged option to allow this
     ~/kent/src/hg/utils/gapToLift/gapToLift -allowBridged -verbose=2 \
	-minGap=10000 perManBai1 \
 	jkStuff/perManBai1.10Kgaps.lft -bedFile=jkStuff/perManBai1.10Kgaps.bed

#     gapToLift -verbose=2 -minGap=50000 perManBai1 \
# 	jkStuff/perManBai1.nonBridged.lft -bedFile=jkStuff/perManBai1.nonBridged.bed

#########################################################################
# lastz/chain/net swap from hg38 (TBD - 2018-08-09 - Hiram)

    # alignment to hg38:
    cd /hive/data/genomes/hg38/bed/lastzPerManBai1.2018-07-09
    cat fb.hg38.chainPerManBai1Link.txt
    # 54520910 bases of 3049335806 (1.788%) in intersection

    cat fb.hg38.chainSynPerManBai1Link.txt
    # 3343407 bases of 3049335806 (0.110%) in intersection

    cat fb.hg38.chainRBest.PerManBai1.txt
    # 37383183 bases of 3049335806 (1.226%) in intersection

    # and for the swap:
    mkdir /hive/data/genomes/perManBai1/bed/blastz.hg38.swap
    cd /hive/data/genomes/perManBai1/bed/blastz.hg38.swap

    time (doBlastzChainNet.pl -verbose=2 \
      /hive/data/genomes/hg38/bed/lastzPerManBai1.2018-07-09/DEF \
        -swap -chainMinScore=5000 -chainLinearGap=loose \
          -workhorse=hgwdev -smallClusterHub=ku -bigClusterHub=ku \
            -syntenicNet) > swap.log 2>&1 &
    #  real    105m38.443s

    cat fb.perManBai1.chainHg38Link.txt
    # 59846443 bases of 28366694468 (0.211%) in intersection

    cat fb.perManBai1.chainSynHg38Link.txt
    # 3456707 bases of 28366694468 (0.012%) in intersection

    time (doRecipBest.pl -load -workhorse=hgwdev -buildDir=`pwd` perManBai1 hg38) > rbest.log 2>&1 &
    #  real    555m51.873s

    cat fb.perManBai1.chainRBest.Hg38.txt
    # 38573370 bases of 28366694468 (0.136%) in intersection

#############################################################################
# lastz/chain/net swap from mm10 (TBD - 2018-06-10 - Hiram)

    # alignment to mm10
    cd /hive/data/genomes/mm10/bed/lastzPerManBai1.2018-07-09
    cat fb.mm10.chainPerManBai1Link.txt
    # 52143617 bases of 2652783500 (1.966%) in intersection

    cat fb.mm10.chainSynPerManBai1Link.txt
    # 2686570 bases of 2652783500 (0.101%) in intersection

    cat fb.mm10.chainRBest.PerManBai1.txt
    # 36938030 bases of 2652783500 (1.392%) in intersection

    # and for the swap:
    mkdir /hive/data/genomes/perManBai1/bed/blastz.mm10.swap
    cd /hive/data/genomes/perManBai1/bed/blastz.mm10.swap

    time (doBlastzChainNet.pl -verbose=2 \
      /hive/data/genomes/mm10/bed/lastzPerManBai1.2018-07-09/DEF \
        -swap -chainMinScore=5000 -chainLinearGap=loose \
          -workhorse=hgwdev -smallClusterHub=ku -bigClusterHub=ku \
            -syntenicNet) > swap.log 2>&1 &
    #  real    39m28.757s

    cat fb.perManBai1.chainMm10Link.txt
    # 87124587 bases of 28366694468 (0.307%) in intersection

    cat fb.perManBai1.chainSynMm10Link.txt
    # 2893381 bases of 28366694468 (0.010%) in intersection

    time (doRecipBest.pl -load -workhorse=hgwdev -buildDir=`pwd` perManBai1 mm10) > rbest.log 2>&1 &
    # real    568m10.621s

    # something odd went haywire at the download step
    time (doRecipBest.pl -load -continue=download -workhorse=hgwdev -buildDir=`pwd` perManBai1 mm10) > download.log 2>&1 &
    # real    3m16.404s

    cat fb.perManBai1.chainRBest.Mm10.txt
    # 38584422 bases of 28366694468 (0.136%) in intersection

##############################################################################
# GENBANK AUTO UPDATE (TBD - 2018-07-10 - Hiram)
    ssh hgwdev
    cd $HOME/kent/src/hg/makeDb/genbank
    git pull
    # /cluster/data/genbank/data/organism.lst shows:
    # organism           mrnaCnt estCnt  refSeqCnt
    # Ambystoma mexicanum   7705  43326      0

    # edit etc/genbank.conf to add perManBai1 just before nanPar1

# perManBai1 (Axolotl - Ambystoma mexicanum) 125724 scaffolds N50 3052786 - 30Gb total
perManBai1.serverGenome = /hive/data/genomes/perManBai1/perManBai1.2bit
perManBai1.clusterGenome = /hive/data/genomes/perManBai1/perManBai1.2bit
perManBai1.ooc = /hive/data/genomes/perManBai1/jkStuff/perManBai1.11.ooc
perManBai1.lift = /hive/data/genomes/perManBai1/jkStuff/perManBai1.10Kgaps.lft
perManBai1.perChromTables = no
perManBai1.downloadDir = perManBai1
perManBai1.refseq.mrna.xeno.pslCDnaFilter    = ${lowCover.refseq.mrna.xeno.pslCDnaFilter}
perManBai1.refseq.mrna.native.pslCDnaFilter  = ${lowCover.refseq.mrna.native.pslCDnaFilter}
perManBai1.genbank.mrna.native.pslCDnaFilter = ${lowCover.genbank.mrna.native.pslCDnaFilter}
perManBai1.genbank.mrna.xeno.pslCDnaFilter   = ${lowCover.genbank.mrna.xeno.pslCDnaFilter}
perManBai1.genbank.est.native.pslCDnaFilter  = ${lowCover.genbank.est.native.pslCDnaFilter}
perManBai1.genbank.est.xeno.pslCDnaFilter    = ${lowCover.genbank.est.xeno.pslCDnaFilter}
# defaults yes: genbank.mrna.native.load genbank.mrna.native.loadDesc
# yes: genbank.est.native.load refseq.mrna.native.load
# yes: refseq.mrna.native.loadDesc refseq.mrna.xeno.load
# yes: refseq.mrna.xeno.loadDesc
# defaults no: genbank.mrna.xeno.load genbank.mrna.xeno.loadDesc
# no: genbank.est.native.loadDesc genbank.est.xeno.load
# no: genbank.est.xeno.loadDesc
# DO NOT NEED genbank.mrna.xeno except for human, mouse
# perManBai1.upstreamGeneTbl = ensGene
# perManBai1.upstreamMaf = multiz6way /hive/data/genomes/perManBai1/bed/multiz6way/species.list

# And edit src/lib/gbGenome.c to add new species
#  static char *ambMexNames[] = {"Ambystoma mexicanum", NULL};

    git commit -m 'adding perManBai1 Axolotl - Ambystoma mexicanum' src/lib/gbGenome.c etc/genbank.conf
    git push

    # verify stated file paths do exist:
    grep perManBai1 etc/genbank.conf | egrep "Genome|ooc|lift" \
	| awk '{print $NF}' | grep -v -w no | xargs ls -og
-rw-rw-r-- 1 9104343295 Jul  8 19:32 /hive/data/genomes/perManBai1/perManBai1.2bit
-rw-rw-r-- 1   11166819 Jul 10 10:45 /hive/data/genomes/perManBai1/jkStuff/perManBai1.10Kgaps.lft
-rw-rw-r-- 1    1144264 Jul 10 10:00 /hive/data/genomes/perManBai1/jkStuff/perManBai1.11.ooc

    # update /cluster/data/genbank/:
    make etc-update
    make install-server

    # add perManBai1 to:
    #   etc/align.dbs etc/hgwdev.dbs
    git commit -m 'adding perManBai1/axolotl refs #21715' \
	etc/align.dbs etc/hgwdev.dbs
    git push
    # update /cluster/data/genbank/:
    make etc-update

    # XXX a few days later the genbank tables will be in the database

##############################################################################
#  BLATSERVERS ENTRY (TBD - 2017-10-06 - Hiram)
#	After getting a blat server assigned by the Blat Server Gods,
    ssh hgwdev

    hgsql -e 'INSERT INTO blatServers (db, host, port, isTrans, canPcr) \
	VALUES ("perManBai1", "blat1c", "17880", "1", "0"); \
	INSERT INTO blatServers (db, host, port, isTrans, canPcr) \
	VALUES ("perManBai1", "blat1c", "17881", "0", "1");' \
	    hgcentraltest
    #	test it with some sequence

##############################################################################
# set default position to the PAX3 gene (TBD - 2018-07-11 - Hiram)
#  as found via UCSC 'Other refseq' track

    hgsql -e \
'update dbDb set defaultPos="PGSH01084259v1:1812985-2038932" where name="perManBai1";' \
	hgcentraltest

##############################################################################
# all.joiner update, downloads and in pushQ - (TBD - 2018-07-13 - Hiram)
    cd $HOME/kent/src/hg/makeDb/schema

    ~/kent/src/hg/utils/automation/verifyBrowser.pl perManBai1
# 40 tables in database perManBai1 - Axolotl, Ambystoma mexicanum
# verified 40 tables in database perManBai1, 0 extra tables, 12 optional tables
# chainNetRBestHg38     3 optional tables
# chainNetRBestMm10     3 optional tables
# chainNetSynHg38       3 optional tables
# chainNetSynMm10       3 optional tables
# ERROR: no genbank tables found
# verified 28 required tables, 1 missing tables
# 1     ucscToRefSeq    - missing table
# hg38 chainNet to perManBai1 found 3 required tables
# mm10 chainNet to perManBai1 found 3 required tables
# hg38 chainNet RBest and syntenic to perManBai1 found 6 optional tables
# mm10 chainNet RBest and syntenic to perManBai1 found 3 optional tables
# ERROR: blat server not found in hgcentraltest.blatServers ?

    # fixup all.joiner until this is a clean output
    joinerCheck -database=perManBai1 -tableCoverage all.joiner
    joinerCheck -database=perManBai1 -times all.joiner
    joinerCheck -database=perManBai1 -keys all.joiner

    # need to specify workhorse so it will not overload ku
    # and this procedure fails on ku due to limited memory for the
    # maskOutFa step
    cd /hive/data/genomes/perManBai1
    time (makeDownloads.pl -workhorse=hgwdev perManBai1) > downloads.log 2>&1
    # real    117m17.756s

    # continue after broken 'compress' step manually completed:
    time (makeDownloads.pl -continue=install -workhorse=hgwdev perManBai1) \
	> downloadsInstall.log 2>&1
    # real    0m4.786s

    #   now ready for pushQ entry
    mkdir /hive/data/genomes/perManBai1/pushQ
    cd /hive/data/genomes/perManBai1/pushQ
    time (makePushQSql.pl -redmineList perManBai1) \
	> perManBai1.pushQ.sql 2> stderr.out
    # real    4m11.149s

    # remove the tandemDups and gapOverlap from the listings:
    sed -i -e "/tandemDups/d" redmine.perManBai1.table.list
    sed -i -e "/Tandem Dups/d" redmine.perManBai1.releaseLog.txt
    sed -i -e "/gapOverlap/d" redmine.perManBai1.table.list
    sed -i -e "/Gap Overlaps/d" redmine.perManBai1.releaseLog.txt

    #   check for errors in stderr.out, some are OK, e.g.:
    # writing redmine listings to
    # redmine.perManBai1.file.list
    # redmine.perManBai1.table.list
    # redmine.perManBai1.releaseLog.txt
    # WARNING: perManBai1 does not have seq
    # WARNING: perManBai1 does not have extFile

    # verify the file listings are valid, should be no output to stderr:
    cat redmine.perManBai1.file.list \
        | while read L; do ls -ogL $L; done  > /dev/null

    # to verify the database.table list is correct, should be the same
    # line count for these two commands:
    wc -l redmine.perManBai1.table.list
    # 41 redmine.perManBai1.table.list
    awk -F'.' '{
printf "hgsql -N -e \"show table status like '"'"'%s'"'"';\" %s\n", $2, $1
}' redmine.perManBai1.table.list | while read L; do eval $L; done | wc -l
    # 41

# add the path names to the listing files in the redmine issue
    # in the three appropriate entry boxes:
    ls `pwd`/redmine*

/hive/data/genomes/perManBai1/pushQ/redmine.perManBai1.file.list
/hive/data/genomes/perManBai1/pushQ/redmine.perManBai1.releaseLog.txt
/hive/data/genomes/perManBai1/pushQ/redmine.perManBai1.table.list

#########################################################################
