#!/usr/bin/env perl

# DO NOT EDIT the /cluster/bin/scripts copy of this file --
# edit ~/kent/src/hg/utils/automation/doGenscan.pl instead.

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
    /;

# Specify the steps supported with -continue / -stop:
my $stepper = new HgStepManager(
    [ { name => 'hardMask',   func => \&doHardMask },
      { name => 'genscan', func => \&doGenscan },
      { name => 'makeBed', func => \&doMakeBed },
      { name => 'load', func => \&doLoadGenscan },
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
                          $HgAutomate::clusterData/\$db/$HgAutomate::trackBuild/genscan
                          (necessary when continuing at a later date).
    -maskedSeq seq.2bit   Use seq.2bit as the masked input sequence instead
                          of default ($maskedSeq).
_EOF_
  ;
  print STDERR &HgAutomate::getCommonOptionHelp('dbHost' => $dbHost,
                                'bigClusterHub' => $bigClusterHub,
                                'workhorse' => $defaultWorkhorse);
  print STDERR "
Automates UCSC's Genscan track construction for the database \$db.  Steps:
    hardMask:  Creates hard-masked fastas needed for the CpG Island program.
    genscan:   Run gsBig on the hard-masked fastas
    makeBed:   Transform output from gsBig into genscan.gtf genscan.pep and genscanSubopt.bed
    load:      Load genscan.gtf and genscanSubopt.bed into \$db.
    cleanup:   Removes hard-masked fastas and output from gsBig.
All operations are performed in the build directory which is
$HgAutomate::clusterData/\$db/$HgAutomate::trackBuild/genscan unless -buildDir is given.
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
# * step: hard mask [bigClusterHub]
sub doHardMask {
  # Set up and perform the cluster run to run the hardMask sequence.
  my $paraHub = $bigClusterHub;
  my $runDir = "$buildDir/run.hardMask";
  my $outRoot = '../hardMaskedFa';

  # First, make sure we're starting clean.
  if (-e "$buildDir/genscan.gtf.gz") {
    die "doHardMask: looks like this entire business was run already,\n" .
      "(genscan.gtf.gz exists)\n" .
	"    please investigate the directory: $buildDir\n";
  } elsif (-e "$runDir/run.time") {
    die "doHardMask: looks like this step was run successfully already " .
      "(run.hardMask/run.time exists).  Either run with -continue genscan or some later " .
	"stage, or move aside/remove $runDir/ and run again.\n";
  } elsif ((-e "$runDir/gsub" || -e "$runDir/jobList") && ! $opt_debug) {
    die "doHardMask: looks like we are not starting with a clean " .
      "slate.\n\tPlease move aside or remove\n  $runDir/\n\tand run again.\n";
  }
  &HgAutomate::mustMkdir($runDir);
  my $templateCmd = ("./runOne.csh " . '$(root1) '
                . "{check out exists+ $outRoot/" . '$(lastDir1)/$(file1)}');
  &HgAutomate::makeGsub($runDir, $templateCmd);
 `touch "$runDir/para_hub_$paraHub"`;

  my $fh = &HgAutomate::mustOpen(">$runDir/runOne.csh");
  print $fh <<_EOF_
#!/bin/csh -ef
set chrom = \$1
set result = \$2
twoBitToFa $maskedSeq:\$chrom stdout \\
  | maskOutFa stdin hard \$result
_EOF_
  ;
  close($fh);

  my $whatItDoes = "Make hard-masked fastas for each chrom.";
  my $bossScript = new HgRemoteScript("$runDir/doHardMask.csh", $paraHub,
				      $runDir, $whatItDoes);

  my $paraRun = &HgAutomate::paraRun();
  my $gensub2 = &HgAutomate::gensub2();
  $bossScript->add(<<_EOF_
mkdir -p $outRoot
chmod a+x runOne.csh
set perDirLimit = 4000
set ctgCount = `twoBitInfo $maskedSeq stdout | wc -l`
set subDirCount = `echo \$ctgCount | awk '{printf "%d", 1+\$1/4000}'`
@ dirCount = 0
set dirName = `echo \$dirCount | awk '{printf "%03d", \$1}'`
@ perDirCount = 0
mkdir $outRoot/\$dirName
/bin/rm -f chr.list
/bin/touch chr.list
foreach chrom ( `twoBitInfo $maskedSeq stdout | cut -f1` )
  if (\$perDirCount < \$perDirLimit) then
    @ perDirCount += 1
  else
    @ dirCount += 1
    set dirName = `echo \$dirCount | awk '{printf "%03d", \$1}'`
    set perDirCount = 1
    mkdir $outRoot/\$dirName
  endif
  echo \$dirName/\$chrom.fa >> chr.list
end
$gensub2 chr.list single gsub jobList
$paraRun
_EOF_
  );
  $bossScript->execute();
} # doHardMask

#########################################################################
# * step: genscan [bigClusterHub]
sub doGenscan {
  # Set up and perform the cluster run to run the CpG function on the
  #     hard masked sequence.
  my $paraHub = $bigClusterHub;
  my $runDir = $buildDir;
  # First, make sure we're starting clean.
  if (! -e "$buildDir/run.hardMask/run.time") {
    die "doGenscan: the previous step hardMask did not complete \n" .
      "successfully (run.hardMask/run.time does not exists).\nPlease " .
      "complete the previous step: -continue=-hardMask\n";
  } elsif (-e "$runDir/run.time") {
    die "doGenscan: looks like this step was run successfully already " .
      "(run.time exists).\nEither run with -continue makeBed or some later " .
	"stage,\nor move aside/remove $runDir/ and run again.\n";
  } elsif ((-e "$runDir/gsub" || -e "$runDir/jobList") && ! $opt_debug) {
    die "doGenscan: looks like we are not starting with a clean " .
      "slate.\n\tclean\n  $runDir/\n\tand run again.\n";
  }
  &HgAutomate::mustMkdir($runDir);

  my $templateCmd = ("./runGsBig.csh " . '$(root1) $(lastDir1) '
        . '{check out exists gtf/$(lastDir1)/$(root1).gtf} '
        . '{check out exists pep/$(lastDir1)/$(root1).pep} '
        . '{check out exists subopt/$(lastDir1)/$(root1).bed}');
  &HgAutomate::makeGsub($runDir, $templateCmd);
 `touch "$runDir/para_hub_$paraHub"`;

  my $fh = &HgAutomate::mustOpen(">$runDir/runGsBig.csh");
  print $fh <<_EOF_
#!/bin/csh -ef
set chrom = \$1
set dir = \$2
set resultGtf = \$3
set resultPep = \$4
set resultSubopt = \$5
mkdir -p gtf/\$dir pep/\$dir subopt/\$dir
set seqFile = hardMaskedFa/\$dir/\$chrom.fa
/cluster/bin/x86_64/gsBig \$seqFile \$resultGtf -trans=\$resultPep -subopt=\$resultSubopt -exe=/hive/data/staging/data/genscan/genscan -par=/hive/data/staging/data/genscan/HumanIso.smat -tmp=/tmp -window=2400000
_EOF_
  ;
  close($fh);

  my $whatItDoes = "Run gsBig on masked sequence.";
  my $bossScript = new HgRemoteScript("$runDir/doGenscan.csh", $paraHub,
				      $runDir, $whatItDoes);
  my $paraRun = &HgAutomate::paraRun();
  my $gensub2 = &HgAutomate::gensub2();
  $bossScript->add(<<_EOF_
chmod a+x runGsBig.csh
rm -f file.list
find ./hardMaskedFa -type f > file.list
$gensub2 file.list single gsub jobList
$paraRun
_EOF_
  );
  $bossScript->execute();
} # doGenscan

#########################################################################
# * step: make bed [workhorse]
sub doMakeBed {
  my $runDir = $buildDir;
  &HgAutomate::mustMkdir($runDir);

  # First, make sure we're starting clean.
  if (! -e "$runDir/run.time") {
    die "doMakeBed: the previous step genscan did not complete \n" .
      "successfully ($buildDir/run.time does not exist).\nPlease " .
      "complete the previous step: -continue=-genscan\n";
  } elsif (-e "$runDir/genscan.gtf" || -e "$runDir/genscan.gtf.gz" ) {
    die "doMakeBed: looks like this was run successfully already\n" .
      "(genscan.gtf exists).  Either run with -continue load or cleanup\n" .
	"or move aside/remove $runDir/genscan.gtf\nand run again.\n";
  }

  my $whatItDoes = "Makes gtf/pep/bed files from gsBig output.";
  my $bossScript = newBash HgRemoteScript("$runDir/doMakeBed.bash", $workhorse,
				      $runDir, $whatItDoes);

  $bossScript->add(<<_EOF_
export db=$db
catDir -r gtf > genscan.gtf
catDir -r pep > genscan.pep
catDir -r subopt | sort -k1,1 -k2,2n > genscanSubopt.bed
gtfToGenePred genscan.gtf \$db.genscan.gp
genePredToBed \$db.genscan.gp stdout | sort -k1,1 -k2,2n > \$db.genscan.bed
_EOF_
  );
  $bossScript->execute();
} # doMakeBed

#########################################################################
# * step: load [dbHost]
sub doLoadGenscan {
  my $runDir = $buildDir;
  &HgAutomate::mustMkdir($runDir);

  if (! -e "$runDir/genscan.gtf") {
    die "doLoadGenscan: the previous step makeBed did not complete \n" .
      "successfully (genscan.gtf does not exists).\nPlease " .
      "complete the previous step: -continue=-makeBed\n";
  } elsif (-e "$runDir/fb.$db.genscan.txt" ) {
    die "doLoadGenscan: looks like this was run successfully already\n" .
      "(fb.$db.genscan.txt exists).  Either run with -continue cleanup\n" .
	"or move aside/remove\n$runDir/fb.$db.genscan.txt and run again.\n";
  }
  my $whatItDoes = "Loads genscan.gtf and genscanSubopt.bed.";
  my $bossScript = new HgRemoteScript("$runDir/doLoadGenscan.csh", $dbHost,
				      $runDir, $whatItDoes);

  $bossScript->add(<<_EOF_
bedToBigBed $db.genscan.bed ../../chrom.sizes $db.genscan.bb
bedToBigBed genscanSubopt.bed ../../chrom.sizes $db.genscanSubopt.bb
gtfToGenePred genscan.gtf genscan.gp
genePredCheck -db=$db genscan.gp
ldHgGene -gtf $db genscan genscan.gtf
hgLoadBed $db genscanSubopt genscanSubopt.bed
featureBits $db genscan >&fb.$db.genscan.txt
featureBits $db genscanSubopt >&fb.$db.genscanSubopt.txt
checkTableCoords -verboseBlocks -table=genscan $db
checkTableCoords -verboseBlocks -table=genscanSubopt $db
_EOF_
  );
  $bossScript->execute();
} # doLoad

#########################################################################
# * step: cleanup [fileServer]
sub doCleanup {
  my $runDir = $buildDir;

  if (-e "$runDir/genscan.gtf.gz" ) {
    die "doCleanup: looks like this was run successfully already\n" .
      "(genscan.gtf.gz exists).  Investigate the run directory:\n" .
	" $runDir/\n";
  }
  my $whatItDoes = "It cleans up or compresses intermediate files.";
  my $fileServer = &HgAutomate::chooseFileServer($runDir);
  my $bossScript = new HgRemoteScript("$runDir/doCleanup.csh", $fileServer,
				      $runDir, $whatItDoes);
  $bossScript->add(<<_EOF_
rm -rf hardMaskedFa/ err/ run.hardMask/err/
rm -f batch.bak bed.tab run.hardMask/batch.bak $db.genscan.gp $db.genscan.bed
gzip genscanSubopt.bed genscan.gtf genscan.gp genscan.pep
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
  "$HgAutomate::clusterData/$db/$HgAutomate::trackBuild/genscan";
$maskedSeq = $opt_maskedSeq ? $opt_maskedSeq :
  "$HgAutomate::clusterData/$db/$db.2bit";

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

