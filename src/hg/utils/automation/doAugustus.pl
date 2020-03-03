#!/usr/bin/env perl

# DO NOT EDIT the /cluster/bin/scripts copy of this file --
# edit ~/kent/src/hg/utils/automation/doAugustus.pl instead.

use Getopt::Long;
use warnings;
use strict;
use FindBin qw($Bin);
use lib "$Bin";
use HgAutomate;
use HgRemoteScript;
use HgStepManager;

# Option variable names, both common and peculiar to this script:
use vars @HgAutomate::commonOptionVars;
use vars @HgStepManager::optionVars;
use vars qw/
    $opt_buildDir
    $opt_maskedSeq
    $opt_species
    $opt_utr
    $opt_noDbGenePredCheck
    /;

# Specify the steps supported with -continue / -stop:
my $stepper = new HgStepManager(
    [ { name => 'partition',   func => \&doPartition },
      { name => 'augustus', func => \&doAugustus },
      { name => 'makeGp', func => \&doMakeGp },
      { name => 'load', func => \&doLoadAugustus },
      { name => 'cleanup', func => \&doCleanup },
    ]
				);

# Option defaults:
# my $bigClusterHub = 'swarm';
my $bigClusterHub = 'ku';
my $workhorse = 'hgwdev';
my $dbHost = 'hgwdev';
my $defaultWorkhorse = 'hgwdev';
my $maskedSeq = "$HgAutomate::clusterData/\$db/\$db.2bit";
my $utr = "off";
my $noDbGenePredCheck = 1;    # default yes, use -db for genePredCheck
my $species = "human";
my $augustusDir = "/hive/data/outside/augustus/augustus-3.3.1";
my $augustusConfig="$augustusDir/config";

my $base = $0;
$base =~ s/^(.*\/)?//;

sub usage {
  # Usage / help / self-documentation:
  my ($status, $detailed) = @_;
  # Basic help (for incorrect usage):
  print STDERR "
usage: $base db
options:
";
  print STDERR $stepper->getOptionHelp();
  print STDERR <<_EOF_
    -buildDir dir         Use dir instead of default
                          $HgAutomate::clusterData/\$db/$HgAutomate::trackBuild/augustus
                          (necessary when continuing at a later date).
    -maskedSeq seq.2bit   Use seq.2bit as the masked input sequence instead
                          of default ($maskedSeq).
    -utr                  Obsolete, now is automatic (was: Use augustus arg: --UTR=on, default is --UTR=off)
    -noDbGenePredCheck    do not use -db= on genePredCheck, there is no real db
    -species <name>       name from list: human chicken zebrafish, default: human
_EOF_
  ;
  print STDERR &HgAutomate::getCommonOptionHelp('dbHost' => $dbHost,
                                'bigClusterHub' => $bigClusterHub,
                                'workhorse' => $defaultWorkhorse);
  print STDERR "
Automates UCSC's Augustus track construction for the database \$db.  Steps:
    partition:  Creates hard-masked fastas needed for the CpG Island program.
    augustus:   Run gsBig on the hard-masked fastas
    makeGp:   Transform output from gsBig into augustus.gtf augustus.pep and
    load:      Load augustus.gtf and into \$db.
    cleanup:   Removes hard-masked fastas and output from gsBig.
All operations are performed in the build directory which is
$HgAutomate::clusterData/\$db/$HgAutomate::trackBuild/augustus unless -buildDir is given.
";
  # Detailed help (-help):
  print STDERR "
Assumptions:
1. $HgAutomate::clusterData/\$db/\$db.2bit contains RepeatMasked sequence for
   database/assembly \$db.
" if ($detailed);
  print "\n";
  exit $status;
}


# Globals:
# Command line args: db
my ($db);
# Other:
my ($buildDir, $secondsStart, $secondsEnd);

sub checkOptions {
  # Make sure command line options are valid/supported.
  my $ok = GetOptions(@HgStepManager::optionSpec,
		      'buildDir=s',
		      'maskedSeq=s',
		      'species=s',
		      'utr',
		      'noDbGenePredCheck',
		      @HgAutomate::commonOptionSpec,
		      );
  &usage(1) if (!$ok);
  &usage(0, 1) if ($opt_help);
  &HgAutomate::processCommonOptions();
  my $err = $stepper->processOptions();
  usage(1) if ($err);
  $workhorse = $opt_workhorse if ($opt_workhorse);
  $bigClusterHub = $opt_bigClusterHub if ($opt_bigClusterHub);
  $dbHost = $opt_dbHost if ($opt_dbHost);
}

#########################################################################
# * step: partition [workhorse]
sub doPartition {
  # Set up and perform the cluster run to run create the partitioned sequences.
  my $runDir = "$buildDir/partition";

  # First, make sure we're starting clean.
  if (-d "$runDir") {
    die "doPartition: partition step already done, remove directory 'partition' to rerun,\n" .
      "or '-continue augustus' to run next step.\n";
  }

  &HgAutomate::mustMkdir($runDir);
  my $whatItDoes="partition 2bit file into fasta chunks for augustus processing.";
  my $bossScript = newBash HgRemoteScript("$runDir/doPartition.bash", $workhorse,
				      $runDir, $whatItDoes);
  $bossScript->add(<<_EOF_
export db="$db"
mkdir -p partBundles
mkdir -p ../fasta

twoBitInfo $maskedSeq stdout | sort -k2,2nr > \$db.chrom.sizes
# aim for 1000 bundles or less
export partSize=`cat \$db.chrom.sizes | wc -l | awk '{printf "%d", 1+\$1/1000}'`
if [ "\$partSize" -gt 1000 ]; then
   partSize=1000
fi

/cluster/bin/scripts/partitionSequence.pl 11000000 1000000 \\
    $maskedSeq \\
    \$db.chrom.sizes \$partSize -rawDir=gtf \\
    -lstDir=partBundles > part.list

(grep -v partBundles part.list || /bin/true) | sed -e 's/.*2bit://;' \\
   | awk -F':' '{print \$1}' | sort -u | while read chr
do
   twoBitToFa $maskedSeq:\${chr} ../fasta/\${chr}.fa
done
if [ -s partBundles/part000.lst ]; then
  for F in partBundles/*.lst
  do
     B=`basename \$F | sed -e 's/.lst//;'`
     sed -e 's#.*.2bit:##;' \$F \\
      | twoBitToFa -seqList=stdin $maskedSeq stdout > ../fasta/\${B}.fa
  done
fi
_EOF_
  );

  $bossScript->execute();
} # doPartition

#########################################################################
# * step: augustus [bigClusterHub]
sub doAugustus {
  # Set up and perform the cluster run to run the augustus gene finder on the
  #     chunked fasta sequences.
  my $paraHub = $bigClusterHub;
  my $runDir = "$buildDir/run.augustus";
  # First, make sure we're starting clean.
  if (! -d "$buildDir/fasta" || ! -s "$buildDir/partition/part.list") {
    die "doAugustus: the previous step 'partition' did not complete \n" .
      "successfully (partition/part.list or fasta/*.fa does not exists).\nPlease " .
      "complete the previous step: -continue=-partition\n";
  } elsif (-e "$runDir/run.time") {
    die "doAugustus: looks like this step was run successfully already " .
      "(run.time exists).\nEither run with -continue makeGp or some later " .
	"stage,\nor move aside/remove $runDir/ and run again.\n";
  } elsif ((-e "$runDir/jobList") && ! $opt_debug) {
    die "doAugustus: looks like we are not starting with a clean " .
      "slate.\n\tclean\n  $runDir/\n\tand run again.\n";
  }
  &HgAutomate::mustMkdir($runDir);
  my $whatItDoes = "runs one parasol augustus prediction";
  my $bossScript = new HgRemoteScript("$runDir/runOne", $dbHost,
				      $runDir, $whatItDoes);

  $bossScript->add(<<_EOF_
# expected inputs:
#   <integer> <integer> </fullPath/fasta.fa> <gtf/directory/file.gtf>
#       1         2            3                   4
#     start      end        chr fasta file        output file result
#       start==end  for a multiple fasta file
#     start is 0 relative, will increment by 1 here for augustus
#     error result will end up in augErr/directory/file.err

set db = "$db"
set start = \$1
set end = \$2
set fasta = \$3
set chrName = \$fasta:r:t

set resultGz = \$4
set gtfFile = \$resultGz:t:r
set errFile = "augErr/\$resultGz:h:t/\$gtfFile:r:t.err"
mkdir -p \$resultGz:h
mkdir -p \$errFile:h
set tmpDir = "/dev/shm/\$db.\$chrName.\$start.\$end"

mkdir -p \$tmpDir
pushd \$tmpDir

if ( \$start == \$end ) then
$augustusDir/bin/augustus --species=$species --softmasking=1 --UTR=$utr --protein=off \\
 --AUGUSTUS_CONFIG_PATH=$augustusConfig \\
  --alternatives-from-sampling=true --sample=100 --minexonintronprob=0.2 \\
   --minmeanexonintronprob=0.5 --maxtracks=3 --temperature=2 \\
    \$fasta --outfile=\$gtfFile --errfile=\$errFile:t
else
 @ start++
$augustusDir/bin/augustus --species=$species --softmasking=1 --UTR=$utr --protein=off \\
 --AUGUSTUS_CONFIG_PATH=$augustusConfig \\
  --alternatives-from-sampling=true --sample=100 --minexonintronprob=0.2 \\
   --minmeanexonintronprob=0.5 --maxtracks=3 --temperature=2 \\
     --predictionStart=\$start --predictionEnd=\$end \\
      \$fasta --outfile=\$gtfFile --errfile=\$errFile:t
endif

gzip \$gtfFile
popd
mv \$tmpDir/\$gtfFile.gz \$resultGz
mv \$tmpDir/\$errFile:t \$errFile
rm -fr \$tmpDir
_EOF_
  );

  $whatItDoes = "Run augustus on chunked fasta sequences.";
  $bossScript = newBash HgRemoteScript("$runDir/runAugustus.bash", $paraHub,
				      $runDir, $whatItDoes);
  my $paraRun = &HgAutomate::paraRun();
  $bossScript->add(<<_EOF_
(grep -v partBundles ../partition/part.list || /bin/true) | while read twoBit
do
  chr=`echo \$twoBit |  sed -e 's/.*2bit://;' | awk -F':' '{print \$1}'`
  chrStart=`echo \$twoBit |  sed -e 's/.*2bit://;' | awk -F':' '{print \$2}' | sed -e 's/-.*//;'`
  chrEnd=`echo \$twoBit |  sed -e 's/.*2bit://;' | awk -F':' '{print \$2}' | sed -e 's/.*-//;'`
  echo "runOne \$chrStart \$chrEnd {check in exists+ $buildDir/fasta/\${chr}.fa} {check out exists+ gtf/\$chr/\$chr.\$chrStart.\$chrEnd.gtf.gz}"
done > jobList

(grep partBundles ../partition/part.list || /bin/true) | while read bundleName
do
  B=`basename \$bundleName | sed -e 's/.lst//;'`
  echo "runOne 0 0 {check in exists+ $buildDir/fasta/\${B}.fa} {check out exists+ gtf/\${B}/\${B}.0.0.gtf.gz}"
done >> jobList

chmod +x runOne

$paraRun
_EOF_
  );
  $bossScript->execute();
} # doAugustus

#########################################################################
# * step: make gp [workhorse]
sub doMakeGp {
  my $runDir = $buildDir;
  &HgAutomate::mustMkdir($runDir);

  # First, make sure we're starting clean.
  if (! -e "$runDir/run.augustus/run.time") {
    die "doMakeGp: the previous step augustus did not complete \n" .
      "successfully ($buildDir/run.augustus/run.time does not exist).\nPlease " .
      "complete the previous step: -continue=augustus\n";
  } elsif (-e "$runDir/$db.augustus.bb" ) {
    die "doMakeGp: looks like this was run successfully already\n" .
      "($db.augustus.bb exists).  Either run with -continue load or cleanup\n" .
	"or move aside/remove $runDir/$db.augustus.bb\nand run again.\n";
  }

  my $whatItDoes = "Makes genePred file from augustus gff output.";
  my $bossScript = newBash HgRemoteScript("$runDir/makeGp.bash", $workhorse,
				      $runDir, $whatItDoes);

  my $dbCheck = "-db=$db";
  $dbCheck = "" if (0 == $noDbGenePredCheck);

  $bossScript->add(<<_EOF_
export db=$db
find ./run.augustus/gtf -type f | grep ".gtf.gz\$" \\
  | sed -e 's#/# _D_ #g; s#\\.# _dot_ #g;' \\
    | sort -k11,11 -k13,13n \\
     | sed -e 's# _dot_ #.#g; s# _D_ #/#g' | xargs zcat \\
       | $augustusDir/scripts/join_aug_pred.pl \\
          | grep -P "\\t(CDS|exon|stop_codon|start_codon|tts|tss)\\t" \\
            > \$db.augustus.gtf
gtfToGenePred -genePredExt -infoOut=\$db.info \$db.augustus.gtf \$db.augustus.gp
genePredCheck $dbCheck \$db.augustus.gp
genePredToBed \$db.augustus.gp stdout \\
    | bedToExons stdin stdout | bedSingleCover.pl stdin > \$db.exons.bed
export baseCount=`awk '{sum+=\$3-\$2}END{printf "%d", sum}' \$db.exons.bed`
export asmSizeNoGaps=`grep sequences ../../\$db.faSize.txt | awk '{print \$5}'`
export perCent=`echo \$baseCount \$asmSizeNoGaps | awk '{printf "%.3f", 100.0*\$1/\$2}'`
printf "%d bases of %d (%s%%) in intersection\\n" "\$baseCount" "\$asmSizeNoGaps" "\$perCent" > fb.\$db.augustus.txt
rm -f \$db.exons.bed
genePredToBigGenePred \$db.augustus.gp stdout | sort -k1,1 -k2,2n > \$db.augustus.bgp
bedToBigBed -type=bed12+8 -tab -as=$ENV{'HOME'}/kent/src/hg/lib/bigGenePred.as \$db.augustus.bgp partition/\$db.chrom.sizes \$db.augustus.bb
getRnaPred -genePredExt -keepMasking -genomeSeqs=$maskedSeq \$db \$db.augustus.gp all \$db.augustusGene.rna.fa
getRnaPred -genePredExt -peptides -genomeSeqs=$maskedSeq \$db \$db.augustus.gp all \$db.augustusGene.faa
_EOF_
  );
  $bossScript->execute();
} # doMakeGp

#########################################################################
# * step: load [dbHost]
sub doLoadAugustus {
  my $runDir = $buildDir;
  &HgAutomate::mustMkdir($runDir);
  my $tableName = "augustusGene";

  if (! -e "$runDir/$db.augustus.gp") {
    die "doLoadAugustus: the previous step makeGp did not complete \n" .
      "successfully ($db.augustus.gp does not exists).\nPlease " .
      "complete the previous step: -continue=-makeGp\n";
  } elsif (-e "$runDir/fb.$db.$tableName.txt" ) {
    die "doLoadAugustus: looks like this was run successfully already\n" .
      "(fb.$db.$tableName.txt exists).  Either run with -continue cleanup\n" .
	"or move aside/remove\n$runDir/fb.$db.$tableName.txt and run again.\n";
  }
  my $whatItDoes = "Loads $db.augustus.gp.";
  my $bossScript = newBash HgRemoteScript("$runDir/loadAugustus.bash", $dbHost,
				      $runDir, $whatItDoes);

  my $dbCheck = "-db=$db";
  $dbCheck = "" if (0 == $noDbGenePredCheck);

  $bossScript->add(<<_EOF_
export db="$db"
export table="$tableName"
genePredCheck $dbCheck \$db.augustus.gp
hgLoadGenePred \$db -genePredExt \$table \$db.augustus.gp
genePredCheck $dbCheck \$table
featureBits \$db \$table > fb.\$db.\$table.txt 2>&1
checkTableCoords -verboseBlocks -table=\$table \$db
cat fb.\$db.\$table.txt
_EOF_
  );
  $bossScript->execute();
} # doLoad

#########################################################################
# * step: cleanup [workhorse]
sub doCleanup {
  my $runDir = $buildDir;

  if (-e "$runDir/augustus.gtf.gz" ) {
    die "doCleanup: looks like this was run successfully already\n" .
      "(augustus.gtf.gz exists).  Investigate the run directory:\n" .
	" $runDir/\n";
  }
  my $whatItDoes = "It cleans up or compresses intermediate files.";
  my $bossScript = newBash HgRemoteScript("$runDir/cleanup.bash", $workhorse,
				      $runDir, $whatItDoes);
  $bossScript->add(<<_EOF_
export db="$db"
rm -fr $buildDir/fasta
rm -fr $buildDir/run.augustus/err/
rm -f $buildDir/run.augustus/batch.bak
rm -fr $buildDir/run.augustus/augErr
rm -f $buildDir/\$db.augustus.bgp
gzip $buildDir/\$db.augustus.gtf
gzip $buildDir/\$db.augustus.gp
gzip $buildDir/\$db.augustusGene.rna.fa
gzip $buildDir/\$db.augustusGene.faa
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
&usage(1) if (scalar(@ARGV) != 1);
$secondsStart = `date "+%s"`;
chomp $secondsStart;
($db) = @ARGV;

# Force debug and verbose until this is looking pretty solid:
#$opt_debug = 1;
#$opt_verbose = 3 if ($opt_verbose < 3);

$noDbGenePredCheck = $opt_noDbGenePredCheck ? 0 : $noDbGenePredCheck;

# Establish what directory we will work in.
$buildDir = $opt_buildDir ? $opt_buildDir :
  "$HgAutomate::clusterData/$db/$HgAutomate::trackBuild/augustus";
$maskedSeq = $opt_maskedSeq ? $opt_maskedSeq :
  "$HgAutomate::clusterData/$db/$db.2bit";
$species = $opt_species ? $opt_species : $species;
if ( -s "${augustusConfig}/species/$species/${species}_utr_probs.pbl" ) {
  $utr = "on";
} else {
  $utr = "off";
}

# Do everything.
$stepper->execute();

# Tell the user anything they should know.
my $stopStep = $stepper->getStopStep();
my $upThrough = ($stopStep eq 'cleanup') ? "" :
  "  (through the '$stopStep' step)";

$secondsEnd = `date "+%s"`;
chomp $secondsEnd;
my $elapsedSeconds = $secondsEnd - $secondsStart;
my $elapsedMinutes = int($elapsedSeconds/60);
$elapsedSeconds -= $elapsedMinutes * 60;

&HgAutomate::verbose(1,
	"\n *** All done !  Elapsed time: ${elapsedMinutes}m${elapsedSeconds}s\n");
&HgAutomate::verbose(1,
	"\n *** All done!$upThrough\n");
&HgAutomate::verbose(1,
	" *** Steps were performed in $buildDir\n");
&HgAutomate::verbose(1, "\n");

