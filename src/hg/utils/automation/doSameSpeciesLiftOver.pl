#!/usr/bin/env perl

# DO NOT EDIT the /cluster/bin/scripts copy of this file -- 
# edit ~/kent/src/hg/utils/automation/doSameSpeciesLiftOver.pl instead.

# $Id: doSameSpeciesLiftOver.pl,v 1.5 2008/09/19 04:38:08 angie Exp $

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
    $opt_ooc
    /;

# Specify the steps supported with -continue / -stop:
my $stepper = new HgStepManager(
    [ { name => 'align',   func => \&doAlign },
      { name => 'chain',   func => \&doChain },
      { name => 'net',     func => \&doNet },
      { name => 'load',    func => \&doLoad },
      { name => 'cleanup', func => \&doCleanup },
    ]
				);

# Option defaults:
my $dbHost = 'hgwdev';

# This could be made into an option:
my $splitSize = '10k';

my $base = $0;
$base =~ s/^(.*\/)?//;

sub usage {
  # Usage / help / self-documentation:
  my ($status, $detailed) = @_;
  # Basic help (for incorrect usage):
  print STDERR "
usage: $base fromDb toDb
options:
";
  print STDERR $stepper->getOptionHelp();
  print STDERR <<_EOF_
    -buildDir dir         Use dir instead of default
                          $HgAutomate::clusterData/\$fromDb/$HgAutomate::trackBuild/blat.\$toDb.\$date
                          (necessary when continuing at a later date).
    -ooc /path/11.ooc     Use this instead of the default
                          /hive/data/genomes/fromDb/11.ooc
                          Can be "none".
_EOF_
  ;
  print STDERR &HgAutomate::getCommonOptionHelp('dbHost' => $dbHost,
						'workhorse' => '',
						'bigClusterHub' => '');
  print STDERR "
Automates UCSC's same-species liftOver (blat/chain/net) pipeline, based on
Kate's suite of makeLo-* scripts:
    align: Aligns the assemblies using blat -fastMap on a big cluster.
    chain: Chains the alignments on a big cluster.
    net:   Nets the alignments, uses netChainSubset to extract liftOver chains.
    load:  Installs liftOver chain files, calls hgAddLiftOverChain on $dbHost.
    cleanup: Removes or compresses intermediate files.
All operations are performed in the build directory which is
$HgAutomate::clusterData/\$fromDb/$HgAutomate::trackBuild/blat.\$toDb.\$date unless -buildDir is given.
";
  # Detailed help (-help):
  print STDERR "
Assumptions:
1. $HgAutomate::clusterData/\$db/\$db.2bit contains RepeatMasked sequence for
   database/assembly \$db.
2. $HgAutomate::clusterData/\$db/chrom.sizes contains all sequence names and sizes from
   \$db.2bit.
3. The \$db.2bit files have already been distributed to cluster-scratch
   (/scratch/hg or /iscratch, /san etc).
" if ($detailed);
  print "\n";
  exit $status;
}


# Globals:
# Command line args: tDb=fromDb, qDb=toDb
my ($tDb, $qDb);
# Other:
my ($buildDir);
my ($tSeq, $tSizes, $qSeq, $qSizes, $QDb);
my ($liftOverChainDir, $liftOverChainFile, $liftOverChainPath);

sub checkOptions {
  # Make sure command line options are valid/supported.
  my $ok = GetOptions(@HgStepManager::optionSpec,
		      'buildDir=s',
		      'ooc=s',
		      @HgAutomate::commonOptionSpec,
		      );
  &usage(1) if (!$ok);
  &usage(0, 1) if ($opt_help);
  &HgAutomate::processCommonOptions();
  my $err = $stepper->processOptions();
  usage(1) if ($err);
  $dbHost = $opt_dbHost if ($opt_dbHost);
}


sub getClusterSeqs {
  # Choose cluster and look for already-installed 2bit files on appropriate
  # cluster-scratch storage.  Exit with an error message if we can't find them.
  my $paraHub = $opt_bigClusterHub ? $opt_bigClusterHub :
    &HgAutomate::chooseClusterByBandwidth();
  my @okFilesystems =
    &HgAutomate::chooseFilesystemsForCluster($paraHub, 'in');
  my ($tSeqScratch, $qSeqScratch);
  foreach my $fs (@okFilesystems) {
    if (&HgAutomate::machineHasFile($paraHub, "$fs/$tDb/$tDb.2bit")) {
      $tSeqScratch = "$fs/$tDb/$tDb.2bit";
      last;
    }
  }
  if (! defined $tSeqScratch) {
    die "align: can't find $tDb/$tDb.2bit in " .
      join("/, ", @okFilesystems) . "/ -- please distribute.\n";
  }
  foreach my $fs (@okFilesystems) {
    if (&HgAutomate::machineHasFile($paraHub, "$fs/$qDb/$qDb.2bit")) {
      $qSeqScratch = "$fs/$qDb/$qDb.2bit";
      last;
    }
  }
  if (! defined $qSeqScratch) {
    die "align: can't find $qDb/$qDb.2bit in " .
      join("/, ", @okFilesystems) . "/ -- please distribute.\n";
  }
  &HgAutomate::verbose(1, "Using $paraHub, $tSeqScratch and $qSeqScratch\n");
  return ($paraHub, $tSeqScratch, $qSeqScratch);
} # getClusterSeqs



#########################################################################
# * step: align [bigClusterHub]
sub doAlign {
  my $runDir = "$buildDir/run.blat";
  &HgAutomate::mustMkdir($runDir);

  my $ooc = "/hive/data/genomes/$tDb/11.ooc";
  if ($opt_ooc) {
    if ($opt_ooc eq 'none') {
      $ooc = "";
    } else {
      $ooc = "$opt_ooc";
    }
  }
  my $dashOoc = "-ooc=$ooc";
  my $pslDir = "$runDir/psl";

  &HgAutomate::checkCleanSlate('align', 'chain', $pslDir, 'run.time');

  my ($paraHub, $tSeqScratch, $qSeqScratch) = &getClusterSeqs();
  if (! &HgAutomate::machineHasFile($paraHub, $ooc)) {
    die "align: $paraHub does not have $ooc -- if that is not the correct " .
      "location, please run again with -ooc.\n";
  }

  # script for a single job: split query partition further into
  # $splitSize bites while building a .lft liftUp spec; blat; lift.
  my $size = $splitSize;
  $size =~ s/k$/000/;
  my $fh = &HgAutomate::mustOpen(">$runDir/job.csh");
  print $fh <<_EOF_
#!/bin/csh -ef

set targetList = \$1
set queryListIn = \$2
set outPsl = \$3

if (\$targetList:e == "lst") set targetList = $runDir/\$targetList
if (\$queryListIn:e == "lst") set queryListIn = $runDir/\$queryListIn

# Use local disk for output, and move the final result to \$outPsl
# when done, to minimize I/O.
set tmpDir = `mktemp -d -p /scratch/tmp doSame.blat.XXXXXX`
pushd \$tmpDir

# We might get a .lst or a 2bit spec here -- convert to (list of) 2bit spec:
if (\$queryListIn:e == "lst") then
  set specList = `cat \$queryListIn`
else
  set specList = \$queryListIn
endif

# Further partition the query spec(s) into $splitSize coord ranges, building up
# a .lst of 2bit specs for blat and a .lft liftUp spec for the results:
cp /dev/null reSplitQuery.lst
cp /dev/null query.lft
foreach spec (\$specList)
  set file  = `echo \$spec | awk -F: '{print \$1;}'`
  set seq   = `echo \$spec | awk -F: '{print \$2;}'`
  set range = `echo \$spec | awk -F: '{print \$3;}'`
  set start = `echo \$range | awk -F- '{print \$1;}'`
  set end   = `echo \$range | awk -F- '{print \$2;}'`
  if (! -e q.sizes) twoBitInfo \$file q.sizes
  set seqSize = `awk '\$1 == "'\$seq'" {print \$2;}' q.sizes`
  while (\$start < \$end)
    set chunkEnd = `expr \$start + $size`
    if (\$chunkEnd > \$end) set chunkEnd = \$end
    set chunkSize = `expr \$chunkEnd - \$start`
    echo \$file\\:\$seq\\:\$start-\$chunkEnd >> reSplitQuery.lst
    if ((\$start == 0) && (\$chunkEnd == \$seqSize)) then
      echo "\$start	\$seq	\$seqSize	\$seq	\$seqSize" >> query.lft
    else
      echo "\$start	\$seq"":\$start-\$chunkEnd	\$chunkSize	\$seq	\$seqSize" >> query.lft
    endif
    set start = \$chunkEnd
  end
end

# Align unsplit target sequence(s) to .lst of 2bit specs for $splitSize chunks
# of query:
blat \$targetList reSplitQuery.lst tmpUnlifted.psl \\
  -tileSize=11 $dashOoc -minScore=100 -minIdentity=98 -fastMap -noHead

# Lift query coords back up:
liftUp -pslQ -nohead tmpOut.psl query.lft warn tmpUnlifted.psl

# Move final result into place:
mv tmpOut.psl \$outPsl

popd
rm -rf \$tmpDir
_EOF_
  ;
  close($fh);
  &HgAutomate::run("chmod a+x $runDir/job.csh");

  &HgAutomate::makeGsub($runDir,
			'job.csh $(path1) $(path2) {check out line ' .
			 $pslDir . '/$(file1)/$(file2).psl}');

  my $whatItDoes = "It performs a cluster run of blat -fastMap.";
  my $bossScript = new HgRemoteScript("$runDir/doAlign.csh", $paraHub,
				      $runDir, $whatItDoes);

  # Don't allow target sequences to be split because we don't lift them
  # nor do we cat them before chaining.  Use the max target seq size
  # as the chunkSize for partitionSequence.pl on the target.
  my $tpSize = `awk '{print \$2;}' $tSizes | sort -nr | head -1`;
  chomp $tpSize;
  # However, $tDb might be a collection of zillions of tiny scaffolds.
  # So to ensure reasonable cluster batch size, make sure that chunkSize
  # is at least 10,000,000 for the target.
  my $minTpSize = 10000000;
  $tpSize = $minTpSize if ($tpSize < $minTpSize);

  $bossScript->add(<<_EOF_
# Compute partition (coordinate ranges) for cluster job.  This does
# not need to be run on the build fileserver because it does not actually
# split any sequences -- it merely computes ranges based on the chrom.sizes.
rm -rf tParts
$Bin/partitionSequence.pl $tpSize 0 $tSeqScratch \\
   $tSizes 2000 \\
  -lstDir=tParts > t.lst
rm -rf qParts
$Bin/partitionSequence.pl 10000000 0 $qSeqScratch \\
  $qSizes 1000 \\
  -lstDir=qParts > q.lst

mkdir $pslDir
foreach f (`cat t.lst`)
  mkdir $pslDir/\$f:t
end

$HgAutomate::gensub2 t.lst q.lst gsub jobList

$HgAutomate::paraRun
_EOF_
  );
  $bossScript->execute();
} # doAlign


#########################################################################
# * step: chain [smallClusterHub]

sub makePslPartsLst {
  # $pslDir/$tPart/ files look like either
  #   part006.lst.psl --> make this into a single job.
  #   $qDb.2bit:$seq:$start-$end.psl --> cat $qDb.2bit:$seq:*.psl into a job.
  # ==> Make a list of any part*.lst.psl plus collapsed roots
  #     (one $qDb.2bit:$seq: per $seq).
  my ($pslDir) = @_;
  &HgAutomate::verbose(2, "Making a list of patterns from the contents of " .
		       "$pslDir\n");
  return if ($opt_debug);
  opendir(P, "$pslDir")
    || die "Couldn't open directory $pslDir for reading: $!\n";
  my @tParts = readdir(P);
  closedir(P);
  my $fh = &HgAutomate::mustOpen(">$buildDir/run.chain/pslParts.lst");
  my $totalCount = 0;
  foreach my $tPart (@tParts) {
    next if ($tPart =~ /^\.\.?$/);
    my %seqs = ();
    my $count = 0;
    opendir(P, "$pslDir/$tPart")
      || die "Couldn't open directory $pslDir/$tPart for reading: $!\n";
    my @qParts = readdir(P);
    closedir(P);
    foreach my $q (@qParts) {
      if ($q =~ /^part\d+.lst.psl$/) {
	print $fh "$tPart/$q\n";
	$count++;
      } elsif ($q =~ s@^(\S+\.2bit:\w+):\d+.*@$1@) {
	# Collapse subsequences (subranges of a sequence) down to one entry
	# per sequence:
	$seqs{$q} = 1;
      } elsif ($q ne '.' && $q ne '..') {
	warn "makePslPartsLst: Unrecognized partition file format \"$q\"";
      }
    }
    foreach my $q (keys %seqs) {
      print $fh "$tPart/$q:\n";
      $count++;
    }
    if ($count < 1) {
      die "makePslPartsLst: didn't find anything in $pslDir/$tPart/ .";
    }
    $totalCount += $count;
  }
  close($fh);
  if ($totalCount < 1) {
    die "makePslPartsLst: didn't find anything in $pslDir/ .";
  }
  &HgAutomate::verbose(2, "Found $totalCount patterns in $pslDir/*/.\n");
} # makePslPartsLst

sub doChain {
  my $runDir = "$buildDir/run.chain";
  &HgAutomate::mustMkdir($runDir);

  my $pslDir = "$buildDir/run.blat/psl";
  my $blatDoneFile = "$buildDir/run.blat/run.time";
  &HgAutomate::checkCleanSlate('chain', 'net', 'chainRaw');
  &HgAutomate::checkExistsUnlessDebug('align', 'chain',
				      $pslDir, $blatDoneFile);

  &makePslPartsLst($pslDir);
  my ($paraHub, $tSeqScratch, $qSeqScratch) = &getClusterSeqs();

  # script for a single job: cat inputs if necessary and chain.
  my $fh = &HgAutomate::mustOpen(">$runDir/job.csh");
  print $fh <<_EOF_
#!/bin/csh -ef

set inPattern = \$1
set outChain = \$2

set tmpOut = `mktemp -p /scratch/tmp doSame.chain.XXXXXX`

cat $pslDir/\$inPattern* \\
| axtChain -verbose=0 -linearGap=medium -psl stdin \\
    $tSeqScratch $qSeqScratch \\
    \$tmpOut
mv \$tmpOut \$outChain
chmod 664 \$outChain
_EOF_
  ;
  close($fh);
  &HgAutomate::run("chmod a+x $runDir/job.csh");

  &HgAutomate::makeGsub($runDir,
			'job.csh $(path1) ' .
			'{check out line+ chainRaw/$(path1).chain}');
  my $whatItDoes = "It does a cluster run to chain the blat alignments.";
  my $bossScript = new HgRemoteScript("$runDir/doChain.csh", $paraHub,
				      $runDir, $whatItDoes);
  $bossScript->add(<<_EOF_
mkdir chainRaw
foreach d ($pslDir/*)
  mkdir chainRaw/\$d:t
end

$HgAutomate::gensub2 pslParts.lst single gsub jobList
$HgAutomate::paraRun
_EOF_
  );
  $bossScript->execute();
} # doChain


#########################################################################
# * step: net [workhorse]
sub doNet {
  my $runDir = "$buildDir/run.chain";
  my @outs = ("$runDir/$tDb.$qDb.all.chain/gz",
	      "$runDir/$tDb.$qDb.noClass.net.gz");
  &HgAutomate::checkCleanSlate('net', 'load', @outs);
  &HgAutomate::checkExistsUnlessDebug('chain', 'net', "$runDir/chainRaw/");

  my $whatItDoes =
"It nets the chained blat alignments and runs netChainSubset to produce
liftOver chains.";
  my $mach = &HgAutomate::chooseWorkhorse();
  my $bossScript = new HgRemoteScript("$runDir/doNet.csh", $mach,
				      $runDir, $whatItDoes);
  my $chromBased = (`wc -l < $tSizes` <= $HgAutomate::splitThreshold);
  my $lump = $chromBased ? "" : "-lump=100";
  $bossScript->add(<<_EOF_
# Use local scratch disk... this can be quite I/O intensive:
set tmpDir = `mktemp -d -p /scratch/tmp doSame.blat.XXXXXX`

# Merge up the hierarchy and assign unique chain IDs:
mkdir \$tmpDir/chainMerged
foreach d (chainRaw/*)
  set tChunk = \$d:t
  chainMergeSort \$d/*.chain > \$tmpDir/chainMerged/\$tChunk.chain
end

chainMergeSort \$tmpDir/chainMerged/*.chain \\
| chainSplit $lump \$tmpDir/chainSplit stdin
endsInLf \$tmpDir/chainSplit/*.chain
rm -rf \$tmpDir/chainMerged/

mkdir \$tmpDir/netSplit \$tmpDir/overSplit
foreach f (\$tmpDir/chainSplit/*.chain)
  set split = \$f:t:r
  chainNet \$f \\
    $tSizes $qSizes \\
    \$tmpDir/netSplit/\$split.net /dev/null
  netChainSubset \$tmpDir/netSplit/\$split.net \$f stdout \\
  | chainStitchId stdin \$tmpDir/overSplit/\$split.chain
end
endsInLf \$tmpDir/netSplit/*.net
endsInLf \$tmpDir/overSplit/*.chain

cat \$tmpDir/chainSplit/*.chain | gzip -c > $tDb.$qDb.all.chain.gz
cat \$tmpDir/netSplit/*.net     | gzip -c > $tDb.$qDb.noClass.net.gz

cat \$tmpDir/overSplit/*.chain | gzip -c > $buildDir/$liftOverChainFile

rm -rf \$tmpDir/
_EOF_
  );
  $bossScript->execute();
} # doNet


#########################################################################
# * step: load [dbHost]
sub doLoad {
  my $runDir = "$buildDir";
  &HgAutomate::checkExistsUnlessDebug('net', 'load',
				      "$buildDir/$liftOverChainFile");

  my $whatItDoes =
"It makes links from $HgAutomate::gbdb/ and goldenPath/ (download area) to the liftOver
chains file, and calls hgAddLiftOverChain to register the $HgAutomate::gbdb location.";
  my $bossScript = new HgRemoteScript("$runDir/doLoad.csh", $dbHost,
				      $runDir, $whatItDoes);
  $bossScript->add(<<_EOF_
# Link to standardized location of liftOver files:
mkdir -p $liftOverChainDir
rm -f $liftOverChainPath
ln -s $buildDir/$liftOverChainFile $liftOverChainPath

# Link from download area:
mkdir -p $HgAutomate::goldenPath/$tDb/liftOver
rm -f $HgAutomate::goldenPath/$tDb/liftOver/$liftOverChainFile
ln -s $liftOverChainPath $HgAutomate::goldenPath/$tDb/liftOver/

# Link from genome browser fileserver:
mkdir -p $HgAutomate::gbdb/$tDb/liftOver
rm -f $HgAutomate::gbdb/$tDb/liftOver/$liftOverChainFile
ln -s $liftOverChainPath $HgAutomate::gbdb/$tDb/liftOver/

# Add an entry to liftOverChain table in central database (specified in
# ~/.hg.conf) so that hgLiftOver will know that this is available:
hgAddLiftOverChain $tDb $qDb
_EOF_
  );
  $bossScript->execute();
} # doLoad


#########################################################################
# * step: cleanup [fileServer]
sub doCleanup {
  my $runDir = "$buildDir";
  my $whatItDoes = "It cleans up or compresses intermediate files.";
  my $fileServer = &HgAutomate::chooseFileServer($runDir);
  my $bossScript = new HgRemoteScript("$runDir/doCleanup.csh", $fileServer,
				      $runDir, $whatItDoes);
  my $pslDir = "run.blat/psl";
  $bossScript->add(<<_EOF_
rm -rf $pslDir/
rm -rf run.chain/chainRaw/
rm -rf run.chain/chainMerged/
rm -rf run.chain/chainSplit/
rm -rf run.chain/netSplit/
rm -rf run.chain/overSplit/
_EOF_
  );
  $bossScript->execute();
} # doCleanup


sub getSeqAndSizes {
  # Test assumptions about 2bit and chrom.sizes files.
  $tSeq = "$HgAutomate::clusterData/$tDb/$tDb.2bit";
  $tSizes = "$HgAutomate::clusterData/$tDb/chrom.sizes";
  $qSeq = "$HgAutomate::clusterData/$qDb/$qDb.2bit";
  $qSizes = "$HgAutomate::clusterData/$qDb/chrom.sizes";
  my $problem = 0;
  foreach my $file ($tSeq, $tSizes, $qSeq, $qSizes) {
    if (! -e $file) {
      warn "Error: cannot find required file \"$file\"\n";
      $problem = 1;
    }
  }
  if ($problem && !$opt_debug) {
    warn "Run $base -help for a description of expected files.\n";
    exit 1;
  }
}

#########################################################################
# main

# Prevent "Suspended (tty input)" hanging:
&HgAutomate::closeStdin();

&checkOptions();

&usage(1) if (scalar(@ARGV) != 2);
($tDb, $qDb) = @ARGV;

&getSeqAndSizes();
$QDb = ucfirst($qDb);
$liftOverChainDir = "$HgAutomate::clusterData/$tDb/$HgAutomate::trackBuild/liftOver";
$liftOverChainFile = "${tDb}To${QDb}.over.chain.gz";
$liftOverChainPath = "$liftOverChainDir/$liftOverChainFile";

my $date = `date +%Y-%m-%d`;
chomp $date;
$buildDir = $opt_buildDir ? $opt_buildDir :
  "$HgAutomate::clusterData/$tDb/$HgAutomate::trackBuild/blat.$qDb.$date";

if (! -d $buildDir) {
  if ($stepper->stepPrecedes('align', $stepper->getStartStep())) {
    die "$buildDir does not exist; try running again with -buildDir.\n";
  }
  &HgAutomate::mustMkdir($buildDir);
}

$stepper->execute();

my $stopStep = $stepper->getStopStep();
my $upThrough = ($stopStep eq 'cleanup') ? "" :
  "  (through the '$stopStep' step)";

&HgAutomate::verbose(1,
	"\n *** All done!$upThrough\n");
&HgAutomate::verbose(1,
	" *** Steps were performed in $buildDir\n");
if ($stepper->stepPrecedes('net', $stopStep)) {
  &HgAutomate::verbose(1,
	" *** Test installation ($HgAutomate::gbdb, goldenPath, hgLiftover " .
	"operation) on $dbHost.\n");
}
&HgAutomate::verbose(1, "\n");

