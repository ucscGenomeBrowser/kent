#!/usr/bin/env perl

# DO NOT EDIT the /cluster/bin/scripts copy of this file --
# edit ~/kent/src/hg/utils/automation/doTandemDup.pl instead.

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
    $opt_kmerSize
    $opt_gapSize
    $opt_buildDir
    $opt_twoBit
    /;

# Specify the steps supported with -continue / -stop:
my $stepper = new HgStepManager(
    [ { name => 'kmers',   func => \&doKmers },
      { name => 'pairedEnds',   func => \&doPairedEnds },
      { name => 'collapsePairedEnds',   func => \&doCollapsePairedEnds },
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
my $kmerSize = 30;	# initial default kmer size
my $gapSize = 20000;	# initial default gap size

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
    -kmerSize N       Initial kmer size N instead of default: $kmerSize
    -gapSize N        Initial gap size N instead of default: $gapSize
    -buildDir dir     Use dir instead of default
                      $HgAutomate::clusterData/\$db/$HgAutomate::trackBuild/tandemDups
    -twoBit seq.2bit  Use seq.2bit as the input sequence instead
                      of default ($twoBit).

_EOF_
  ;
  print STDERR &HgAutomate::getCommonOptionHelp('dbHost' => $dbHost,
						'workhorse' => $defaultWorkhorse,
						'bigClusterHub' => $bigClusterHub,
						'smallClusterHub' => '');
  print STDERR "
Automates UCSC's build of tandemDups track.  Steps:
    kmers - generate kmers over all sequence of size $kmerSize
    pairedEnds - pair up all kmers that are identical with gap from 1 to $gapSize
    collapsePairedEnds - collapse the pairedEnds when overlapping and same gap size
    load:       Collect results into bed file and load table '<db>.tandemDups'
    cleanup:    Removes or compresses intermediate files.
All operations are performed in the build directory which is
$HgAutomate::clusterData/\$db/$HgAutomate::trackBuild/tandemDups unless -buildDir is given.
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
my ($buildDir, $secondsStart, $secondsEnd, $dbExists, $kmersMinus1);

sub checkOptions {
  # Make sure command line options are valid/supported.
  my $ok = GetOptions(@HgStepManager::optionSpec,
		      'kmerSize=i',
		      'gapSize=i',
		      'buildDir=s',
		      'twoBit=s',
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
# * step: kmers [bigClusterHub]
sub doKmers {
  my $runDir = "$buildDir/kmers";
  my $paraHub = $bigClusterHub;

  # First, make sure we're starting clean.
  if ( ! $opt_debug && ( -s "$runDir/runOne" ) ) {
    die "kmers: looks like this was run successfully already " .
      "(file db/bed/tandemDups/kmers/runOne exists).  Either run with -continue pairedEnds or some later " .
        "stage, or move aside/remove $runDir and run again.\n";
  }

  &HgAutomate::mustMkdir($runDir);

  my $templateCmd = ('runOne $(path1) {check out exists tmp/$(path1).' .
            "$kmersMinus1" . 'mer.txt.gz}');
  &HgAutomate::makeGsub($runDir, $templateCmd);
  `touch "$runDir/para_hub_$paraHub"`;

  my $whatItDoes = "Generate kmers for entire sequence.";
  my $bossScript = newBash HgRemoteScript("$runDir/runKmers.bash",
		$paraHub, $runDir, $whatItDoes);

  my $paraRun = &HgAutomate::paraRun();
  $bossScript->add(<<_EOF_
twoBitInfo $twoBit stdout | cut -f1 > part.list
printf '#!/bin/bash

set -beEu -o pipefail

export fa=\$1
export result=\$2

mkdir -p tmp

twoBitToFa ${twoBit}:\$fa stdout \\
  | $Bin/kmerPrint.pl $kmersMinus1 stdin | gzip -c > \$result
' > runOne
chmod +x runOne
gensub2 part.list single gsub jobList
$paraRun
_EOF_
  );
  $bossScript->execute();
} # doKmers()

#########################################################################
# * step: pairedEnds [bigClusterHub]
sub doPairedEnds {
  my $prevRunDir = "$buildDir/kmers";
  my $runDir = "$buildDir/pairedEnds";
  my $paraHub = $bigClusterHub;

  # First, make sure we're starting clean.
  if ( ! $opt_debug && ( -s "$runDir/runOne" ) ) {
    die "pairedEnds: looks like this was run successfully already " .
      "(file db/bed/tandemDups/pairedEnds/runOne exists).  Either run with -continue blat or some later " .
        "stage, or move aside/remove $runDir and run again.\n";
  }
  if ( ! $opt_debug && ( ! -s "$prevRunDir/run.time" ) ) {
    die "pairedEnds: looks like previous kmers step has not been competed, " .
      "file db/bed/tandemDups/kmers/run.time does not exist. run with: -continue kmers to perform previous step.\n";
  }

  &HgAutomate::mustMkdir($runDir);

  my $templateCmd = ("runOne $kmersMinus1 $gapSize " . '$(path1) {check out exists tmp/$(path1).bed.gz}');
  &HgAutomate::makeGsub($runDir, $templateCmd);
  `touch "$runDir/para_hub_$paraHub"`;

  my $whatItDoes = "Collect kmers into pairs.";
  my $bossScript = newBash HgRemoteScript("$runDir/runPairedEnds.bash",
		$paraHub, $runDir, $whatItDoes);

  my $paraRun = &HgAutomate::paraRun();
  $bossScript->add(<<_EOF_
ln -s $prevRunDir/part.list .
printf '#!/bin/bash

set -beEu -o pipefail

export kmerSize=\$1
export gapSize=\$2
export chrName=\$3
export result=\$4

mkdir -p tmp

$Bin/kmerPairs.pl \$kmerSize \$gapSize \$chrName \$result
' > runOne
chmod +x runOne
gensub2 part.list single gsub jobList
$paraRun
_EOF_
  );
  $bossScript->execute();
} # doPairedEnds()

#########################################################################
# * step: collapsePairedEnds [bigClusterHub]
sub doCollapsePairedEnds {
  my $prevRunDir = "$buildDir/pairedEnds";
  my $runDir = "$buildDir/collapsePairedEnds";
  my $paraHub = $bigClusterHub;

  # First, make sure we're starting clean.
  if ( ! $opt_debug && ( -s "$runDir/runOne" ) ) {
    die "collapsePairedEnds: looks like this was run successfully already " .
      "(file db/bed/tandemDups/collapsePairedEnds/runOne exists).  Either run with -continue load or some later " .
        "stage, or move aside/remove $runDir and run again.\n";
  }
  if ( ! $opt_debug && ( ! -s "$prevRunDir/run.time" ) ) {
    die "pairedEnds: looks like previous pairedEnds step has not been competed, " .
      "file db/bed/tandemDups/pairedEnds/run.time does not exist. run with: -continue pairedEnds to perform previous step.\n";
  }

  &HgAutomate::mustMkdir($runDir);

  my $templateCmd = ('runOne $(path1) {check out exists tmp/$(path1).bed.gz}');
  &HgAutomate::makeGsub($runDir, $templateCmd);
  `touch "$runDir/para_hub_$paraHub"`;

  my $whatItDoes = "Collapse kmers pairs into larger paired ends.";
  my $bossScript = newBash HgRemoteScript("$runDir/runCollapsePairedEnds.bash",
		$paraHub, $runDir, $whatItDoes);

  my $paraRun = &HgAutomate::paraRun();
  $bossScript->add(<<_EOF_
ln -s $prevRunDir/part.list .
printf '#!/bin/bash

set -beEu -o pipefail

export chrName=\$1
export result=\$2
mkdir -p tmp

$Bin/kmerCollapsePairedEnds.pl $kmersMinus1 ../pairedEnds/tmp/\$chrName.bed.gz \\
   | gzip -c > tmp/\$chrName.bed.gz
' > runOne
chmod +x runOne
gensub2 part.list single gsub jobList
$paraRun
_EOF_
  );
  $bossScript->execute();
} # doCollapsePairedEnds()

#########################################################################
# * step: load [dbHost]
sub doLoad {
  my $prevRunDir = "$buildDir/collapsePairedEnds";
  my $runDir = "$buildDir";

  # First, make sure we're starting clean.
  if ( ! $opt_debug && ( -s "$runDir/loadUp.bash" ) ) {
    die "load: looks like this was run successfully already, " .
      "file db/bed/tandemDups/loadUp.bash exists.  Can run with: -continue cleanup " .
        "to complete this procedure.\n";
  }
  # And, must have something to load
  if ( ! $opt_debug && ( ! -s "$prevRunDir/run.time" ) ) {
    die "load: previous step collapsePairedEnds has not completed,\n" .
      "Can run with: -continue collapsePairedEnds or check why previous step has not completed.\n";
  }

  my $whatItDoes = "Collect results into single file, if db exists, load table tandemDups.";
  my $bossScript = newBash HgRemoteScript("$runDir/loadUp.bash",
		$workhorse, $runDir, $whatItDoes);

  $bossScript->add(<<_EOF_
for F in collapsePairedEnds/tmp/*.bed.gz
do
  zcat \$F | awk '\$5 > $kmersMinus1'
done | sort -k1,1 -k2,2n | gzip -c > $db.tandemDups.bed.gz
twoBitInfo $twoBit stdout | sort -k2nr > $db.chrom.sizes
export maxScore=`zcat $db.tandemDups.bed.gz | cut -f5 | sort -k1,1nr | head -1`
zcat $db.tandemDups.bed.gz | awk -vmax=\$maxScore '{score=\$5; newScore=int(1000*score/max); printf "%s\\t%s\\t%s\\t%s\\t", \$1, \$2, \$3, \$4; printf "%s\\t%s\\t%s\\t%s\\t%s\\t%s\\t%s\\t%s\\t%s\\n", newScore, \$6, \$7, \$8, \$9, \$10, \$11, \$12, score}' > $db.reScore.bed
bedToBigBed -type=bed12+1 $db.reScore.bed $db.chrom.sizes $db.tandemDups.bb
bedToExons $db.tandemDups.bed.gz stdout | bedSingleCover.pl stdin > $db.exons.bed
export baseCount=`awk '{sum+=\$3-\$2}END{printf "%d", sum}' $db.exons.bed`
export asmSize=`awk '{sum+=\$2}END{printf "%d", sum}' $db.chrom.sizes`
export perCent=`echo \$baseCount \$asmSize | awk '{printf "%.3f", 100.0*\$1/\$2}'`
printf "%d bases of %d (%s%%) in intersection\\n" "\$baseCount" "\$asmSize" "\$perCent" > fb.$db.tandemDups.txt
_EOF_
  );

  # do not load if db does not exist:
  if ( $opt_debug || $dbExists ) {
      $bossScript->add(<<_EOF_
# do not load empty files
if [ -s $db.reScore.bed ]; then
  hgLoadBed -as=\$HOME/kent/src/hg/lib/fullBed.as -type=bed4+8 $db tandemDups $db.tandemDups.bed.gz
  checkTableCoords $db tandemDups
fi
_EOF_
      );
  }

  $bossScript->execute();
} # doLoad

#########################################################################
# * step: cleanup [workhorse]
sub doCleanup {
  my $runDir = "$buildDir";

  # Skip this if already done
  if ( ! $opt_debug && ( -s "$runDir/doCleanup.csh" ) ) {
    printf STDERR "# cleanup already done\n";
  } else {
    my $whatItDoes = "Clean up and/or compress intermediate files.";
    my $bossScript = new HgRemoteScript("$runDir/doCleanup.csh", $workhorse,
				      $runDir, $whatItDoes);
    $bossScript->add(<<_EOF_
rm -f $db.reScore.bed $db.exons.bed
rm -fr kmers/tmp pairedEnds/tmp collapsePairedEnds/tmp $db.chrom.sizes bed.tab
rm -fr kmers/err pairedEnds/err collapsePairedEnds/err
_EOF_
    );
    $bossScript->execute();
  }
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
  "$HgAutomate::clusterData/$db/$HgAutomate::trackBuild/tandemDups";
$twoBit = $opt_twoBit ? $opt_twoBit :
  "$HgAutomate::clusterData/$db/$db.2bit";
$kmerSize = $opt_kmerSize ? $opt_kmerSize : $kmerSize;
# initial kmer set are size one less than specified kmerSize
# this gets the first set of kmer pairs established correctly at size kmerSize
$kmersMinus1 = $kmerSize - 1;
$gapSize = $opt_gapSize ? $opt_gapSize : $gapSize;
&HgAutomate::verbose(1, "# kmer size: $kmerSize\n");
&HgAutomate::verbose(1, "#  gap size: $gapSize\n");
&HgAutomate::verbose(1, "# 2bit file: $twoBit\n");
die "kmerSize must be >= 30, this is the lower limit" if ($kmerSize < 30);
die "gapSize must be >= 1, this is the lower limit" if ($gapSize < 1);
die "can not find 2bit file: '$twoBit'" if ( ! -s $twoBit);

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
