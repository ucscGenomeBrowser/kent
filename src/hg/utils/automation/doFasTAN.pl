#!/usr/bin/env perl

# DO NOT EDIT the /cluster/bin/scripts copy of this file --
# edit ~/kent/src/hg/utils/automation/doFasTAN.pl instead.

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
    $opt_unmaskedSeq
    /;

# Specify the steps supported with -continue / -stop:
my $stepper = new HgStepManager(
    [ { name => 'setup',   func => \&doSetup },
      { name => 'cluster', func => \&doCluster },
      { name => 'bedResult', func => \&doBedResult },
      { name => 'cleanup', func => \&doCleanup },
    ]
				);

# FasTAN and tanbed commands
my $FasTAN = "/cluster/bin/x86_64/FasTAN";
my $tanbed = "/cluster/bin/x86_64/tanbed";

# Option defaults:
my $bigClusterHub = 'hgwdev';
my $defaultWorkhorse = 'least loaded';
my $dbHost = 'hgwdev';
my $unmaskedSeq = "";

my $base = $0;
$base =~ s/^(.*\/)?//;

sub usage {
  # Usage / help / self-documentation:
  my ($status, $detailed) = @_;
  # Basic help (for incorrect usage):
  print STDERR "usage: $base db path/to/unmasked.2bit
options:
";
  print STDERR $stepper->getOptionHelp();
  print STDERR <<_EOF_
    -buildDir dir         Use dir instead of default
                          $HgAutomate::clusterData/\$db/$HgAutomate::trackBuild/longdust.\$date
                          (necessary when continuing at a later date).
_EOF_
  ;
  print STDERR HgAutomate::getCommonOptionHelp('dbHost' => $dbHost,
					'workhorse' => $defaultWorkhorse);
  print STDERR "
Automates Gene Myers' 'FasTAN' process for the given unmasked.2bit sequence.
Steps:
    setup:   Prepares partitioned sequence listings from the given unmasked.2bit
    cluster: Does the cluster run of 'FasTAN' on the partitioned sequences
    bedResult: Gathers the individual cluster job results into
               one .bed and .bb bigBed result
    cleanup: Removes temporary files
All operations are performed in the build directory which is
$HgAutomate::clusterData/\$db/$HgAutomate::trackBuild/fasTan.\$date unless -buildDir is given.";
  # There is no detailed help (-help):
  print "\n";
  exit $status;
}


# Globals:
# Command line args: db
my ($db);
# Other:
my ($buildDir);

sub checkOptions {
  # Make sure command line options are valid/supported.
  my $ok = GetOptions(@HgStepManager::optionSpec,
		      'buildDir=s',
		      'unmaskedSeq=s',
		      @HgAutomate::commonOptionSpec,
		      );
  usage(1) if (!$ok);
  usage(0, 1) if ($opt_help);
  HgAutomate::processCommonOptions();
  my $err = $stepper->processOptions();
  usage(1) if ($err);
  $dbHost = $opt_dbHost if ($opt_dbHost);
}

#########################################################################
# * step: setup [workhorse]
sub doSetup {
  my $runDir = "$buildDir";
  if ( ! $opt_debug && (-s "$runDir/chrom.sizes" && -s "$runDir/part.list" )) {
     printf STDERR "# setup step already complete\n";
     return;
  }
  if (! $opt_debug) {
    my @outs = ("$runDir/setup.bash",
                "$runDir/chrom.sizes",
                "$runDir/part.list");
    HgAutomate::checkCleanSlate('setup', 'cluster', @outs);
  }
  HgAutomate::mustMkdir($runDir);

  my $whatItDoes = "prepare files for FasTAN cluster run.";
  my $workhorse = $opt_debug ? "hgwdev" : HgAutomate::chooseWorkhorse();
  my $bossScript = newBash HgRemoteScript("$runDir/setup.bash", $workhorse,
				      $runDir, $whatItDoes);

  my $tmpDir = HgAutomate::tmpDir();
  $bossScript->add(<<_EOF_
rm -fr unmasked.2bit listFiles
ln -s "$unmaskedSeq" unmasked.2bit

twoBitInfo unmasked.2bit stdout | sort -k2nr > chrom.sizes

export seqMax=`head -1 chrom.sizes | awk '{printf "%d", \$2+1}'`

partitionSequence.pl -lstDir listFiles \$seqMax 0 \\
  unmasked.2bit chrom.sizes 10000
ls -S listFiles/*.lst > part.list
printf "#####################################################\n" > version.txt
printf "# FasTAN version: %s\n" "`ls -og $FasTAN`" >> version.txt
printf "# tanbed version: %s\n" "`ls -og $tanbed`" >> version.txt
printf "#####################################################\n" >> version.txt
printf "# running FasTAN with -T8 option for 8 threads\n" >> version.txt
printf "#####################################################\n" >> version.txt
printf "# %s\n" "$FasTAN -v tmp/seqName.fa tmp/seqName" >> version.txt
printf "# %s\n" "$tanbed tmp/seqName.1aln >> result/seqName.bed.gz" >> version.txt
printf "#####################################################\n" >> version.txt
$FasTAN >> version.txt 2>&1 || true
printf "#####################################################\n" >> version.txt
_EOF_
  );
  $bossScript->execute() if (! $opt_debug);
} # doSetup

#########################################################################
# * step: cluster [bigClusterHub]
sub doCluster {
  my $paraHub = $bigClusterHub;
  my $runDir = "$buildDir";
  if ( ! $opt_debug && -s "$runDir/run.time") {
     printf STDERR "# cluster step already complete\n";
     return;
  }
  my $partList = "part.list";	# from doSetup
  HgAutomate::checkExistsUnlessDebug('setup', 'bedResult', "$runDir/part.list");
  my $whatItDoes = "Cluster run FasTAN on the part.list sequences.  Results into ./result/*.bed.gz";
  my $templateCmd = ('runOne $(path1) {check out exists result/$(root1).bed.gz}');
  HgAutomate::makeGsub($runDir, $templateCmd);
  `touch "$runDir/para_hub_$paraHub"`;
  my $paraRun = <<'_EOF_';
# 24 Gb = 3 Gb each thread X 8 threads
para make -cpu=8 -ram=24g jobList
para check
para time > run.time
cat run.time
_EOF_

  my $bossScript = newBash HgRemoteScript("$runDir/doCluster.bash", $paraHub,
				      $runDir, $whatItDoes);
  $bossScript->add(<<_EOF_
cat <<'_EOM_' > runOne
#!/bin/bash

set -beEu -o pipefail

# do not let a lot of jobs all start up at the same instant
# sleep between 12 and 32 seconds
export sleepTime=\$((RANDOM % 21 + 12))
sleep "\$sleepTime"

export listFile="\${1}"
export resultBedGz="\${2}"
export resultBed="\${resultBedGz%.gz}"

mkdir -p tmp result

rm -f "\${resultBed}" "\${resultBedGz}"
touch "\${resultBed}"

for seqSpec in `cat \$listFile`
do
   seqName=`basename \$seqSpec | cut -d":" -f2`
   rm -f tmp/\${seqName}.fa
   twoBitToFa \$seqSpec tmp/\${seqName}.fa
   $FasTAN -T8 -v tmp/\${seqName}.fa tmp/\${seqName}
   $tanbed tmp/\${seqName}.1aln >> \${resultBed}
   rm -f tmp/\${seqName}.fa tmp/\$seqName.1aln
done
gzip \${resultBed}
_EOM_

chmod +x runOne

gensub2 $partList single gsub jobList
$paraRun
_EOF_
  );

  $bossScript->execute() if (! $opt_debug);
} # doCluster

#########################################################################
# * step: bedResult [fileServer]
sub doBedResult {
  my $runDir = "$buildDir";
  if ( ! $opt_debug && -s "$runDir/fasTAN.bb") {
     printf STDERR "# bedResult step already complete\n";
     return;
  }
  my $whatItDoes = "Consolidate the cluster run bed.gz files.  Make single bed and bigBed file.";
  HgAutomate::checkExistsUnlessDebug('cluster', 'cleanup', "$runDir/run.time");
  my $fileServer = $opt_debug ? "hgwdev" : HgAutomate::chooseFileServer($runDir);
  my $bossScript = newBash HgRemoteScript("$runDir/makeBed.bash", $fileServer,
				      $runDir, $whatItDoes);
  $bossScript->add(<<_EOF_
if [ -s ~/kent/src/hg/lib/fasTAN.as ]; then
  cp -p ~/kent/src/hg/lib/fasTAN.as ./
else
  wget --no-check-certificate -O fasTAN.as 'https://raw.githubusercontent.com/ucscGenomeBrowser/kent/refs/heads/master/src/hg/lib/fasTAN.as'
fi
ls -S result/*.bed.gz | xargs zcat | gzip -c > fasTAN.bed.gz
bedToBigBed -type=bed3+2 -as=fasTAN.as fasTAN.bed.gz chrom.sizes fasTAN.bb
export totalBases=`ave -col=2 chrom.sizes | grep total | awk '{printf "%d", \$NF}'`
export basesCovered=`bigBedInfo fasTAN.bb | grep basesCovered | awk '{printf "%s", \$NF}' | tr -d ','`
export percentCovered=`echo \$basesCovered \$totalBases | awk '{printf "%.2f", 100*\$1/\$2}'`
printf "%d bases of %d (%s%%) in intersection\n" "\$basesCovered" "\$totalBases" "\$percentCovered" > fb.fasTAN.txt
cat fb.fasTAN.txt
_EOF_
  );
  $bossScript->execute() if (! $opt_debug);
} #doBedResult

#########################################################################
# * step: cleanup [fileServer]
sub doCleanup {
  my $runDir = "$buildDir";
  if ( ! $opt_debug && ( ! -d "$runDir/tmp" && ! -d "$runDir/result")) {
     printf STDERR "# cleanup step already complete\n";
     return;
  }
  my $whatItDoes = "Cleans up or compresses intermediate files.";
  my $fileServer = $opt_debug ? "hgwdev" : HgAutomate::chooseFileServer($runDir);
  my $bossScript = newBash HgRemoteScript("$runDir/cleanup.bash", $fileServer,
				      $runDir, $whatItDoes);
  $bossScript->add(<<_EOF_
rm -fr tmp result liftFiles err part.list chrom.sizes
_EOF_
  );
  $bossScript->execute() if (! $opt_debug);
} # doCleanup

#########################################################################
# main

# Prevent "Suspended (tty input)" hanging:
HgAutomate::closeStdin();

# Make sure we have valid options and exactly 1 argument:
checkOptions();
usage(1) if (scalar(@ARGV) != 2);
my $secondsStart = `date "+%s"`;
chomp $secondsStart;
($db, $unmaskedSeq) = @ARGV;

# Force debug and verbose until this is looking pretty solid:
#$opt_debug = 1;
$opt_verbose = 3 if ($opt_verbose < 3);

# Establish what directory we will work in.
my $date = `date +%Y-%m-%d`;
chomp $date;
$buildDir = $opt_buildDir ? $opt_buildDir :
  "$HgAutomate::clusterData/$db/$HgAutomate::trackBuild/longdust.$date";

# Do everything.
$stepper->execute();

# Tell the user anything they should know.
my $stopStep = $stepper->getStopStep();
my $upThrough = ($stopStep eq 'cleanup') ? "" :
  "  (through the '$stopStep' step)";

my $secondsEnd = `date "+%s"`;
chomp $secondsEnd;
my $elapsedSeconds = $secondsEnd - $secondsStart;
my $elapsedMinutes = int($elapsedSeconds/60);
$elapsedSeconds -= $elapsedMinutes * 60;

HgAutomate::verbose(1,
	"\n *** All done !$upThrough - Elapsed time: ${elapsedMinutes}m${elapsedSeconds}s\n");
HgAutomate::verbose(1,
	" *** Steps were performed in $buildDir\n");
HgAutomate::verbose(1, "\n");
