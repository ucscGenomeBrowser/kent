#!/usr/bin/perl -w
use strict;

#
# This script creates the zoo browser databases
# It runs in 2 large main loops. The first loop distributes input files and sets up
# blat jobs. It may need some babysitting or incremental running to make sure
# that it works correctly since it does a lot. Uncomment each track are you want to 
# create. The first loop works only when run on kk, because some of the data
# it references lives there.
# After you create the blat job input files you need to ssh to kk and run the blat jobs by hand.
# I have not scripted this because it requires lots of user intervention.
# After the blat jobs are done you can comment out the first loop and run the second loop.
# This loads the database with the blat results.
#




# Hash keyed by genus species, containing values indicating the project name
my %orgHash = (
    "Papio_cynocephalus_anubis" => "zooBaboon3",
    "Felis_catus" => "zooCat3",
    "Gallus_gallus" => "zooChicken3",
    "Pan_troglodytes" => "zooChimp3",
    "Bos_taurus" => "zooCow3",
    "Canis_familiaris" => "zooDog3",
    "Takifugu_rubripes" => "zooFugu3",
    "Homo_sapiens" => "zooHuman3",
    "Mus_musculus" => "zooMouse3", 
    "Sus_scrofa" => "zooPig3",
    "Rattus_norvegicus" => "zooRat3",
    "Tetraodon_nigroviridis" => "zooTetra3",
    "Danio_rerio" => "zooZebrafish3",       
               );

# Hash keyed by genus species, containing values indicating the common organism name
my %orgNameHash = (
    "Papio_cynocephalus_anubis" => "baboon",
    "Felis_catus" => "cat",
    "Gallus_gallus" => "chicken",
    "Pan_troglodytes" => "chimp",
    "Bos_taurus" => "cow",
    "Canis_familiaris" => "dog",
    "Takifugu_rubripes" => "fugu",
    "Homo_sapiens" => "human",
    "Mus_musculus" => "mouse", 
    "Sus_scrofa" => "pig",
    "Rattus_norvegicus" => "rat",
    "Tetraodon_nigroviridis" => "tetra",
    "Danio_rerio" => "zfish",
                   );

my $zooDir = "/cluster/store2/zoo3";
my $mrnaRoot = "/cluster/store1/mrna.130/org";
my $xenoRoot = "/cluster/store1/mrna.130/zoo/work";
my $refSeqDir = "/cluster/store1/mrna.130/refSeq";
my $org;
my $orgName;
my $dbName;
my $dirName;
my $blatDir;
my $workingDir;

# One time step - create all the databases
##run("mysql -u SECRET_USER -pBIG_SECRET -A < ${zooDir}/createDbs.sql\n");

# Set up the blat and repeat masker directories
##run("rm -rf work");
##run("mkdir -p work");
##run("mkdir -p work/blat");
##run("mkdir -p work/blatFish");
##run("mkdir -p work/simpleRepeat");
##run("mkdir -p work/xeno");
##run("mkdir -p work/refSeq");
##run("cp scripts/blatFish-gsub work/blatFish/gsub");
##run("cp scripts/simpleRepeat-gsub work/simpleRepeat/gsub");

for $org (keys(%orgHash)) {
    $dirName = $dbName = $orgHash{$org};
    $orgName = $orgNameHash{$org};

# pjt.pl is a one-off script for a prorietary manually curated data set 
#  I got from Pamela Jacques-Thomas
##    run("$zooDir/pjt.pl data/$orgName/${orgName}_target1.annot.ff ${orgName}_pjt");
##    run("hgsql $dbName < sql/dropCrap.sql");
##    run("hgLoadBed -noBin $dbName pjt_gene ${orgName}_pjt_all.bed");
##    run("hgLoadBed -noBin $dbName pjt_genscan ${orgName}_pjt_genscan.bed");
 
##    run("mkdir -p $dirName");
##    run("mkdir -p $dirName/bed");
##    run("mkdir -p $dirName/1");
##    run("mkdir -p $dirName/bed/rmskS");
##    run("cp $zooDir/data/${orgName}/repeat/${orgName}_target1.fasta.masked $dirName/1/target1.fa");
##    run("cp $zooDir/data/${orgName}/repeat/${orgName}_target1.fasta.out $dirName/bed/rmskS/target1.fa.out");
##    run("cp $zooDir/data/${orgName}/repeat/${orgName}_target1.fasta.masked $dirName/1/target1.fa");
##    run("cp $zooDir/data/${orgName}/repeat/${orgName}_target1.fasta.out $dirName/bed/rmskS/target1.fa.out");
    run("cp $zooDir/data/${orgName}/${orgName}_target1.agp $dirName/1/target1.agp");
    run("hgGoldGapGl $dbName -noGl $zooDir $dirName");
# Uncomment this next line only for a major db rebuild
# Add the trackDb database table

##    run("hgLoadRna drop $dbName");
##    run("hgTrackDb $dbName trackDb ~/kent/src/hg/lib/trackDb.sql ~/kent/src/hg/makeDb/hgTrackDb/hgRoot");

# Create the zooSeq ordering table
##    run("hgsql $dbName < sql/zooSeqOrder.sql");

##    run("hgLoadRna new $dbName");
##    run("hgLoadRna add -type=mRna $dbName $mrnaRoot/$org/mrna.fa $mrnaRoot/$org/mrna.ra");

##    if (-e "$refSeqDir/$orgName/refSeq.fa") {
##        run("hgLoadRna add -type=refSeq $dbName $refSeqDir/$orgName/refSeq.fa $refSeqDir/$orgName/refSeq.ra");
##     }

# If the file exists, load it
##    if (-e "$mrnaRoot/$org/est.fa") {
##       run("hgLoadRna add -type=EST $dbName $mrnaRoot/$org/est.fa $mrnaRoot/$org/est.ra");
##   } else {
##       print("No est file in $mrnaRoot/$org/est.fa\n");
##   }

## run("hgLoadRna add -type=xenoRna $dbName /cluster/store1/mrna.129/zoo/work/${orgName}XenoRna.fa /cluster/store1/mrna.129/zoo/work/${orgName}XenoRna.ra");
## run("hgLoadRna add -type=xenoEst $dbName /cluster/store1/mrna.129/zoo/work/${orgName}XenoEst.fa /cluster/store1/mrna.129/zoo/work/${orgName}XenoEst.ra");

##    foreach $blatDir ("mrna", "est") {
##        if (-e "$mrnaRoot/$org/$blatDir.fa") {
##           run("mkdir -p $dirName/bed/$blatDir");
##           run("mkdir -p $dirName/bed/$blatDir/psl");            
            # Make the mrna/est alignment BLAT spec file
##           run("echo /cluster/home/kent/bin/i386/blat -t=dna -q=rna -ooc=/cluster/home/kent/hg/h/11.ooc mask=lower $zooDir/$dirName/1/target1.fa $mrnaRoot/$org/${blatDir}.fa $zooDir/$dirName/bed/${blatDir}/psl/target1_${blatDir}.psl >> work/blat/spec");
##        }
##    }

# Set up refSeq track if a refSeq sequence exists
##        if (-e "$refSeqDir/$orgName/$orgName.fa") {
##           run("mkdir -p $dirName/bed/refSeq");
##           run("mkdir -p $dirName/bed/refSeq/psl");            
##           run("echo /cluster/home/kent/bin/i386/blat -t=dna -q=rna -ooc=/cluster/home/kent/hg/h/11.ooc mask=lower $zooDir/$dirName/1/target1.fa $refSeqDir/$orgName/refSeq.fa $zooDir/$dirName/bed/refSeq/psl/target1_refSeq.psl >> work/refSeq/spec");
##}

##    run("mkdir -p $dirName/bed/xenoRna");
##    run("mkdir -p $dirName/bed/xenoRna/psl");
##    run("mkdir -p $dirName/bed/xenoEst");
##    run("mkdir -p $dirName/bed/xenoEst/psl");
##    run("echo /cluster/home/kent/bin/i386/blat -t=dna -q=rna mask=lower {check in line+ $zooDir/$dirName/1/target1.fa} {check in line+ $xenoRoot/${orgName}XenoRna.fa} {check out line+  $zooDir/$dirName/bed/xenoRna/psl/target1_xenoRna.psl} >> work/xeno/spec");
##    run("echo /cluster/home/kent/bin/i386/blat -t=dna -q=rna mask=lower {check in line+ $zooDir/$dirName/1/target1.fa} {check in line+ $xenoRoot/${orgName}XenoEst.fa} {check out line+  $zooDir/$dirName/bed/xenoEst/psl/target1_xenoEst.psl} >> work/xeno/spec");

    
# Load repeat masker and GCPercent tracks
##   run("scripts/makeNib.csh $dirName");
##   run("hgLoadOut $dbName $zooDir/$dirName/bed/rmskS/*.fa.out");
##   run("mysql -u SECRET_USER -pBIG_SECRET -A $dbName  < ~/kent/src/hg/lib/chromInfo.sql");
##   run("hgNibSeq -preMadeNib $dbName $zooDir/$dirName/nib $zooDir/$dirName/?/*.fa");
##   run("hgGoldGapGl $dbName $zooDir/ $dirName -noGl");

    # Make and load GC percent table
##   run("mkdir -p $zooDir/$dirName/bed/gcPercent");
##   run("mysql -u SECRET_USER -pBIG_SECRET -A $dbName < ~/src/hg/lib/gcPercent.sql");
##   run("hgGcPercent $dbName $dirName/nib");
##   run("mv gcPercent.bed $dirName/bed/gcPercent");

##    run("mkdir -p $dirName/bed/blatFish");
##    run("mkdir -p $dirName/bed/blatFish/psl");
##    run("mkdir -p $dirName/bed/blatFish/chrom");
##    run("echo $zooDir/$dirName | wordLine stdin >> work/blatFish/genome.lst");

##    run("ls -1S $zooDir/$dirName/ >> work/simpleRepeat/genome.lst"); # HACK - need to generalize the gsub file
##    run("mkdir -p $dirName/bed/simpleRepeat");
##    run("mkdir -p $dirName/bed/simpleRepeat/trf");

##    run("gbToFaRa dna.fil $orgName.fa $orgName.ra $orgName.ta data/$orgName/${orgName}_target1.annot.ff");

##    if ($dbName eq "zooHuman3") {
##        run("hgLoadBed $dbName mcs_u $zooDir/data/bed/mcs_u.bed");
##        run("hgLoadBed $dbName mcs_b $zooDir/data/bed/mcs_b.bed");
##        run("hgLoadBed $dbName dhs $zooDir/data/bed/dhs.bed");
##    }
} # END OF FIRST WHILE LOOP

# The line below only works on kk - that's where the fish is stored
##run("ls -1S /scratch/hg/tetFish/*.fa > work/blatFish/blatFish.lst");
##run("gensub2 work/blatFish/genome.lst work/blatFish/blatFish.lst work/blatFish/gsub work/blatFish/spec");
##run("gensub2 work/simpleRepeat/genome.lst single work/simpleRepeat/gsub work/simpleRepeat/spec");









# NOW RUN BLAT BY HAND USING PARASOL
# Second main loop iteration
# This will work ONLY AFTER BLAT is run and you have results in the desired output dirs

for $org (keys(%orgHash)) {
    $dirName = $dbName = $orgHash{$org};
    $orgName = $orgNameHash{$org};

    # Process psl alignment files
#    run("cp $dirName/bed/mrna/psl/target1_mrna.psl $dirName/bed/mrna/chrom/target1_mrna.psl");

#    if (-e "$dirName/bed/est") {
#        run("cp $dirName/bed/est/psl/target1_est.psl $dirName/bed/est/chrom/target1_est.psl");
#    }
    
    $workingDir = "$dirName/bed/mrna";
##    run("pslSort dirs $workingDir/raw.psl /tmp $workingDir/psl");
##    run("pslReps -minAli=0.98 -sizeMatters -nearTop=0.005 $workingDir/raw.psl $workingDir/all_mrna.psl /dev/null");
##    run("pslSortAcc nohead $workingDir/chrom /tmp $workingDir/all_mrna.psl");

    $workingDir = "$dirName/bed/est";
    if (-e $workingDir) {
##        run("pslSort dirs $workingDir/raw.psl /tmp $workingDir/psl");
##        run("pslReps -minAli=0.98 -sizeMatters -nearTop=0.005 $workingDir/raw.psl $workingDir/all_est.psl /dev/null");
##        run("pslSortAcc nohead $workingDir/chrom /tmp $workingDir/all_est.psl");
    }

    $workingDir = "$dirName/bed/refSeq";
    if (-e "$workingDir/psl") {
##        run("pslSort dirs $workingDir/raw.psl /tmp $workingDir/psl");
##        run("pslReps -minCover=0.2 -sizeMatters -minAli=0.98 -nearTop=0.002 $workingDir/raw.psl $workingDir/all_refSeq.psl /dev/null");
##        run("pslSortAcc nohead $workingDir/chrom /tmp $workingDir/all_refSeq.psl");
    }

    $workingDir = "$dirName/bed/xenoRna";
##    run("pslSort dirs $workingDir/raw.psl /tmp $workingDir/psl");
##    run("pslReps -minAli=0.25 $workingDir/raw.psl $workingDir/xenoMrna.psl /dev/null");
##    run("pslSortAcc nohead $workingDir/chrom /tmp $workingDir/xenoMrna.psl");

    $workingDir = "$dirName/bed/xenoEst";
##    run("pslSort dirs $workingDir/raw.psl /tmp $workingDir/psl");
##    run("pslReps -minAli=0.25 $workingDir/raw.psl $workingDir/xenoEst.psl /dev/null");
##    run("pslSortAcc nohead $workingDir/chrom /tmp $workingDir/xenoEst.psl");

    # Load psl alignment files into the DB
    $workingDir = "$dirName/bed/mrna";
##    run("hgLoadPsl $dbName $workingDir/chrom/*.psl");
##    run("hgLoadPsl $dbName $workingDir/all_mrna.psl -nobin");

    $workingDir = "$dirName/bed/est";
    if (-e $workingDir) {
##    run("hgLoadPsl $dbName $workingDir/chrom/*.psl");
##    run("hgLoadPsl $dbName $workingDir/all_est.psl -nobin");
    }

    $workingDir = "$dirName/bed/xenoRna";
    if (-e $workingDir) {
##    run("hgLoadPsl $dbName $workingDir/xenoMrna.psl -nobin");
    }

    $workingDir = "$dirName/bed/xenoEst";
    if (-e $workingDir) {
##    run("hgLoadPsl $dbName $workingDir/xenoEst.psl -nobin");
    }

    $workingDir = "$dirName/bed/refSeq";
    if (-e "$workingDir/chrom") {
##        run("pslCat -dir $workingDir/chrom > $workingDir/refSeqAli.psl");
##        run("hgLoadPsl $dbName -tNameIx $workingDir/refSeqAli.psl");
    }

    $workingDir = "$dirName/bed/refSeq";
##    print("$refSeqDir/$orgName\n\n");
##    if (-e "$refSeqDir/$orgName") {
##        run("hgRefSeqMrna $dbName $refSeqDir/$orgName/refSeq.fa $refSeqDir/$orgName/refSeq.ra $workingDir/all_refSeq.psl $refSeqDir/loc2ref $refSeqDir/$orgName/$orgName.prot.fa $refSeqDir/mim2loc");
##        run("hgRefSeqStatus $dbName $refSeqDir/loc2ref");
##    }

#    $workingDir = "$dirName/bed/xeno";
#    run("hgLoadPsl -tNameIx $dbName $workingDir/psl/xenoMrna.psl");    
#    run("hgLoadRna add $dbName /cluster/store1/mrna.129/mrna.fa /cluster/store1/mrna.129/mrna.ra");

# Make spliced ESTs track
##    run("scripts/makeIntronEst.csh $dirName");
##    run("hgLoadPsl $dbName $dirName/bed/est/intronEst/*.psl");

    # Sort the fish blat alignments
    $workingDir = "$zooDir/$dirName/bed/blatFish";
##    run("pslCat -dir $workingDir/psl | pslSortAcc nohead $workingDir/chrom temp stdin");
##    run("scripts/loadFish.csh $dirName");

    # Make fish blat track sequence data
##    run("hgLoadRna addSeq $dbName /cluster/store3/matt/tetFish/*.fa");
##    run("rm *.tab");

#    $workingDir = $dirName;
#    run("hgLoadPsl $dbName $workingDir/bed/crossAlign/psl/*.psl");

##    run("rm -f $dirName/bed/simpleRepeat/*.bed");
##    run("cat $dirName/bed/simpleRepeat/trf/*.bed >> $dirName/bed/simpleRepeat/simpleRepeat.bed");
##    run("hgLoadBed $dbName simpleRepeat $dirName/bed/simpleRepeat/simpleRepeat.bed -sqlTable=/cluster/home/kent/src/hg/lib/simpleRepeat.sql");

##    my $tablePrefix;
##    for $tablePrefix (values(%orgNameHash)) {
##        if ($tablePrefix eq "zfish") {
##            $tablePrefix = "zebrafish";
##        }

# Create the tables and load the NIB files
##        run("echo \"DROP TABLE ${tablePrefix}Chrom\" | hgsql $dbName");
##        run("echo \"CREATE TABLE ${tablePrefix}Chrom (chrom varchar(255) not null, size int unsigned not null, fileName varchar(255) not null, PRIMARY KEY(chrom(16)))\" | hgsql $dbName");
##        run("hgNibSeq -preMadeNib -table=${tablePrefix}Chrom $dbName $dirName/nib $dirName/?/chr*.fa $dirName/??/chr*.fa");
#    }

# RYAN GO HERE
} # END OF LOOP


################## END OF MAIN ROUTINE #######################################

################## SUBROUTINES ###############################################
sub run
{
    my ($cmd) = @_;
    print $cmd . "\n";
    system $cmd;
    if ($? != 0) {
        die("$cmd FAILED!!!\n");
        print("$cmd FAILED!!!\n");
    }
}
