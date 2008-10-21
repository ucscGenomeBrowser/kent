#!/usr/bin/env perl

# DO NOT EDIT the /cluster/bin/scripts copy of this file --
# edit ~/kent/src/hg/utils/automation/doRepeatMasker.pl instead.

# $Id: doRepeatMasker.pl,v 1.12 2008/10/21 22:49:57 hiram Exp $

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
my $RepeatMaskerPath = "/scratch/data/RepeatMasker";
my $RepeatMasker = "$RepeatMaskerPath/RepeatMasker";
my $liftRMAlign = "/cluster/bin/scripts/liftRMAlign.pl";

# Option variable names, both common and peculiar to this script:
use vars @HgAutomate::commonOptionVars;
use vars @HgStepManager::optionVars;
use vars qw/
    $opt_buildDir
    $opt_species
    $opt_unmaskedSeq
    $opt_customLib
    /;

# Specify the steps supported with -continue / -stop:
my $stepper = new HgStepManager(
    [ { name => 'cluster', func => \&doCluster },
      { name => 'cat',     func => \&doCat },
      { name => 'mask',    func => \&doMask },
      { name => 'install', func => \&doInstall },
      { name => 'cleanup', func => \&doCleanup },
    ]
				);

# Option defaults:
my $dbHost = 'hgwdev';
my $unmaskedSeq = "\$db.unmasked.2bit";
my $defaultSpecies = 'scientificName from dbDb';

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
                          $HgAutomate::clusterData/\$db/$HgAutomate::trackBuild/RepeatMasker.\$date
                          (necessary when continuing at a later date).
    -species sp           Use sp (which can be quoted genus and species, or
                          a common name that RepeatMasker recognizes.
                          Default: $defaultSpecies.
    -unmaskedSeq seq.2bit Use seq.2bit as the unmasked input sequence instead
                          of default ($unmaskedSeq).
    -customLib lib.fa     Use custom repeat library instead of RepeatMaskers\'s.
_EOF_
  ;
  print STDERR &HgAutomate::getCommonOptionHelp('dbHost' => $dbHost,
						'workhorse' => '',
						'bigClusterHub' => '');
  print STDERR "
Automates UCSC's RepeatMasker process for genome database \$db.  Steps:
    cluster: Do a cluster run of RepeatMasker on 500kb sequence chunks.
    cat:     Concatenate the cluster run results into \$db.fa.out.
    mask:    Create a \$db.2bit masked by \$db.fa.out.
    install: Load \$db.fa.out into the rmsk table (possibly split) in \$db,
             install \$db.2bit in $HgAutomate::clusterData/\$db/, and if \$db is
             chrom-based, split \$db.fa.out into per-chrom files.
    cleanup: Removes or compresses intermediate files.
All operations are performed in the build directory which is
$HgAutomate::clusterData/\$db/$HgAutomate::trackBuild/RepeatMasker.\$date unless -buildDir is given.
Run -help to see what files are required for this script.
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
my ($buildDir, $chromBased);

sub checkOptions {
  # Make sure command line options are valid/supported.
  my $ok = GetOptions(@HgStepManager::optionSpec,
		      'buildDir=s',
		      'species=s',
		      'unmaskedSeq=s',
		      'customLib=s',
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
# * step: cluster [bigClusterHub]
sub doCluster {
  my $runDir = "$buildDir/run.cluster";
  &HgAutomate::mustMkdir($runDir);

  my $paraHub = $opt_bigClusterHub ? $opt_bigClusterHub :
    &HgAutomate::chooseClusterByBandwidth();
  if (! -e $unmaskedSeq) {
    die "Error: required file $unmaskedSeq does not exist.";
  }
  my @okIn = grep !/scratch/,
    &HgAutomate::chooseFilesystemsForCluster($paraHub, "in");
  my @okOut = &HgAutomate::chooseFilesystemsForCluster($paraHub, "out");
  if (scalar(@okOut) > 1) {
    @okOut = grep !/$okIn[0]/, @okOut;
  }
  my $inHive = 0;
  $inHive = 1 if ($okIn[0] =~ m#/hive/data/genomes#);
  my $clusterSeqDir = "$okIn[0]/$db";
  my $clusterSeq = "$clusterSeqDir/$db.unmasked.2bit";
  my $partDir .= "$okOut[0]/$db/RMPart";
  my $species = $opt_species ? $opt_species : &HgAutomate::getSpecies($dbHost, $db);
  my $customLib = $opt_customLib;
  my $repeatLib = "";
  if ($opt_customLib && $opt_species) {
     $repeatLib = "-species \'$species\' -lib $customLib";
  }
  elsif ($opt_customLib) {
     $repeatLib = "-lib $customLib";
  }
  else {
     $repeatLib = "-species \'$species\'";
  }

  # Script to do a dummy run of RepeatMasker, to test our invocation and
  # unpack library files before kicking off a large cluster run.
  #  And now that RM is being run from local /scratch/data/RepeatMasker/
  #  this is also done in the cluster run script so each node will have
  #	its library initialized
  my $fh = &HgAutomate::mustOpen(">$runDir/dummyRun.csh");
  print $fh <<_EOF_
#!/bin/csh -ef

$RepeatMasker $repeatLib /dev/null
_EOF_
  ;
  close($fh);

  # Cluster job script:
  $fh = &HgAutomate::mustOpen(">$runDir/RMRun.csh");
  print $fh <<_EOF_
#!/bin/csh -ef

set finalOut = \$1

set inLst = \$finalOut:r
set inLft = \$inLst:r.lft
set alignOut = \$finalOut:r.align

# Use local disk for output, and move the final result to \$outPsl
# when done, to minimize I/O.
set tmpDir = `mktemp -d -p /scratch/tmp doRepeatMasker.cluster.XXXXXX`
pushd \$tmpDir

# Initialize local library
$RepeatMasker $repeatLib /dev/null

foreach spec (`cat \$inLst`)
  # Remove path and .2bit filename to get just the seq:start-end spec:
  set base = `echo \$spec | sed -r -e 's/^[^:]+://'`
  # If \$spec is the whole sequence, twoBitToFa removes the :start-end part,
  # which causes liftUp to barf later.  So tweak the header back to
  # seq:start-end for liftUp's sake:
  twoBitToFa \$spec stdout \\
  | sed -e "s/^>.*/>\$base/" > \$base.fa
  $RepeatMasker -align -s $repeatLib \$base.fa
end

# Lift up (leave the RepeatMasker header in place because we'll liftUp
# again later):
liftUp -type=.out stdout \$inLft error *.out \\
  > tmpOut__out

$liftRMAlign \$inLft > tmpOut__align

# Move final result into place:
mv tmpOut__out \$finalOut
mv tmpOut__align \$alignOut

popd
rm -rf \$tmpDir
_EOF_
  ;
  close($fh);

  &HgAutomate::makeGsub($runDir,
      "./RMRun.csh {check out line $partDir/\$(path1).out}");

  my $whatItDoes = 
"It computes a logical partition of unmasked 2bit into 500k chunks
and runs it on the cluster with the most available bandwidth.";
  my $bossScript = new HgRemoteScript("$runDir/doCluster.csh", $paraHub,
				      $runDir, $whatItDoes);

  $bossScript->add(<<_EOF_
chmod a+x dummyRun.csh
chmod a+x RMRun.csh
./dummyRun.csh

# Record RM version used:
ls -l $RepeatMaskerPath
grep 'version of RepeatMasker\$' $RepeatMasker
grep RELEASE $RepeatMaskerPath/Libraries/RepeatMaskerLib.embl
_EOF_
  );
  if (! $inHive) {
    $bossScript->add(<<_EOF_
mkdir -p $clusterSeqDir
rsync -av $unmaskedSeq $clusterSeq
_EOF_
    );
  }
  $bossScript->add(<<_EOF_
rm -rf $partDir
$Bin/simplePartition.pl $clusterSeq 500000 $partDir
rm -f $buildDir/RMPart
ln -s $partDir $buildDir/RMPart

$HgAutomate::gensub2 $partDir/partitions.lst single gsub jobList
$HgAutomate::paraRun

_EOF_
  );
  if (! $inHive) {
    $bossScript->add(<<_EOF_
rm -f $clusterSeq
_EOF_
    );
  }
  $bossScript->execute();
} # doCluster

#########################################################################
# * step: cat [fileServer]
sub doCat {
  my $runDir = "$buildDir";
  &HgAutomate::checkExistsUnlessDebug('cluster', 'cat',
				      "$buildDir/run.cluster/run.time");

  my $whatItDoes = 
"It concatenates .out files from cluster run into a single $db.fa.out.\n" .
"liftUp (with no lift specs) is used to concatenate .out files because it\n" .
"uniquifies (per input file) the .out IDs which can then be used to join\n" .
"fragmented repeats in the Nested Repeats track.";
  my $fileServer = &HgAutomate::chooseFileServer($runDir);
  my $bossScript = new HgRemoteScript("$runDir/doCat.csh", $fileServer,
				      $runDir, $whatItDoes);

  # Use symbolic link created in cluster step:
  my $partDir = "$buildDir/RMPart";
  my $indent = "";
  my $path = "";
  my $levels = $opt_debug ? 1 : `cat $partDir/levels`;
  chomp $levels;
  $bossScript->add("# Debug mode -- don't know the real number of levels\n")
    if ($opt_debug);

  # Make the appropriate number of nested foreach's to match the number
  # of levels from the partitioning.  If $levels is 1, no foreach required.
  for (my $l = $levels - 2;  $l >= 0;  $l--) {
    my $dir = ($l == ($levels - 2)) ? $partDir : "\$d" . ($l + 1);
    $bossScript->add(<<_EOF_
${indent}foreach d$l ($dir/???)
_EOF_
    );
    $indent .= '  ';
    $path .= "\$d$l/";
  }
  for (my $l = 0;  $l <= $levels - 2;  $l++) {
    $bossScript->add(<<_EOF_
${indent}  liftUp ${path}cat.out /dev/null carry ${path}???/*.out
${indent}  cat ${path}???/*.align > ${path}cat.align
${indent}end
_EOF_
    );
    $indent =~ s/  //;
    $path =~ s/\$d\d+\/$//;
  }
  $bossScript->add(<<_EOF_

liftUp $db.fa.out /dev/null carry $partDir/???/*.out
cat $partDir/???/*.align >> $db.fa.align
_EOF_
  );
  $bossScript->add(<<_EOF_

# Use the ID column to join up fragments of interrupted repeats for the
# Nested Repeats track.
$Bin/extractNestedRepeats.pl $db.fa.out > $db.nestedRepeats.bed
_EOF_
  );
  $bossScript->execute();
} # doCat


#########################################################################
# * step: mask [workhorse]
sub doMask {
  my $runDir = "$buildDir";
  &HgAutomate::checkExistsUnlessDebug('cat', 'mask', "$buildDir/$db.fa.out");

  my $whatItDoes = "It makes a masked .2bit in this build directory.";
  my $workhorse = &HgAutomate::chooseWorkhorse();
  my $bossScript = new HgRemoteScript("$runDir/doMask.csh", $workhorse,
				      $runDir, $whatItDoes);

  $bossScript->add(<<_EOF_
twoBitMask $unmaskedSeq $db.fa.out $db.rmsk.2bit
twoBitToFa $db.rmsk.2bit stdout | faSize stdin > faSize.rmsk.txt
_EOF_
  );
  $bossScript->execute();
} # doMask


#########################################################################
# * step: install [dbHost, maybe fileServer]
sub doInstall {
  my $runDir = "$buildDir";
  &HgAutomate::checkExistsUnlessDebug('cat', 'install', "$buildDir/$db.fa.out");

  my $split = $chromBased ? " (split)" : "";
  my $whatItDoes =
"It loads $db.fa.out into the$split rmsk table and $db.nestedRepeats.bed\n" .
"into the nestedRepeats table.  It also installs the masked 2bit.";
  my $bossScript = new HgRemoteScript("$runDir/doLoad.csh", $dbHost,
				      $runDir, $whatItDoes);

  $split = $chromBased ? "-split" : "-nosplit";
  my $installDir = "$HgAutomate::clusterData/$db";
  $bossScript->add(<<_EOF_
hgLoadOut -table=rmsk $split $db $db.fa.out
hgLoadBed $db nestedRepeats $db.nestedRepeats.bed \\
  -sqlTable=\$HOME/kent/src/hg/lib/nestedRepeats.sql

rm -f $installDir/$db.rmsk.2bit
ln -s $buildDir/$db.rmsk.2bit $installDir/$db.rmsk.2bit
_EOF_
  );
  $bossScript->execute();

  # Make a new script for the fileServer if chrom-based:
  if ($chromBased) {
    my $fileServer = &HgAutomate::chooseFileServer($runDir);
    $whatItDoes =
"It splits $db.fa.out into per-chromosome files in chromosome directories\n" .
"where makeDownload.pl will expect to find them.\n";
    my $bossScript = new HgRemoteScript("$runDir/doSplit.csh", $fileServer,
					$runDir, $whatItDoes);
    $bossScript->add(<<_EOF_
head -3 $db.fa.out > /tmp/rmskHead.txt
tail +4 $db.fa.out \\
| splitFileByColumn -col=5 stdin /cluster/data/$db -chromDirs \\
    -ending=.fa.out -head=/tmp/rmskHead.txt
_EOF_
    );
    $bossScript->execute();
  }
} # doInstall


#########################################################################
# * step: cleanup [fileServer]
sub doCleanup {
  my $runDir = "$buildDir";
  my $whatItDoes = "It cleans up or compresses intermediate files.";
  my $fileServer = &HgAutomate::chooseFileServer($runDir);
  my $bossScript = new HgRemoteScript("$runDir/doCleanup.csh", $fileServer,
				      $runDir, $whatItDoes);
  $bossScript->add(<<_EOF_
if (-e RMPart/000) then
  rm -rf RMPart/???
endif
if (-e $db.fa.align) then
  gzip $db.fa.align
endif
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
($db) = @ARGV;

# Now that we know the $db, figure out our paths:
my $date = `date +%Y-%m-%d`;
chomp $date;
$buildDir = $opt_buildDir ? $opt_buildDir :
  "$HgAutomate::clusterData/$db/$HgAutomate::trackBuild/RepeatMasker.$date";
$unmaskedSeq = $opt_unmaskedSeq ? $opt_unmaskedSeq :
  "$HgAutomate::clusterData/$db/$db.unmasked.2bit";
my $seqCount = `twoBitInfo $unmaskedSeq stdout | wc -l`;
$chromBased = ($seqCount <= $HgAutomate::splitThreshold);

# Do everything.
$stepper->execute();

# Tell the user anything they should know.
my $stopStep = $stepper->getStopStep();
my $upThrough = ($stopStep eq 'cleanup') ? "" :
  "  (through the '$stopStep' step)";

&HgAutomate::verbose(1, <<_EOF_

 *** All done!$upThrough
 *** Steps were performed in $buildDir
_EOF_
);
if ($stepper->stepPrecedes('mask', $stopStep)) {
  &HgAutomate::verbose(1, <<_EOF_
 *** Please check the log file to see if hgLoadOut had warnings that we
     should pass on to Arian and Robert.
 *** Please also spot-check some repeats in the browser, run twoBitToFa on
     a portion of $db.2bit to make sure there's some masking, etc.

_EOF_
  );
}
&HgAutomate::verbose(1, "\n");
