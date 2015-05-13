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
    /;

# Specify the steps supported with -continue / -stop:
my $stepper = new HgStepManager(
    [ { name => 'partition',   func => \&doPartition },
      { name => 'augustus', func => \&doAugustus },
      { name => 'makeBed', func => \&doMakeBed },
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
my $species = "human";

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
    -utr                  Use augustus arg: --UTR=on, default is --UTR=off
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
    makeBed:   Transform output from gsBig into augustus.gtf augustus.pep and
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
				      $runDir, $whatItDoes, undef);
  $bossScript->add(<<_EOF_
export db="$db"
mkdir -p partBundles
mkdir -p ../fasta

twoBitInfo $maskedSeq stdout | sort -k2,2nr > \$db.chrom.sizes
/cluster/bin/scripts/partitionSequence.pl 11000000 1000000 \\
    $maskedSeq \\
    \$db.chrom.sizes 1000 -rawDir=gtf \\
    -lstDir=partBundles > part.list

(grep -v partBundles part.list || /bin/true) | sed -e 's/.*2bit://;' \\
   | awk -F':' '{print \$1}' | sort -u | while read chr
do
   twoBitToFa $maskedSeq:\${chr} ../fasta/\${chr}.fa
done
if [ -d partBundles ]; then
  for F in partBundles/*.lst
  do
     B=`basename \$F | sed -e 's/.lst//;'`
     cat \$F | sed -e 's/.*2bit://;' | awk -F':' '{print \$1}' \\
        | sort -u | while read chr
     do
       twoBitToFa $maskedSeq:\${chr} stdout
     done > ../fasta/\${B}.fa
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
      "(run.time exists).\nEither run with -continue makeBed or some later " .
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

set start = \$1
set end = \$2
set fasta = \$3
set chrName = \$fasta:r:t
echo "chrName: \$chrName"

set gtfOutput = \$4
set errFile = "augErr/\$gtfOutput:h:t/\$gtfOutput:r:t.err"
echo mkdir -p \$gtfOutput:h
mkdir -p \$gtfOutput:h
echo mkdir -p \$errFile:h
mkdir -p \$errFile:h
set tmpDir = "/dev/shm/\$chrName.\$start.\$end"

echo mkdir -p \$tmpDir
mkdir -p \$tmpDir
echo pushd \$tmpDir
pushd \$tmpDir

if ( \$start == \$end ) then
augustus --species=chicken --softmasking=1 --UTR=off --protein=off \\
   --AUGUSTUS_CONFIG_PATH=/hive/data/outside/augustus/augustus.3.1/config \\
     \$fasta --outfile=\$gtfOutput:t --errfile=\$errFile:t
else
 @ start++
augustus --species=chicken --softmasking=1 --UTR=off --protein=off \\
   --AUGUSTUS_CONFIG_PATH=/hive/data/outside/augustus/augustus.3.1/config \\
     --predictionStart=\$start --predictionEnd=\$end \$fasta \\
       --outfile=\$gtfOutput:t --errfile=\$errFile:t
endif

popd
mv \$tmpDir/\$gtfOutput:t \$gtfOutput
mv \$tmpDir/\$errFile:t \$errFile
rm -fr \$tmpDir
_EOF_
  );

  my $whatItDoes = "Run augustus on chunked fasta sequences.";
  my $bossScript = newBash HgRemoteScript("$runDir/doAugustus.bash", $paraHub,
				      $runDir, $whatItDoes);
  $bossScript->add(<<_EOF_
(grep -v partBundles ../partition/part.list || /bin/true) | while read twoBit
do
  chr=`echo \$twoBit |  sed -e 's/.*2bit://;' | awk -F':' '{print \$1}'`
  chrStart=`echo \$twoBit |  sed -e 's/.*2bit://;' | awk -F':' '{print \$2}' | sed -e 's/-.*//;'`
  chrEnd=`echo \$twoBit |  sed -e 's/.*2bit://;' | awk -F':' '{print \$2}' | sed -e 's/.*-//;'`
  echo "runOne \$chrStart \$chrEnd {check in exists+ $buildDir/fasta/\${chr}.fa} {check out line+ gtf/\$chr/\$chr.\$chrStart.\$chrEnd.gtf}"
done > jobList

(grep partBundles ../partition/part.list || /bin/true) | while read bundleName
do
  B=`basename \$bundleName | sed -e 's/.lst//;'`
  echo "runOne 0 0 {check in exists+ $buildDir/fasta/\${B}.fa} {check out line+ gtf/\${B}/\${B}.0.0.gtf}"
done >> jobList

chmod +x runOne

$HgAutomate::paraRun
_EOF_
  );
  $bossScript->execute();
} # doAugustus

#########################################################################
# * step: make bed [workhorse]
sub doMakeBed {
  my $runDir = $buildDir;
  &HgAutomate::mustMkdir($runDir);

  # First, make sure we're starting clean.
  if (! -e "$runDir/run.time") {
    die "doMakeBed: the previous step augustus did not complete \n" .
      "successfully ($buildDir/run.time does not exist).\nPlease " .
      "complete the previous step: -continue=-augustus\n";
  } elsif (-e "$runDir/augustus.gtf" || -e "$runDir/augustus.gtf.gz" ) {
    die "doMakeBed: looks like this was run successfully already\n" .
      "(augustus.gtf exists).  Either run with -continue load or cleanup\n" .
	"or move aside/remove $runDir/augustus.gtf\nand run again.\n";
  }

  my $whatItDoes = "Makes gtf/pep/bed files from gsBig output.";
  my $bossScript = newBash HgRemoteScript("$runDir/doMakeBed.bash", $workhorse,
				      $runDir, $whatItDoes);

  $bossScript->add(<<_EOF_
export db=$db
catDir -r gtf > augustus.gtf
catDir -r pep > augustus.pep
gtfToGenePred augustus.gtf \$db.augustus.gp
genePredToBed \$db.augustus.gp stdout | sort -k1,1 -k2,2n > \$db.augustus.bed
_EOF_
  );
  $bossScript->execute();
} # doMakeBed

#########################################################################
# * step: load [dbHost]
sub doLoadAugustus {
  my $runDir = $buildDir;
  &HgAutomate::mustMkdir($runDir);

  if (! -e "$runDir/augustus.gtf") {
    die "doLoadAugustus: the previous step makeBed did not complete \n" .
      "successfully (augustus.gtf does not exists).\nPlease " .
      "complete the previous step: -continue=-makeBed\n";
  } elsif (-e "$runDir/fb.$db.augustus.txt" ) {
    die "doLoadAugustus: looks like this was run successfully already\n" .
      "(fb.$db.augustus.txt exists).  Either run with -continue cleanup\n" .
	"or move aside/remove\n$runDir/fb.$db.augustus.txt and run again.\n";
  }
  my $whatItDoes = "Loads augustus.gtf.";
  my $bossScript = new HgRemoteScript("$runDir/doLoadAugustus.csh", $dbHost,
				      $runDir, $whatItDoes);

  $bossScript->add(<<_EOF_
bedToBigBed $db.augustus.bed ../../chrom.sizes $db.augustus.bb
gtfToGenePred augustus.gtf augustus.gp
genePredCheck -db=$db augustus.gp
ldHgGene -gtf $db augustus augustus.gtf
featureBits $db augustus >&fb.$db.augustus.txt
checkTableCoords -verboseBlocks -table=augustus $db
_EOF_
  );
  $bossScript->execute();
} # doLoad

#########################################################################
# * step: cleanup [fileServer]
sub doCleanup {
  my $runDir = $buildDir;

  if (-e "$runDir/augustus.gtf.gz" ) {
    die "doCleanup: looks like this was run successfully already\n" .
      "(augustus.gtf.gz exists).  Investigate the run directory:\n" .
	" $runDir/\n";
  }
  my $whatItDoes = "It cleans up or compresses intermediate files.";
  my $fileServer = &HgAutomate::chooseFileServer($runDir);
  my $bossScript = new HgRemoteScript("$runDir/doCleanup.csh", $fileServer,
				      $runDir, $whatItDoes);
  $bossScript->add(<<_EOF_
rm -rf hardMaskedFa/ err/ run.hardMask/err/
rm -f batch.bak bed.tab run.hardMask/batch.bak $db.augustus.gp $db.augustus.bed
gzip augustus.gtf augustus.gp augustus.pep
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

# Establish what directory we will work in.
$buildDir = $opt_buildDir ? $opt_buildDir :
  "$HgAutomate::clusterData/$db/$HgAutomate::trackBuild/augustus";
$maskedSeq = $opt_maskedSeq ? $opt_maskedSeq :
  "$HgAutomate::clusterData/$db/$db.2bit";
$utr = $opt_utr ? "on" : $utr;
$species = $opt_species ? $opt_species : $species;

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

