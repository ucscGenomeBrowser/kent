#!/usr/bin/env perl

# DO NOT EDIT the /cluster/bin/scripts copy of this file --
# edit ~/kent/src/hg/utils/automation/doRepeatModeler.pl instead.

use Getopt::Long;
use warnings;
use strict;
use Carp;
use FindBin qw($Bin);
use lib "$Bin";
use HgAutomate;
use HgRemoteScript;
use HgStepManager;

# Hardcoded command path:
my $RepeatModelerPath = "/hive/data/outside/RepeatModeler-2.0.7";
my $RepeatModeler = "$RepeatModelerPath/RepeatModeler";
my $BuildDatabase = "$RepeatModelerPath/BuildDatabase";
# configured to consume one entire ku machine node
my $threadCount = "-threads 32";
my $parasolOpts = "-cpu=32 -ram=128g";
# Option defaults
my $bigClusterHub = 'hgwdev';
my $workhorse = "hgwdev";
my $defaultWorkhorse = 'hgwdev';

# Option variable names, both common and peculiar to this script:
use vars @HgAutomate::commonOptionVars;
use vars @HgStepManager::optionVars;
use vars qw/
    $opt_buildDir
    $opt_unmaskedSeq
    /;

# Specify the steps supported with -continue / -stop:
my $stepper = new HgStepManager(
    [ { name => 'blastDb', func => \&doBlastDb },
      { name => 'cluster',     func => \&doCluster },
      { name => 'cleanup', func => \&doCleanup },
    ]
				);

# Option defaults:
my $dbHost = 'hgwdev';
my $unmaskedSeq = "\$db.unmasked.2bit";

my $base = $0;
$base =~ s/^(.*\/)?//;

sub usage {
  # Usage / help / self-documentation:
  my ($status, $detailed) = @_;
  # Basic help (for incorrect usage):
  print STDERR "
usage: $base db
options:
    the db argument is a UCSC database name or the assembly identifier
    for a GenArk assembly hub build
";
  print STDERR $stepper->getOptionHelp();
  print STDERR <<_EOF_
    -buildDir dir         Use dir instead of default
                          $HgAutomate::clusterData/\$db/$HgAutomate::trackBuild/repeatModeler.\$date
                          (necessary when continuing at a later date).
    -unmaskedSeq seq.2bit Use seq.2bit as the unmasked input sequence instead
                          of default ($unmaskedSeq).
_EOF_
  ;
  print STDERR &HgAutomate::getCommonOptionHelp('dbHost' => $dbHost,
						'workhorse' => '',
						'bigClusterHub' => '');
  print STDERR "
Automates the RepeatModeler process for genome assembly \$db.  Steps:
    blastDb: construct fasta file from unmasked.2bit and rmblastn index files.
    cluster: Parasol cluster run of RepeatModeler.
    cleanup: Removes or compresses intermediate files.
All operations are performed in the build directory which is
$HgAutomate::clusterData/\$db/$HgAutomate::trackBuild/repeatModeler.\$date unless -buildDir is given.
Run -help to see what files are required for this script.
";
  # Detailed help (-help):
  print STDERR "
Assumptions:
1. $HgAutomate::clusterData/\$db/\$db.unmasked.2bit contains sequence for
   database/assembly \$db.  (This can be overridden with -unmaskedSeq.)
2. When complete, the resulting RepeatMasker library file will be in the build
   directory with the name: asmId-families.fa
" if ($detailed);
  print STDERR "\n";
  exit $status;
}

# Globals:
# Command line args: db
my ($db);
# Other:
my ($buildDir, $chromBased, $updateTable, $secondsStart, $secondsEnd);

sub checkOptions {
  # Make sure command line options are valid/supported.
  my $ok = GetOptions(@HgStepManager::optionSpec,
		      'buildDir=s',
		      'unmaskedSeq=s',
		      @HgAutomate::commonOptionSpec,
		      );
  &usage(1) if (!$ok);
  &usage(0, 1) if ($opt_help);
  &HgAutomate::processCommonOptions();
  my $err = $stepper->processOptions();
  usage(1) if ($err);
  $dbHost = $opt_dbHost if ($opt_dbHost);
  $workhorse = $opt_workhorse if ($opt_workhorse);
  $bigClusterHub = $opt_bigClusterHub if ($opt_bigClusterHub);
}

#########################################################################
# * step: cluster [workhorse]
sub doBlastDb {
  my $runDir = "$buildDir";
  # verify starting with a clean directory, not done before
  if ( ! $opt_debug ) {
    if ( -d "$runDir" ) {
       if ( -s "$runDir/$db.nsq" ) {
         &HgAutomate::verbose(1, "\nblastDb step previously completed\n");
         return;
       }
    }
  }
  &HgAutomate::mustMkdir($runDir);

  if (! -e $unmaskedSeq) {
    die "Error: required file $unmaskedSeq does not exist.";
  }

  my $whatItDoes =
"Construct .fa file from unmasked.2bit, then run BuildDatabase from
RepeatModeler to prepare rmblastn index files.";

  my $bossScript = newBash HgRemoteScript("$runDir/blastDb.bash", $workhorse,
				      $runDir, $whatItDoes);

  $bossScript->add(<<_EOF_
export asmId="${db}"
export unmasked2Bit="${unmaskedSeq}"
export bDatabase="${BuildDatabase}"

if [ "\${unmasked2Bit}" -nt "\${asmId}.fa" ]; then
  twoBitToFa "\${unmasked2Bit}" "\${asmId}.fa"
  touch -r "\${unmasked2Bit}" "\${asmId}.fa"
fi

if [ "\${asmId}.fa" -nt "\${asmId}.nsq" ]; then
  time (\$bDatabase -name "\${asmId}" "\${asmId}.fa") > blastDb.log 2>&1
fi

_EOF_
    );

  $bossScript->execute() if (! $opt_debug);
} # sub doBlastDb

#########################################################################
# * step: cluster [bigClusterHub]
sub doCluster {
  my $runDir = "$buildDir";
  my $paraHub = $bigClusterHub;

  # First, make sure previous step has completed:
  if ( ! $opt_debug ) {
    if ( ! (-s "$runDir/$db.nsq" || -s "$runDir/$db.00.nsq") ) {
      die "doCluster: previous 'blastDb' step has not completed, $db.nsq not present\n";
    }
    # And, verify this step has not run before
    if ( -s "$runDir/run.time" && ! -s "$runDir/${db}-families.fa" ) {
      die "cluster: this step appears to have run before, but is broken, run.time is present but ${db}-families.fa is not present ?";
    }
    if ( -s "$runDir/${db}-families.fa" ) {
         &HgAutomate::verbose(1, "\ncluster step previously completed\n");
         return;
    }
  }

  my $whatItDoes =
"runs single cluster job to perform the RepeatModeler process.";

  my $bossScript = newBash HgRemoteScript("$runDir/doCluster.bash", $paraHub,
				      $runDir, $whatItDoes);
  $bossScript->add(<<_EOF_
printf '#!/bin/bash

set -beEu -o pipefail

unset TMPDIR
if [ -d "/data/tmp" ]; then
  export TMPDIR="/data/tmp"
elif [ -d "/dev/shm" ]; then
  export TMPDIR="/dev/shm"
elif [ -d "/scratch/tmp" ]; then
  export TMPDIR="/scratch/tmp"
else
  export TMPDIR="/tmp"
fi

export tmpDir=`mktemp -d -p \$TMPDIR rModeler.XXXXXX`

# working directory
cd "\${tmpDir}"
rsync --exclude "do.log" -a -P "${runDir}/" "\${tmpDir}/"

export asmId="\${1}"
export threadCount="${threadCount}"
export rModeler="${RepeatModeler}"

time (\$rModeler -engine ncbi \$threadCount -database "\${asmId}") > modeler.log 2>&1
rsync --exclude "do.log" -a -P ./ "${runDir}/"
cd "${runDir}"
rm -fr "\${tmpDir}/"
chmod 775 "${runDir}"
' > oneJob
chmod +x oneJob
printf "oneJob ${db} {check out line+ ${db}-rmod.log}\n" > jobList
para make $parasolOpts jobList
para check
para time > run.time
cat run.time
"${RepeatModeler}" -version > "${runDir}/modelerVersion.txt"

_EOF_
  );
  `touch "$runDir/para_hub_$paraHub"`;
  $bossScript->execute() if (! $opt_debug);
} # doCluster

#########################################################################
# * step: cleanup [workhorse]
sub doCleanup {
  my $runDir = "$buildDir";

  # First, make sure previous step has completed:
  if ( ! $opt_debug ) {
    if ( -s "$runDir/run.time" && ! -s "$runDir/${db}-families.fa" ) {
      die "cleanup: previous 'cluster' step appears to be broken, run.time is present but ${db}-families.fa is not present ?";
    }
    if ( ! -s "$runDir/${db}-families.fa" ) {
      die "cleanup previous 'libResult' step has not completed, ${db}-families.fa not present\n";
    }
    # And, verify this step has not run before
    if ( ! -s "$runDir/${db}.fa" ) {
         &HgAutomate::verbose(1, "\ncleanup step previously completed\n");
         return;
    }
  }
  my $whatItDoes = "Cleans up or compresses intermediate files.";
  my $bossScript = newBash HgRemoteScript("$runDir/modelerCleanup.bash", $workhorse,
				      $runDir, $whatItDoes);
  $bossScript->add(<<_EOF_
export asmId="${db}"

if [ ! -s "\${asmId}-families.fa" ]; then
  printf "cleanup expected result file: \${asmId}-families.fa does not exist\n" 1>&2
  exit 255
fi
rm -fr \${asmId}.fa \${asmId}.n?? ./err/
if [ -s "\${asmId}-families.stk" ]; then
  gzip \${asmId}-families.stk
fi
rm -f ${buildDir}/../../\${asmId}.repeatModeler.families.stk.gz
rm -f ${buildDir}/../../\${asmId}.rmod.log.txt
ln -s trackData/repeatModeler/\${asmId}-families.stk.gz ${buildDir}/../../\${asmId}.repeatModeler.families.stk.gz
ln -s trackData/repeatModeler/\${asmId}-rmod.log ${buildDir}/../../\${asmId}.rmod.log.txt
gzip -c "\${asmId}-families.fa" > "${buildDir}/../../\${asmId}.repeatModeler.families.fa.gz"
touch -r "\${asmId}-families.fa" "${buildDir}/../../\${asmId}.repeatModeler.families.fa.gz"
c=`ls -d RM_* | wc -l`
if [ "\${c}" -eq 1 ]; then
   RM_dir=`ls -d RM_*`
   if [ -d "\${RM_dir}" ]; then
     rm -fr "\${RM_dir}"
   else
     printf "directory RM_* not found ?\\n" 1>&2
     ls -d RM* 1>&2
     exit 255
   fi
else
   printf "single directory RM_* not found ?\\n" 1>&2
   ls -d RM* 1>&2
   exit 255
fi
_EOF_
  );
  $bossScript->execute() if (! $opt_debug);
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

# Now that we know the $db, figure out our paths:
my $date = `date +%Y-%m-%d`;
chomp $date;
$buildDir = $opt_buildDir ? $opt_buildDir :
  "$HgAutomate::clusterData/$db/$HgAutomate::trackBuild/repeatModeler.$date";
$unmaskedSeq = $opt_unmaskedSeq ? $opt_unmaskedSeq :
  "$HgAutomate::clusterData/$db/$db.unmasked.2bit";

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

&HgAutomate::verbose(1, <<_EOF_

 *** All done!$upThrough - Elapsed time: ${elapsedMinutes}m${elapsedSeconds}s
 *** Steps were performed in $buildDir
_EOF_
);
if ($stepper->stepPrecedes('cluster', $stopStep)) {
  &HgAutomate::verbose(1, <<_EOF_
 *** Result library file should be present in\n$buildDir/${db}-families.fa
     to be used by doRepeatMasker.pl -customLib=${db}-families.fa
_EOF_
  );
}
&HgAutomate::verbose(1, "\n");
