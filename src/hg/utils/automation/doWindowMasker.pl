#!/usr/bin/env perl

# DO NOT EDIT the /cluster/bin/scripts copy of this file --
# edit ~/kent/src/hg/utils/automation/doWindowMasker.pl instead.

# $Id: doWindowMasker.pl,v 1.8 2009/03/13 22:27:12 hiram Exp $

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
    [ { name => 'count',   func => \&doCount },
      { name => 'mask', func => \&doMask },
      { name => 'sdust', func => \&doSdust },
      { name => 'twobit', func => \&doTwoBit },
      { name => 'load', func => \&doLoad },
      { name => 'cleanup', func => \&doCleanup },
    ]
				);

# Option defaults:
my $defaultWorkhorse = 'least loaded';
my $dbHost = 'hgwdev';
my $unmaskedSeq = "$HgAutomate::clusterData/\$db/\$db.unmasked.2bit";

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
                          $HgAutomate::clusterData/\$db/$HgAutomate::trackBuild/WindowMasker.\$date
                          (necessary when continuing at a later date).
    -unmaskedSeq <file.2bit>  Use file.2bit for unmasked sequence, default is:
                          $unmaskedSeq
_EOF_
  ;
  print STDERR &HgAutomate::getCommonOptionHelp('dbHost' => $dbHost,
					'workhorse' => $defaultWorkhorse);
  print STDERR "
Automates UCSC's WindowMasker process for genome database \$db.  Steps:
    count: Do the first pass of WindowMasker: collecting the counts.
    mask: The second pass of WindowMasker and collect output.
    sdust: Another pass of WindowMasker using -sdust true.
    twobit: Make masked twobit files.
    load: load and clean of gaps, reload cleaned table.
    cleanup: Removes or compresses intermediate files.
All operations are performed in the build directory which is
$HgAutomate::clusterData/\$db/$HgAutomate::trackBuild/WindowMasker.\$date unless -buildDir is given.
";
  # Detailed help (-help):
  print STDERR "
Assumptions:
1. $HgAutomate::clusterData/\$db/\$db.unmasked.2bit contains sequence for
   database/assembly \$db.  (This can be overridden with -unmaskedSeq.)
" if ($detailed);
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
  &usage(1) if (!$ok);
  &usage(0, 1) if ($opt_help);
  &HgAutomate::processCommonOptions();
  my $err = $stepper->processOptions();
  usage(1) if ($err);
  $dbHost = $opt_dbHost if ($opt_dbHost);
}


#########################################################################
# * step: count [workhorse]
sub doCount {
  my $runDir = "$buildDir";
  &HgAutomate::checkCleanSlate('count', 'mask', "$runDir/windowmasker.counts");
  &HgAutomate::mustMkdir($runDir);
  
  my $whatItDoes = "It does WindowMasker counts step.";
  my $workhorse = &HgAutomate::chooseWorkhorse();
  my $bossScript = new HgRemoteScript("$runDir/doCount.csh", $workhorse,
				      $runDir, $whatItDoes);

  $bossScript->add(<<_EOF_
set windowMaskerDir = /cluster/bin/\$MACHTYPE 
set windowMasker = \$windowMaskerDir/windowmasker
set fa = $db.fa
set tmpDir = `mktemp -d -p /scratch/tmp doWindowMasker.XXXXXX`
chmod 775 \$tmpDir
set inputTwoBit = $unmaskedSeq
pushd \$tmpDir
twoBitToFa \$inputTwoBit \$fa
\$windowMasker -mk_counts true -input \$fa -output windowmasker.counts
popd 
cp \$tmpDir/windowmasker.counts .
rm -rf \$tmpDir
_EOF_
  );
  $bossScript->execute();
} # doCount


#########################################################################
# * step: mask [workhorse]
sub doMask {
  my $runDir = "$buildDir";
  &HgAutomate::checkExistsUnlessDebug('count', 'mask', "$runDir/windowmasker.counts");
  my $whatItDoes = "It does WindowMasker masking step.";
  my $workhorse = &HgAutomate::chooseWorkhorse();
  my $bossScript = new HgRemoteScript("$runDir/doMask.csh", $workhorse,
				      $runDir, $whatItDoes);
  $bossScript->add(<<_EOF_
set windowMaskerDir = /cluster/bin/\$MACHTYPE 
set windowMasker = \$windowMaskerDir/windowmasker
set fa = $db.fa
set tmpDir = `mktemp -d -p /scratch/tmp doWindowMasker.XXXXXX`
chmod 775 \$tmpDir
set inputTwoBit = $unmaskedSeq
cp windowmasker.counts \$tmpDir
pushd \$tmpDir
twoBitToFa \$inputTwoBit \$fa
\$windowMasker -ustat windowmasker.counts -input \$fa -output windowmasker.intervals
perl -wpe \'if \(s\/^\>lcl\\\|\(\.\*\)\\n\$\/\/\) { \$chr = \$1\; } \\
   if \(\/^\(\\d+\) \- \(\\d+\)\/\) { \\
   \$s=\$1\; \$e=\$2+1\; s\/\(\\d+\) \- \(\\d+\)\/\$chr\\t\$s\\t\$e\/\; \\
   }\' windowmasker.intervals > windowmasker.bed
popd 
cp \$tmpDir/windowmasker.bed .
rm -rf \$tmpDir
_EOF_
  );

  $bossScript->execute();
} # doMask

#########################################################################
# * step: sdust [workhorse]
sub doSdust {
  my $runDir = "$buildDir";
  &HgAutomate::checkExistsUnlessDebug('mask', 'sdust', "$runDir/windowmasker.counts");
  my $whatItDoes = "It does WindowMasker masking step with -sdust true.";
  my $workhorse = &HgAutomate::chooseWorkhorse();
  my $bossScript = new HgRemoteScript("$runDir/doSdust.csh", $workhorse,
				      $runDir, $whatItDoes);
  $bossScript->add(<<_EOF_
set windowMaskerDir = /cluster/bin/\$MACHTYPE 
set windowMasker = \$windowMaskerDir/windowmasker
set fa = $db.fa
set tmpDir = `mktemp -d -p /scratch/tmp doWindowMasker.XXXXXX`
chmod 775 \$tmpDir
set inputTwoBit = $unmaskedSeq
cp windowmasker.counts \$tmpDir
pushd \$tmpDir
twoBitToFa \$inputTwoBit \$fa
\$windowMasker -ustat windowmasker.counts -sdust true -input \$fa -output windowmasker.intervals
perl -wpe \'if \(s\/^\>lcl\\\|\(\.\*\)\\n\$\/\/\) { \$chr = \$1\; } \\
   if \(\/^\(\\d+\) \- \(\\d+\)\/\) { \\
   \$s=\$1\; \$e=\$2+1\; s\/\(\\d+\) \- \(\\d+\)\/\$chr\\t\$s\\t\$e\/\; \\
   }\' windowmasker.intervals > windowmasker.sdust.bed
popd 
cp \$tmpDir/windowmasker.sdust.bed .
rm -rf \$tmpDir
_EOF_
  );

  $bossScript->execute();
} # doSdust


#########################################################################
# * step: twobit [fileServer]
sub doTwoBit {
  my $runDir = "$buildDir";
  my $whatItDoes = "Make .2bit files from the beds.";
  &HgAutomate::checkExistsUnlessDebug('sdust', 'twobit', ("$runDir/windowmasker.counts", 
           "$runDir/windowmasker.bed", "$runDir/windowmasker.sdust.bed"));
  my $fileServer = &HgAutomate::chooseFileServer($runDir);
  my $bossScript = new HgRemoteScript("$runDir/doTwoBit.csh", $fileServer,
				      $runDir, $whatItDoes);
  $bossScript->add(<<_EOF_
twoBitMask $unmaskedSeq windowmasker.bed $db.wmsk.2bit
twoBitMask $unmaskedSeq windowmasker.sdust.bed $db.wmsk.sdust.2bit
twoBitToFa $db.wmsk.2bit stdout | faSize stdin >&faSize.$db.wmsk.txt
twoBitToFa $db.wmsk.sdust.2bit stdout | faSize stdin >&faSize.$db.wmsk.sdust.txt
_EOF_
  );
  $bossScript->execute();
} #doTwoBit

#########################################################################
# * step: load [dbHost]
sub doLoad {
  my $runDir = "$buildDir";
  my $whatItDoes = "load sdust.bed and filter with gaps to clean";
  &HgAutomate::checkExistsUnlessDebug('twobit', 'load', ("$runDir/$db.wmsk.sdust.2bit", 
           "$runDir/$db.wmsk.2bit", "$runDir/faSize.$db.wmsk.sdust.txt"));
  my $bossScript = new HgRemoteScript("$runDir/doLoad.csh", $dbHost,
				      $runDir, $whatItDoes);
  $bossScript->add(<<_EOF_
hgLoadBed $db windowmaskerSdust windowmasker.sdust.bed
featureBits -countGaps $db windowmaskerSdust >&fb.$db.windowmaskerSdust.beforeClean.txt
featureBits $db -not gap -bed=notGap.bed
featureBits $db windowmaskerSdust notGap.bed -bed=stdout | gzip -c > cleanWMask.bed.gz
hgLoadBed $db windowmaskerSdust cleanWMask.bed.gz
featureBits -countGaps $db windowmaskerSdust >&fb.$db.windowmaskerSdust.clean.txt
zcat cleanWMask.bed.gz | twoBitMask ../../$db.unmasked.2bit stdin -type=.bed $db.cleanWMSdust.2bit
twoBitToFa $db.cleanWMSdust.2bit stdout | faSize stdin >& faSize.$db.cleanWMSdust.txt
featureBits -countGaps $db rmsk windowmaskerSdust >&fb.$db.rmsk.windowmaskerSdust.txt
_EOF_
  );
  $bossScript->execute();
} #doLoad

#########################################################################
# * step: cleanup [fileServer]
sub doCleanup {
  my $runDir = "$buildDir";
  my $whatItDoes = "It cleans up or compresses intermediate files.";
  my $fileServer = &HgAutomate::chooseFileServer($runDir);
  my $bossScript = new HgRemoteScript("$runDir/doCleanup.csh", $fileServer,
				      $runDir, $whatItDoes);
  $bossScript->add(<<_EOF_
gzip $runDir/windowmasker.counts
gzip $runDir/windowmasker.bed
gzip $runDir/windowmasker.sdust.bed
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
my $secondsStart = `date "+%s"`;
chomp $secondsStart;
($db) = @ARGV;

# Force debug and verbose until this is looking pretty solid:
#$opt_debug = 1;
$opt_verbose = 3 if ($opt_verbose < 3);

# Establish what directory we will work in.
my $date = `date +%Y-%m-%d`;
chomp $date;
$buildDir = $opt_buildDir ? $opt_buildDir :
  "$HgAutomate::clusterData/$db/$HgAutomate::trackBuild/WindowMasker.$date";
$unmaskedSeq = $opt_unmaskedSeq ? $opt_unmaskedSeq :
  "$HgAutomate::clusterData/$db/$db.unmasked.2bit";

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

&HgAutomate::verbose(1,
	"\n *** All done !$upThrough - Elapsed time: ${elapsedMinutes}m${elapsedSeconds}s\n");
&HgAutomate::verbose(1,
	" *** Steps were performed in $buildDir\n");
&HgAutomate::verbose(1, "\n");

