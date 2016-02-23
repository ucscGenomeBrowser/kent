#!/usr/bin/env perl

# DO NOT EDIT the /cluster/bin/scripts copy of this file --
# edit ~/kent/src/hg/utils/automation/doNcbiRefSeq.pl instead.

# HOW TO USE THIS TEMPLATE:
# 1. Global-replace doNcbiRefSeq.pl with your actual script name.
# 2. Search for template and replace each instance with something appropriate.
#    Add steps and subroutines as needed.  Other do*.pl or make*.pl may have
#    useful example code -- this is just a skeleton.

use Getopt::Long;
use warnings;
use strict;
use FindBin qw($Bin);
use lib "$Bin";
use HgAutomate;
use HgRemoteScript;
use HgStepManager;

my $gff3ToRefLink = "$Bin/gff3ToRefLink.pl";
my $gbffToCds = "$Bin/gbffToCds.pl";
my $accRsu = "$Bin/accRsu.pl";

# Option variable names, both common and peculiar to this script:
use vars @HgAutomate::commonOptionVars;
use vars @HgStepManager::optionVars;
use vars qw/
    $opt_buildDir
    $opt_genbank
    $opt_subgroup
    $opt_species
    /;

# Specify the steps supported with -continue / -stop:
my $stepper = new HgStepManager(
    [ { name => 'download', func => \&doDownload },
      { name => 'process', func => \&doProcess },
      { name => 'load', func => \&doLoad },
      { name => 'cleanup', func => \&doCleanup },
    ]
				);

# Option defaults:
my $dbHost = 'hgwdev';

my $base = $0;
$base =~ s/^(.*\/)?//;

sub usage {
  # Usage / help / self-documentation:
  my ($status, $detailed) = @_;
  # Basic help (for incorrect usage):
  print STDERR "
usage: $base [options] genbank|refseq subGroup species asmId db
required arguments:
    genbank|refseq - specify either genbank or refseq hierarchy source
    subGroup       - specify subGroup at NCBI FTP site, examples:
                   - vertebrate_mammalian vertebrate_other plant etc...
    species        - species directory at NCBI FTP site, examples:
                   - Homo_sapiens Mus_musculus etc...
    asmId          - assembly identifier at NCBI FTP site, examples:
                   - GCF_000001405.32_GRCh38.p6 GCF_000001635.24_GRCm38.p4 etc..
    db             - database to load with track tables

options:
";
  print STDERR $stepper->getOptionHelp();
  print STDERR <<_EOF_
    -buildDir dir         Use dir instead of default
                          $HgAutomate::clusterData/\$db/$HgAutomate::trackBuild/ncbiRefSeq.\$date
                          (necessary when continuing at a later date).
_EOF_
  ;
  print STDERR &HgAutomate::getCommonOptionHelp('dbHost' => $dbHost,
						'workhorse' => '',
						'fileServer' => '',
						'bigClusterHub' => '',
						'smallClusterHub' => '');
  print STDERR "
Automates UCSC's ncbiRefSeq track build.  Steps:
    download: rsync required files from NCBI FTP site
    cleanup: Removes or compresses intermediate files.
All operations are performed in the build directory which is
$HgAutomate::clusterData/\$db/$HgAutomate::trackBuild/ncbiRefSeq.\$date unless -buildDir is given.
";
  # Detailed help (-help):
  print STDERR "
Assumptions:
1. $HgAutomate::clusterData/\$db/\$db.2bit contains RepeatMasked sequence for
   database/assembly \$db.
2. $HgAutomate::clusterData/\$db/chrom.sizes contains all sequence names and sizes from
   \$db.2bit.
3. The \$db.2bit files have already been distributed to cluster-scratch
   (/scratch/hg or /iscratch, /san etc).
" if ($detailed);
  print "\n";
  exit $status;
}


# Globals:
# Command line args: genbankRefseq subGroup species asmId db
my ($genbankRefseq, $subGroup, $species, $asmId, $db);
# Other:
my ($buildDir);
my ($secondsStart, $secondsEnd);

sub checkOptions {
  # Make sure command line options are valid/supported.
  my $ok = GetOptions(@HgStepManager::optionSpec,
		      'buildDir=s',
		      @HgAutomate::commonOptionSpec,
		      );
  &usage(1) if (!$ok);
  &usage(0, 1) if ($opt_help);
  &HgAutomate::processCommonOptions();
  my $err = $stepper->processOptions();
  usage(1) if ($err);
  $dbHost = $opt_dbHost if ($opt_dbHost);
}


#########################################################################
# * step: download [workhorse]
sub doDownload {
  my $runDir = "$buildDir/download";
  &HgAutomate::mustMkdir($runDir);

  my $whatItDoes = "download required set of files from NCBI.";
  my $workhorse = &HgAutomate::chooseWorkhorse();
  my $bossScript = newBash HgRemoteScript("$runDir/doDownload.bash", $workhorse,
				      $runDir, $whatItDoes);
  my $ftpDir = "genomes/$genbankRefseq/$subGroup/$species/all_assembly_versions/$asmId";
  my $outsideCopy = "/hive/data/outside/ncbi/$ftpDir";
  my $localData = "/hive/data/inside/ncbi/$ftpDir";
  $localData =~ s/all_assembly_versions/latest_assembly_versions/;
  my $local2Bit = "$localData/$asmId.ncbi.2bit";

  # see if local symLinks can be made with copies already here from NCBI:
  if ( -d "$outsideCopy" && -s $local2Bit ) {
    $bossScript->add(<<_EOF_
ln -s $outsideCopy/${asmId}_genomic.gff.gz .
ln -s $outsideCopy/${asmId}_rna.fna.gz .
ln -s $outsideCopy/${asmId}_rna.gbff.gz .
ln -s $outsideCopy/${asmId}_protein.faa.gz .
ln -s $local2Bit .
_EOF_
    );
  } else {
    $bossScript->add(<<_EOF_
for F in _rna.gbff.gz _rna.fna.gz _rna.gbff.gz _protein.faa.gz _genomic.gff.gz _genomic.fna.gz
do
   rsync -a -P \\
rsync://ftp.ncbi.nlm.nih.gov/$ftpDir/$asmId\${F} ./
done
faToTwoBit ${asmId}_genomic.fna.gz ${asmId}.ncbi.2bit
_EOF_
    );
  }
  $bossScript->add(<<_EOF_
twoBitDup -keyList=stdout $asmId.ncbi.2bit \\
  | sort > ncbi.$asmId.sequence.keys
twoBitDup -keyList=stdout /hive/data/genomes/$db/$db.2bit \\
  | sort > ucsc.$db.sequence.keys
twoBitInfo $asmId.ncbi.2bit stdout | sort -k2nr > $asmId.chrom.sizes
zcat ${asmId}_rna.fna.gz | sed -e 's/ .*//;' | gzip -c > $asmId.rna.fa.gz
faToTwoBit $asmId.rna.fa.gz t.2bit
twoBitInfo t.2bit stdout | sort -k2nr > rna.chrom.sizes
rm -f t.2bit
join -t'\t' ucsc.$db.sequence.keys ncbi.$asmId.sequence.keys \\
   | cut -f2-3 | sort \\
     | join -t'\t' - <(sort /hive/data/genomes/$db/chrom.sizes) \\
       | awk -F'\t' '{printf "0\\t%s\\t%d\\t%s\\t%d\\n", \$2, \$3, \$1, \$3}' \\
          | sort -k3nr > ${asmId}To${db}.lift
zegrep "VERSION|COMMENT" ${asmId}_rna.gbff.gz | awk '{print \$2}' \\
    | xargs -L 2 echo | tr '[ ]' '[\\t]' | sort > rna.status.tab
/hive/data/outside/genbank/bin/x86_64/gbProcess \\
    -inclXMs /dev/null $asmId.raFile.txt ${asmId}_rna.gbff.gz
$accRsu $asmId.raFile.txt 2> /dev/null | sort > $asmId.accession.description.txt
_EOF_
  );
  $bossScript->execute();
} # doDownload

#########################################################################
# * step: process [workhorse]
sub doProcess {
  my $runDir = "$buildDir/process";
  my $downloadDir = "$buildDir/download";
  &HgAutomate::mustMkdir($runDir);

  my $JOINDESCR = HgAutomate::mustOpen(">$runDir/joinDescr.pl");
  print $JOINDESCR <<_EOF_
#!/usr/bin/env perl

use strict;
use warnings;

open (FH, "<$asmId.$db.ncbiRefSeqLink.tab") or die "can not read $asmId.$db.ncbiRefSeqLink.tab";
while (my \$line = <FH>) {
  chomp \$line;
  my (\$acc, \$rest) = split('\\t', \$line, 2);
  my \$noVer = \$acc;
  \$noVer =~ s/\\.[0-9]+\$//;
  printf "\%s\\t\%s\\t\%s\\n", \$noVer, \$acc, \$rest;
}
close (FH);
_EOF_
  ;
  close($JOINDESCR);

  my $whatItDoes = "process NCBI download files into track files.";
  my $workhorse = &HgAutomate::chooseWorkhorse();
  my $bossScript = newBash HgRemoteScript("$runDir/doProcess.bash", $workhorse,
				      $runDir, $whatItDoes);

  $bossScript->add(<<_EOF_
chmod +x joinDescr.pl
gff3ToGenePred -useName -attrsOut=$asmId.attrs.txt -allowMinimalGenes \\
    -unprocessedRootsOut=$asmId.unprocessedRoots.txt \\
      $downloadDir/${asmId}_genomic.gff.gz $asmId.gp
genePredCheck $asmId.gp
$gff3ToRefLink $downloadDir/rna.status.tab $downloadDir/$asmId.raFile.txt $downloadDir/${asmId}_genomic.gff.gz 2> $db.refLink.stderr.txt \\
  | sort > $asmId.refLink.tab
liftUp -extGenePred -type=.gp stdout $downloadDir/${asmId}To${db}.lift drop $asmId.gp \\
  | gzip -c > $asmId.$db.gp.gz
genePredCheck -db=$db $asmId.$db.gp.gz
zcat $asmId.$db.gp.gz | cut -f1 | sort -u > $asmId.$db.name.list
join -t'\t' $asmId.$db.name.list $asmId.refLink.tab > $asmId.$db.ncbiRefSeqLink.tab
zegrep "^N(M|R)_|^YP_" $asmId.$db.gp.gz > $db.curated.gp
zegrep "^X(M|R)_" $asmId.$db.gp.gz > $db.predicted.gp
zegrep -v "^N(M|R)_|^YP_|^X(M|R)_" $asmId.$db.gp.gz > $db.other.gp
genePredCheck -db=$db $db.curated.gp
genePredCheck -db=$db $db.predicted.gp
genePredCheck -db=$db $db.other.gp
(zgrep "^#" $downloadDir/${asmId}_genomic.gff.gz | head || true) > gffForPsl.gff
zegrep -v "NG_" $downloadDir/${asmId}_genomic.gff.gz \\
  | awk -F'\\t' '\$3 == "cDNA_match" || \$3 == "match"' >> gffForPsl.gff
gff3ToPsl $downloadDir/$asmId.chrom.sizes $downloadDir/rna.chrom.sizes \\
  gffForPsl.gff $asmId.psl 
simpleChain -outPsl $asmId.psl stdout | pslSwap stdin stdout \\
  | liftUp -type=.psl stdout $downloadDir/${asmId}To${db}.lift drop stdin \\
   | gzip -c > $db.psl.gz
pslCheck -db=$db $db.psl.gz
genePredToFakePsl $db $asmId.$db.gp.gz $db.fake.psl $db.fake.cds
zcat $db.psl.gz | headRest 5 stdin | cut -f10 > $db.psl.names
pslSomeRecords -not $db.fake.psl $db.psl.names $db.someRecords.psl
pslSort dirs stdout \\
   ./tmpdir $db.psl.gz $db.someRecords.psl | gzip -c > $asmId.$db.psl.gz
rm -fr ./tmpdir
pslCheck -db=$db $asmId.$db.psl.gz
$gbffToCds $downloadDir/${asmId}_rna.gbff.gz > $asmId.rna.cds
rm -f tmp.bigPsl
pslToBigPsl -fa=$downloadDir/$asmId.rna.fa.gz -cds=$asmId.rna.cds $db.psl.gz tmp.bigPsl
sort -k1,1 -k2,2n tmp.bigPsl > $asmId.$db.bigPsl
# bedToBigBed -type=bed12+12 -tab -as=$ENV{'HOME'}/kent/src/hg/lib/bigPsl.as \\
#   $asmId.$db.bigPsl /hive/data/genomes/$db/chrom.sizes $asmId.$db.bigPsl.bb
./joinDescr.pl | sort -k1 > $db.to.join.Link.tab
join -t'\t' ../download/$asmId.accession.description.txt $db.to.join.Link.tab \\
  | awk -F'\\t' '{printf "%s\\t%s\\t%s\\t%s\\t%s\\t%s\\t%s\\t%s\\t%s\\t%s\\t%s\\t%s\\t%s\\t%s\\t%s\\t%s\\t%s\\t%s\\n", \$3,\$4,\$5,\$6,\$7,\$8,\$9,\$10,\$11,\$12,\$13,\$14,\$15,\$16,\$17,\$18,\$19,\$2 }' > $db.joined.Link.tab
join -a 2 -t'\t' ../download/$asmId.accession.description.txt $db.to.join.Link.tab \\
  | awk -F'\\t' 'NF < 20' | cut -f2- > $db.not.joined.Link.tab
sort $db.joined.Link.tab $db.not.joined.Link.tab > $db.ncbiRefSeqLink.tab
_EOF_
  );
  $bossScript->execute();
} # doProcess

#########################################################################
# * step: load [workhorse]
sub doLoad {
  my $runDir = "$buildDir";
  &HgAutomate::mustMkdir($runDir);

  my $whatItDoes = "load NCBI processed files into track files.";
  my $workhorse = &HgAutomate::chooseWorkhorse();
  my $bossScript = newBash HgRemoteScript("$runDir/doLoad.bash", $workhorse,
				      $runDir, $whatItDoes);

  $bossScript->add(<<_EOF_
# loading the genePred tracks
hgLoadGenePred -genePredExt $db ncbiRefSeq process/$asmId.$db.gp.gz
genePredCheck -db=$db ncbiRefSeq

hgLoadGenePred -genePredExt $db ncbiRefSeqCurated process/$db.curated.gp
genePredCheck -db=$db ncbiRefSeqCurated

hgLoadGenePred -genePredExt $db ncbiRefSeqPredicted process/$db.predicted.gp
genePredCheck -db=$db ncbiRefSeqPredicted

hgLoadGenePred -genePredExt $db ncbiRefSeqOther process/$db.other.gp
genePredCheck -db=$db ncbiRefSeqOther

hgLoadSqlTab $db ncbiRefSeqCds ~/kent/src/hg/lib/cdsSpec.sql \\
   process/$asmId.rna.cds 

# loading the cross reference data
hgLoadSqlTab $db ncbiRefSeqLink ~/kent/src/hg/lib/ncbiRefSeqLink.sql \\
   process/$asmId.$db.ncbiRefSeqLink.tab

# prepare Pep table, select only those peptides that match loaded items
hgsql -N -e 'select protAcc from ncbiRefSeqLink;' $db | grep -v "n/a" \\
   | sort -u > $db.ncbiRefSeqLink.protAcc.list

zcat download/${asmId}_protein.faa.gz \\
   | sed -e 's/ .*//;' | faToTab -type=protein -keepAccSuffix stdin stdout \\
     | sort | join -t'\t' $db.ncbiRefSeqLink.protAcc.list - \\
        > $db.ncbiRefSeqPepTable.tab

hgLoadSqlTab $db ncbiRefSeqPepTable ~/kent/src/hg/lib/pepPred.sql \\
   $db.ncbiRefSeqPepTable.tab

# and load the fasta peptides, again, only those for items that exist
zcat process/$asmId.$db.psl.gz | cut -f10 | headRest 5 stdin \\
   | sort -u > $db.psl.used.rna.list
cut -f5 process/$asmId.$db.ncbiRefSeqLink.tab | grep -v "n/a" | sort -u > $db.mrnaAcc.name.list
sort -u $db.psl.used.rna.list $db.mrnaAcc.name.list > $db.rna.select.list
zcat process/$asmId.$db.gp.gz | cut -f1 | sort -u > $db.gp.name.list
comm -12 $db.rna.select.list $db.gp.name.list > $db.toLoad.rna.list
comm -23 $db.rna.select.list $db.gp.name.list > $db.not.used.rna.list
comm -13 $db.rna.select.list $db.gp.name.list > $db.noRna.available.list
faSomeRecords download/$asmId.rna.fa.gz $db.toLoad.rna.list $db.rna.fa

mkdir -p /gbdb/$db/ncbiRefSeq
rm -f /gbdb/$db/ncbiRefSeq/seqNcbiRefSeq.rna.fa
ln -s `pwd`/$db.rna.fa /gbdb/$db/ncbiRefSeq/seqNcbiRefSeq.rna.fa
hgLoadSeq -drop -seqTbl=seqNcbiRefSeq -extFileTbl=extNcbiRefSeq $db \\
   /gbdb/$db/ncbiRefSeq/seqNcbiRefSeq.rna.fa

# bigPsl not needed when PSL table is loaded
# mkdir -p /gbdb/$db/bbi/ncbiRefSeq
# rm -f /gbdb/$db/bbi/ncbiRefSeqBigPsl.bb
# ln -s `pwd`/process/$asmId.$db.bigPsl.bb /gbdb/$db/bbi/ncbiRefSeqBigPsl.bb
# hgBbiDbLink $db ncbiRefSeqBigPsl /gbdb/$db/bbi/ncbiRefSeqBigPsl.bb

hgLoadPsl $db -table=ncbiRefSeqPsl process/$asmId.$db.psl.gz

_EOF_
  );
  $bossScript->execute();
} # doProcess

#########################################################################
# * step: cleanup [fileServer]
sub doCleanup {
  my $runDir = "$buildDir";
  my $whatItDoes = "It cleans up or compresses intermediate files.";
  my $fileServer = &HgAutomate::chooseFileServer($runDir);
  my $bossScript = new HgRemoteScript("$runDir/doCleanup.csh", $fileServer,
				      $runDir, $whatItDoes);
  $bossScript->add(<<_EOF_
rm -rf run.template/raw/
rm -rf templateOtherBigTempFilesOrDirectories
gzip template
_EOF_
  );
  $bossScript->execute();
} # doCleanup


#########################################################################
# main

# Prevent "Suspended (tty input)" hanging:
&HgAutomate::closeStdin();

# Make sure we have valid options and exactly 1 argument:
&checkOptions();
&usage(1) if (scalar(@ARGV) != 5);

$secondsStart = `date "+%s"`;
chomp $secondsStart;

# expected command line arguments after options are processed
($genbankRefseq, $subGroup, $species, $asmId, $db) = @ARGV;

# Force debug and verbose until this is looking pretty solid:
# $opt_debug = 1;
# $opt_verbose = 3 if ($opt_verbose < 3);

# Establish what directory we will work in.
my $date = `date +%Y-%m-%d`;
chomp $date;
$buildDir = $opt_buildDir ? $opt_buildDir :
  "$HgAutomate::clusterData/$db/$HgAutomate::trackBuild/ncbiRefSeq.$date";

# Do everything.
$stepper->execute();

$secondsEnd = `date "+%s"`;
chomp $secondsEnd;
my $elapsedSeconds = $secondsEnd - $secondsStart;
my $elapsedMinutes = int($elapsedSeconds/60);
$elapsedSeconds -= $elapsedMinutes * 60;

# Tell the user anything they should know.
my $stopStep = $stepper->getStopStep();
my $upThrough = ($stopStep eq 'cleanup') ? "" :
  "  (through the '$stopStep' step)";

&HgAutomate::verbose(1,
	"\n *** All done !$upThrough  Elapsed time: ${elapsedMinutes}m${elapsedSeconds}s\n");
&HgAutomate::verbose(1,
	" *** Steps were performed in $buildDir\n");
&HgAutomate::verbose(1, "\n");

