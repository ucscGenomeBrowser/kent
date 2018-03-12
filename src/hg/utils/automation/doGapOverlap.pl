#!/usr/bin/env perl

# DO NOT EDIT the /cluster/bin/scripts copy of this file --
# edit ~/kent/src/hg/utils/automation/doGapOverlap.pl instead.

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
    $opt_twoBit
    $opt_endSize
    /;

# Specify the steps supported with -continue / -stop:
my $stepper = new HgStepManager(
    [ { name => 'partition',   func => \&doPartition },
      { name => 'blat',   func => \&doBlat },
      { name => 'load',   func => \&doLoad },
      { name => 'cleanup', func => \&doCleanup },
    ]
				);

# Option defaults:
my $dbHost = 'hgwdev';
my $bigClusterHub = 'ku';
my $workhorse = 'hgwdev';
my $defaultWorkhorse = 'hgwdev';
my $twoBit = "$HgAutomate::clusterData/\$db/\$db.2bit";
my $defaultEndSize = 1000;
my $endSize = $defaultEndSize;

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
                          $HgAutomate::clusterData/\$db/$HgAutomate::trackBuild/gapOverlap
    -twoBit seq.2bit      Use seq.2bit as the input sequence instead
                          of default ($twoBit).
    -endSize N            Use size N for sequence next to gap instead of
                          default $defaultEndSize bases (limit: 5000)
_EOF_
  ;
  print STDERR &HgAutomate::getCommonOptionHelp('dbHost' => $dbHost,
						'workhorse' => $defaultWorkhorse,
						'bigClusterHub' => $bigClusterHub,
						'smallClusterHub' => '');
  print STDERR "
Automates UCSC's build of gapOverlap track.  Steps:
    partition:  Establish work list of sequence definitions on each side of gaps
    blat:       Run blat on the paired list of sequences
    load:       Collect results into bed file and load table '<db>.gapOverlap'
    cleanup:    Removes or compresses intermediate files.
All operations are performed in the build directory which is
$HgAutomate::clusterData/\$db/$HgAutomate::trackBuild/gapOverlap unless -buildDir is given.
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
my ($buildDir, $secondsStart, $secondsEnd, $dbExists);

sub checkOptions {
  # Make sure command line options are valid/supported.
  my $ok = GetOptions(@HgStepManager::optionSpec,
		      'buildDir=s',
		      'twoBit=s',
		      'endSize=i',
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
  my $runDir = "$buildDir";

  # First, make sure we're starting clean.
  if ( ! $opt_debug && ( -s "$runDir/runOne" ) ) {
    die "partition: looks like this was run successfully already " .
      "(file db/bed/gapOverlap/runOne exists).  Either run with -continue blat or some later " .
        "stage, or move aside/remove $runDir and run again.\n";
  }

  &HgAutomate::mustMkdir($runDir);

  my $whatItDoes = "Set up paired list of sequences to blat.";
  my $bossScript = newBash HgRemoteScript("$runDir/partition.bash",
		$workhorse, $runDir, $whatItDoes);

  $bossScript->add(<<_EOF_
export db=$db
$Bin/nBedToEnds.pl $twoBit $endSize | gzip -c > \$db.pairedEnds.tab.gz
export count=`zcat \$db.pairedEnds.tab.gz | wc -l`
if [ "\$count" -lt 1 ]; then
   touch \$db.gapOverlap.bed
   exit 0
fi

mkdir -p blatPairs

printf '#LOOP
runOne blatPairs/\$(path1) {check out exists psl/\$(path1).psl}
#ENDLOOP
' > template

printf '#!/bin/bash
listIn=\$1
resultFile=\$2
resultDir=`dirname \$resultFile`
mkdir -p \$resultDir
cat \$listIn | tr "[\\t]" "[ ]" | while read sequencePair
do
withDb=`echo \$sequencePair | sed -e "s#^#$twoBit:#; s# # $twoBit:#;"`
blat \$withDb -q=dna -minIdentity=95 -repMatch=10 -noHead stdout \\
   | awk -F"\\t" "\\\$1 > 0 && \\\$2 == 0 && \\\$12 == 0 && \\\$17 == \\\$15 && \\\$18 == 1"
done > \$resultFile
' > runOne

mkdir psl

if [ "\$count" -lt 10000 ]; then
 zcat \$db.pairedEnds.tab.gz \\
  | split --suffix-length=1 --lines=1000 --numeric-suffixes - blatPairs/blatList
  cd blatPairs
  find . -type f | grep blatList | sed -e 's#^./##;' > pair.list
  exit 0
else
 export size10=`echo \$count | awk '{printf "%d", 1+\$1/10}'`
 zcat \$db.pairedEnds.tab.gz \\
  | split --suffix-length=1 --lines=\$size10 --numeric-suffixes - blatPairs/blatList
 cd blatPairs
ls  blatList[0-9]* | while read F
do
  N=\${F/blatList/}
  mkdir \$N
  mv \$F \$N
  split --suffix-length=4 --lines=1000 --numeric-suffixes \$N/\$F \$N/blatList
  rm -f \$N/\$F
done

find . -type f | grep blatList | sed -e 's#^./##;' > pair.list

fi
_EOF_
  );
  $bossScript->execute();
} # doPartition

#########################################################################
# * step: blat [bigClusterHub]
sub doBlat {
  my $runDir = "$buildDir";
  my $paraHub = $bigClusterHub;

  # already have this done, could be empty file from partition step
  return if ( ! $opt_debug && ( -e "$runDir/$db.gapOverlap.bed" ) );
  return if ( ! $opt_debug && ( -e "$runDir/$db.gapOverlap.bed.gz" ) );

  # First, make sure we're starting clean.
  if ( ! $opt_debug && ( -s "$runDir/run.time" ) ) {
    die "doBlat looks like this was run successfully already " .
      "(file db/bed/gapOverlap/run.time exists).  Either run with -continue load or some later " .
        "stage, or move aside/remove $runDir and run again.\n";
  }

  # Verify partition step has completed
  if ( ! $opt_debug && ( ! -s "$runDir/template" ) ) {
    die "doBlat looks like partition step has not been competed, " .
      "file db/bed/gapOverlap/template does not exists. run with: -continue partition to perform required step.\n";
  }

  my $whatItDoes = "Run blat on the paired sequences, collect results.";
  my $bossScript = newBash HgRemoteScript("$runDir/blat.bash",
		$paraHub, $runDir, $whatItDoes);

  my $paraRun = &HgAutomate::paraRun();
  my $gensub2 = &HgAutomate::gensub2();
  $bossScript->add(<<_EOF_
chmod +x runOne
$gensub2 blatPairs/pair.list single template jobList
$paraRun

find ./psl -type f | grep "\.psl\$" | xargs cat | gzip -c > $db.gapOverlap.psl.gz
count=`zcat $db.gapOverlap.psl.gz | wc -l`
if [ "\$count" -lt 1 ]; then
   printf "No PSL results found, no items to load.  Successful procedure.\\n"
   touch $db.gapOverlap.bed
   exit 0
fi
zcat $db.gapOverlap.psl.gz | $Bin/overlapPslToBed.pl stdin | sort -k1,1 -k2,2n > $db.gapOverlap.bed
twoBitInfo $twoBit stdout | sort -k2nr > $db.chrom.sizes
bedToBigBed -type=bed12 $db.gapOverlap.bed $db.chrom.sizes $db.gapOverlap.bb
_EOF_
    );
  $bossScript->execute();
} # doBlat

#########################################################################
# * step: load [dbHost]
sub doLoad {
  my $runDir = "$buildDir";

  # nothing to do here if db does not exist:
  return if ( ! $opt_debug && ! $dbExists );

  # First, make sure we're starting clean.
  if ( ! $opt_debug && ( -s "$runDir/loadUp.bash" ) ) {
    die "doLoad looks like this was run successfully already, " .
      "file db/bed/loadUp.bash exists.  Can run with: -continue cleanup " .
        "to complete this procedure.\n";
  }
  # And, must have something to load
  if ( ! $opt_debug && ( ! -s "$runDir/$db.gapOverlap.bed" ) ) {
    die "doLoad does not find result from blat run: $db.gapOverlap.bed, " .
      "Can run with: -continue blat or check why blat run was empty.\n";
  }

  my $whatItDoes = "load gapOverlap table if results and database exist.";
  my $bossScript = newBash HgRemoteScript("$runDir/loadUp.bash",
		$workhorse, $runDir, $whatItDoes);

  $bossScript->add(<<_EOF_
# do not load empty files
if [ -s $db.gapOverlap.bed ]; then
  hgLoadBed -type=bed12 $db gapOverlap $db.gapOverlap.bed
  checkTableCoords $db gapOverlap
  featureBits -countGaps $db gapOverlap > fb.$db.gapOverlap.txt 2>&1
fi
_EOF_
  );
  $bossScript->execute();
} # doLoad

#########################################################################
# * step: cleanup [workhorse]
sub doCleanup {
  my $runDir = "$buildDir";
  my $whatItDoes = "Clean up and/or compress intermediate files.";
  my $bossScript = new HgRemoteScript("$runDir/doCleanup.csh", $workhorse,
				      $runDir, $whatItDoes);
  $bossScript->add(<<_EOF_
rm -fr psl blatPairs $db.chrom.sizes
gzip $db.gapOverlap.bed
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
# $opt_debug = 1;
$opt_verbose = 3 if ($opt_verbose < 3);

# Establish directory to work in:
$buildDir = $opt_buildDir ? $opt_buildDir :
  "$HgAutomate::clusterData/$db/$HgAutomate::trackBuild/gapOverlap";
$twoBit = $opt_twoBit ? $opt_twoBit :
  "$HgAutomate::clusterData/$db/$db.2bit";
$endSize = $opt_endSize ? $opt_endSize : $defaultEndSize;
die "-endSize can not be larger than 5000" if ($endSize > 5000);

# may be working on a 2bit file that does not have a database browser
$dbExists = 0;
$dbExists = 1 if (&HgAutomate::databaseExists($dbHost, $db));

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
	"\n *** All done !$upThrough  Elapsed time: ${elapsedMinutes}m${elapsedSeconds}s\n");
&HgAutomate::verbose(1,
	" *** Steps were performed in $buildDir\n");
&HgAutomate::verbose(1, "\n");

