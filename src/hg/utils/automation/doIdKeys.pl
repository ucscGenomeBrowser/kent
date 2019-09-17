#!/usr/bin/env perl

# DO NOT EDIT the /cluster/bin/scripts copy of this file --
# edit ~/kent/src/hg/utils/automation/doIdKeys.pl instead.

# calculates md5sum strings for each sequence in a 2bit file
# and constructs a single md5sum from all those individual md5sums for a
# single 'keySignature' identifier for the entire sequence.

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
    /;

# Specify the steps supported with -continue / -stop:
my $stepper = new HgStepManager(
    [ { name => 'setup',   func => \&doSetup },
      { name => 'clusterRun',   func => \&doClusterRun },
      { name => 'finalResult',   func => \&doFinalResult },
      { name => 'cleanup', func => \&doCleanup },
    ]
				);

# Option defaults:
my $dbHost = 'hgwdev';
my $bigClusterHub = 'ku';
my $workhorse = 'hgwdev';
my $defaultWorkhorse = 'hgwdev';
my $twoBit = "$HgAutomate::clusterData/\$db/\$db.2bit";

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
                          $HgAutomate::clusterData/\$db/$HgAutomate::trackBuild/idKeys
    -twoBit seq.2bit      Use seq.2bit as the input sequence instead
                          of default ($twoBit).
_EOF_
  ;
  print STDERR &HgAutomate::getCommonOptionHelp('dbHost' => $dbHost,
						'workhorse' => $defaultWorkhorse,
						'bigClusterHub' => $bigClusterHub);
  print STDERR "
Automates the construction of an 'idKeys' file for a 2bit sequence
    The 'idKeys' are the md5sum results of each sequence in the 2bit file.
    Steps:
    setup: establish work directores and scripts for processing
    clusterRun: perform the cluster run
                cluster run is performed only when number of
                sequences is >= 5,000, else twoBitDup is used once
    finalResult: gather the results of the clusterRun (or twoBitDup) into
                a single results file '<db>.idKeys.txt': two colums:
                1. md5sum string (sorted on this column)
                2. sequence name (chromosome name, scaffold name, contig...)
    cleanup: Removes or compresses intermediate files.
All operations are performed in the build directory which is
$HgAutomate::clusterData/\$db/$HgAutomate::trackBuild/idKeys unless -buildDir is given.";
  # Detailed help (-help):
  print STDERR "
Assumptions:
1. 2bit file is a valid sequence file
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
# * step: setup [workhorse]
sub doSetup {
  my $runDir = "$buildDir";

  # First, make sure we're starting clean.
  if ( ! $opt_debug && ( -s "$runDir/doSetup.bash" ) ) {
    die "doSetup: looks like this was run successfully already " .
      "(directory db/bed/idKeys exists).  Either run with -continue clusterRun or some later " .
        "stage, or move aside/remove $runDir and run again.\n";
  }

  &HgAutomate::mustMkdir($runDir);

  my $whatItDoes = "Establish working directory and scripts to run the job.";
  my $bossScript = newBash HgRemoteScript("$runDir/doSetup.bash", $workhorse,
				      $runDir, $whatItDoes);

  $bossScript->add(<<_EOF_
twoBitInfo $twoBit stdout | sort -k2nr | cut -f1 > part.list
export partCount=`cat part.list | wc -l`
if [ "\${partCount}" -lt 5000 ]; then
  time ( twoBitDup -keyList=stdout $twoBit | grep -v "are identical" | sort > $db.idKeys.txt) > twoBitDup.log 2>&1
else
  mkdir -p splitList
  split -a 3 -d -l 5000 part.list splitList/part
  for F in splitList/part*
  do
    export B=`basename \$F`
    cat \$F | while read P
    do
      printf "runOne %s {check out exists+ result/%s/%s.txt}\n" \\
             "\${P}" "\${B}" "\${P}"
    done
  done > jobList

  printf '#!/bin/bash
set -beEu -o pipefail

export contig=\$1
export result=\$2
mkdir -p `dirname \$result`
sleep 1
touch `dirname \$result`
sleep 1

printf "%%s\\\\t%%s\\\\n" `twoBitToFa -noMask \\
  $twoBit:\${contig} stdout \\
   | grep -v "^>" | tr "[A-Z]" "[a-z]" | tr --delete "\\\\n" | md5sum \\
      | awk '"'"'{print \$1}'"'"'` "\${contig}" > "\${result}"
' > runOne
fi
_EOF_
  );
  $bossScript->execute();
} # doSetup

#########################################################################
# * step: clusterRun [bigClusterHub]
sub doClusterRun {
  my $runDir = "$buildDir";
  my $paraHub = $bigClusterHub;

  # First, make sure previous step has completed:
  if ( ! $opt_debug && ( ! -s "$runDir/part.list" ) ) {
    die "doClusterRun: previous 'setup' step has not completed, no part.list file present.\n";
  }
  # Then, make sure we're starting clean.
  if ( ! $opt_debug && ( -s "$runDir/run.time" ) ) {
    die "doClusterRun: looks like this was run successfully already " .
      "(file <db>/bed/idKeys/run.time exists).  Either run with -continue finalResult or some later " .
        "stage, or move aside/remove $runDir and run again.\n";
  }

  my $whatItDoes = "Perform cluster run if necessary.";
  my $bossScript = newBash HgRemoteScript("$runDir/clusterRun.bash", $paraHub,
				      $runDir, $whatItDoes);

  my $paraRun = &HgAutomate::paraRun();
  $bossScript->add(<<_EOF_
if [ -s runOne ]; then
  chmod +x runOne
  $paraRun
else
  if [ ! -s $db.idKeys.txt ]; then
     printf "ERROR: previous step doIdKeys failed twoBitDup procedure" 1>&2
     exit 255
  else
     cp -p twoBitDup.log run.time
  fi
fi
_EOF_
  );
  $bossScript->execute();
} # doClusterRun

#########################################################################
# * step: finalResult [workhorse]
sub doFinalResult {
  my $runDir = "$buildDir";

  # First, make sure previous step has completed:
  if ( ! $opt_debug && ( ! -s "$runDir/run.time" ) ) {
    die "doFinalResult: previous 'clusterRun' step has not completed, no run.time file present.\n";
  }
  # Then, make sure we're starting clean.
  if ( ! $opt_debug && ( -s "$runDir/doFinalResult.bash" ) ) {
    die "doFinalResult: looks like this was run successfully already " .
      "(file db/bed/doFinalResult.bash exists).  Either run with -continue cleanup " .
        ", or move aside/remove $runDir and run again.\n";
  }

  my $whatItDoes = "Collect cluster run results into one single result file, and construct the 'keySignature' for the entire sequence.";
  my $bossScript = newBash HgRemoteScript("$runDir/doFinalResult.bash",
                   $workhorse, $runDir, $whatItDoes);

  $bossScript->add(<<_EOF_
if [ -d result ]; then
  catDir result/part* | sort -k1,1 > $db.idKeys.txt
fi
if [ -s $db.idKeys.txt ]; then
  printf "# finalStep: $db.idKeys.txt file is present and done\\n" 1>&2
  cut -f 1 $db.idKeys.txt | md5sum | awk '{print \$1}' > $db.keySignature.txt
  cut -f1 $db.idKeys.txt | sort | uniq -c | awk '\$1 > 1' > $db.hasDups.txt
  if [ ! -s $db.hasDups.txt ]; then
    rm -f $db.hasDups.txt
  fi
else
  printf "ERROR: finalstep: $db.idKeys.txt file is missing\\n" 1>&2
  exit 255
fi
_EOF_
  );
  $bossScript->execute();
} # doFinalResult

#########################################################################
# * step: cleanup [workhorse]
sub doCleanup {
  my $runDir = "$buildDir";

  # Make sure we're starting clean.
  if ( ! $opt_debug && ( -s "$runDir/doCleanup.bash" ) ) {
    die "doCleanup: looks like this was run successfully already " .
      "(file db/bed/doCleanup.bash exists).\n";
  }
  # Verify previous step has completed
  if ( ! $opt_debug && ( ! -s "$runDir/$db.idKeys.txt" ) ) {
    die "doCleanup: ERROR: previous steps have not completed, there is no " .
      "file $runDir/$db.idKeys.txt present.\n";
  }

  my $whatItDoes = "It cleans up or compresses intermediate files.";
  my $bossScript = newBash HgRemoteScript("$runDir/doCleanup.bash", $workhorse,
				      $runDir, $whatItDoes);
  $bossScript->add(<<_EOF_
rm -fr err splitList result para.results batch para.bookmark batch.bak
gzip part.list
if [ -s jobList ]; then
  gzip jobList
fi
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

# Establish what directory we will work in.
$buildDir = $opt_buildDir ? $opt_buildDir :
  "$HgAutomate::clusterData/$db/$HgAutomate::trackBuild/idKeys";
$twoBit = $opt_twoBit ? $opt_twoBit :
  "$HgAutomate::clusterData/$db/$db.2bit";

if ( ! -s "$twoBit" ) {
  die "can not find 2bit file:\n\t$twoBit";
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
	"\n *** All done !$upThrough  Elapsed time: ${elapsedMinutes}m${elapsedSeconds}s\n");
&HgAutomate::verbose(1,
	" *** Steps were performed in $buildDir\n");
&HgAutomate::verbose(1, "\n");
