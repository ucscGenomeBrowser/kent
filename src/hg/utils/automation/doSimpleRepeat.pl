#!/usr/bin/env perl

# DO NOT EDIT the /cluster/bin/scripts copy of this file --
# edit ~/kent/src/hg/utils/automation/doSimpleRepeat.pl instead.

# $Id: doSimpleRepeat.pl,v 1.5 2009/06/08 18:38:58 hiram Exp $

use Getopt::Long;
use warnings;
use strict;
use Carp;
use FindBin qw($Bin);
use lib "$Bin";
use HgAutomate;
use HgRemoteScript;
use HgStepManager;

# Hardcoded (for now):
my $chunkSize = 50000000;
my $singleRunSize = 200000000;
my $clusterBin = qw(/cluster/bin/$MACHTYPE);

# Option variable names, both common and peculiar to this script:
use vars @HgAutomate::commonOptionVars;
use vars @HgStepManager::optionVars;
use vars qw/
    $opt_buildDir
    $opt_unmaskedSeq
    $opt_trf409
    /;

# Specify the steps supported with -continue / -stop:
my $stepper = new HgStepManager(
    [ { name => 'trf',     func => \&doTrf },
      { name => 'filter',  func => \&doFilter },
      { name => 'load',    func => \&doLoad },
      { name => 'cleanup', func => \&doCleanup },
    ]
				);

# Option defaults:
my $defaultSmallClusterHub = 'most available';
my $defaultWorkhorse = 'least loaded';
my $dbHost = 'hgwdev';
my $unmaskedSeq = "\$db.unmasked.2bit";
my $trf409 = "";

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
                          $HgAutomate::clusterData/\$db/$HgAutomate::trackBuild/simpleRepeat.\$date
                          (necessary when continuing at a later date).
    -unmaskedSeq seq.2bit Use seq.2bit as the unmasked input sequence instead
                          of default ($unmaskedSeq).
    -trf409 n             use new -l option to trf v4.09 (l=n)
                          maximum TR length expected (in millions)
                          (eg, -l=3 for 3 million)
                          Human genome hg38 uses: -trf409=6 -> -l=6
_EOF_
  ;
  print STDERR &HgAutomate::getCommonOptionHelp('dbHost' => $dbHost,
						'workhorse' => '',
						'smallClusterHub' => '');
  my ($sizeM, $chunkM) = ($singleRunSize, $chunkSize);
  $sizeM =~ s/000000$/Mb/;  $chunkM =~ s/000000$/Mb/;
  print STDERR "
Automates UCSC's simpleRepeat (TRF) process for genome database \$db.  Steps:
    trf:     If total genome size is <= $sizeM, run trfBig on a workhorse;
             otherwise do a cluster run of trfBig on $chunkM sequence chunks.
    filter:  If a cluster run was performed, concatenate the results into
             simpleRepeat.bed.  Filter simpleRepeat.bed (period <= 12) to
             trfMask.bed.  If \$db is chrom-based, split trfMaskBed into
             trfMaskChrom/chr*.bed for downloads.
    load:    Load simpleRepeat.bed into the simpleRepeat table in \$db.
    cleanup: Removes or compresses intermediate files.
All operations are performed in the build directory which is
$HgAutomate::clusterData/\$db/$HgAutomate::trackBuild/simpleRepeat.\$date unless -buildDir is given.
Run -help to see what files are required for this script.
";
  # Detailed help (-help):
  print STDERR "
Assumptions:
1. $HgAutomate::clusterData/\$db/\$db.unmasked.2bit contains sequence for
   database/assembly \$db.  (This can be overridden with -unmaskedSeq.)
2. $clusterBin/trfBig and $clusterBin/trf exist
   on workhorse, fileServer and cluster nodes.
" if ($detailed);
  print "\n";
  exit $status;
}


# Globals:
# Command line args: db
my ($db);
# Other:
my ($buildDir, $chromBased, $useCluster);

sub checkOptions {
  # Make sure command line options are valid/supported.
  my $ok = GetOptions(@HgStepManager::optionSpec,
		      'buildDir=s',
		      'unmaskedSeq=s',
		      'trf409=s',
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
# * step: trf [smallClusterHub or workhorse depending on genome size]
sub doCluster {
  my $runDir = "$buildDir/run.cluster";
  &HgAutomate::mustMkdir($runDir);

  my $paraHub = $opt_smallClusterHub ? $opt_smallClusterHub :
    &HgAutomate::chooseSmallClusterByBandwidth();
  my @okIn = grep !/scratch/,
    &HgAutomate::chooseFilesystemsForCluster($paraHub, "in");
  my @okOut =
    &HgAutomate::chooseFilesystemsForCluster($paraHub, "out");
  if (scalar(@okOut) > 1) {
    @okOut = grep !/$okIn[0]/, @okOut;
  }
  my $inHive = 0;
  $inHive = 1 if ($okIn[0] =~ m#/hive/data/genomes#);
  my $clusterSeqDir = "$okIn[0]/$db";
  $clusterSeqDir = "$buildDir" if ($opt_unmaskedSeq);
  my $clusterSeq = "$clusterSeqDir/$db.doSimp.2bit";
  if ($inHive) {
    $clusterSeq = "$clusterSeqDir/$db.unmasked.2bit";
  }
  $clusterSeq = "$unmaskedSeq" if ($opt_unmaskedSeq);
  my $partDir .= "$okOut[0]/$db/TrfPart";
  $partDir = "$buildDir/TrfPart" if ($opt_unmaskedSeq);

  my $trf409Option = "";
  my $trfCmd = "trf";
  if ($trf409 ne 0) {
     $trf409Option = "-l=$trf409";
     $trfCmd = "trf.4.09";
  }
  # Cluster job script:
  my $fh = &HgAutomate::mustOpen(">$runDir/TrfRun.csh");
  print $fh <<_EOF_
#!/bin/csh -ef

set finalOut = \$1

set inLst = \$finalOut:r
set inLft = \$inLst:r.lft

$HgAutomate::setMachtype

# Use local disk for output, and move the final result to \$finalOut
# when done, to minimize I/O.
set tmpDir = `mktemp -d -p /scratch/tmp doSimpleRepeat.cluster.XXXXXX`
pushd \$tmpDir

foreach spec (`cat \$inLst`)
  # Remove path and .2bit filename to get just the seq:start-end spec:
  set base = `echo \$spec | sed -r -e 's/^[^:]+://'`

  # If \$spec is the whole sequence, twoBitToFa removes the :start-end part,
  # which causes liftUp to barf later.  So tweak the header back to
  # seq:start-end for liftUp's sake:
  twoBitToFa \$spec stdout \\
  | sed -e "s/^>.*/>\$base/" \\
  | $clusterBin/trfBig $trf409Option -trf=$clusterBin/$trfCmd \\
      stdin /dev/null -bedAt=\$base.bed -tempDir=/scratch/tmp
end

# Due to the large chunk size, .lft files can have thousands of lines.
# Even though the liftUp code does &lineFileClose, somehow we still
# run out of filehandles.  So limit the size of liftSpecs:
split -a 3 -d -l 500 \$inLft SplitLft.

# Lift up:
foreach splitLft (SplitLft.*)
  set bedFiles = `awk '{print \$2 ".bed"};' \$splitLft`
  endsInLf -zeroOk \$bedFiles
  set lineCount=`cat \$bedFiles | wc -l`
  if (\$lineCount > 0) then
    cat \$bedFiles \\
      | liftUp -type=.bed tmpOut.\$splitLft \$splitLft error stdin
  else
    touch tmpOut.\$splitLft
  endif
end
cat tmpOut.* > tmpOut__bed

# endsInLf is much faster than using para to {check out line}:
endsInLf -zeroOk tmpOut*

# Move final result into place:
mv tmpOut__bed \$finalOut

popd
rm -rf \$tmpDir
_EOF_
  ;
  close($fh);

  &HgAutomate::makeGsub($runDir,
      "./TrfRun.csh $partDir/\$(path1).bed");
  `touch "$runDir/para_hub_$paraHub"`;

  my $chunkM = $chunkSize;
  $chunkM =~ s/000000$/Mb/;
  my $whatItDoes =
"It computes a logical partition of unmasked 2bit into $chunkM chunks
and runs it on the cluster with the most available bandwidth.";
  my $bossScript = new HgRemoteScript("$runDir/doTrf.csh", $paraHub,
				      $runDir, $whatItDoes);

  if ( ! $opt_unmaskedSeq && ! $inHive) {
    $bossScript->add(<<_EOF_
mkdir -p $clusterSeqDir
rsync -av $unmaskedSeq $clusterSeq
_EOF_
    );
  }

  my $paraRun = &HgAutomate::paraRun();
  my $gensub2 = &HgAutomate::gensub2();
  if ($opt_unmaskedSeq) {
    $bossScript->add(<<_EOF_
chmod a+x TrfRun.csh

rm -rf $partDir
$Bin/simplePartition.pl $clusterSeq $chunkSize $partDir

$gensub2 $partDir/partitions.lst single gsub jobList
$paraRun
_EOF_
    );
  } else {
  my $paraRun = &HgAutomate::paraRun();
  my $gensub2 = &HgAutomate::gensub2();
  $bossScript->add(<<_EOF_
chmod a+x TrfRun.csh

rm -rf $partDir
$Bin/simplePartition.pl $clusterSeq $chunkSize $partDir
rm -f $buildDir/TrfPart
ln -s $partDir $buildDir/TrfPart

$gensub2 $partDir/partitions.lst single gsub jobList
$paraRun
_EOF_
  );
  }

  if (! $opt_unmaskedSeq && ! $inHive) {
    $bossScript->add(<<_EOF_
rm -f $clusterSeq
_EOF_
    );
  }
  $bossScript->execute();
} # doCluster

sub doSingle {
  my $runDir = "$buildDir";
  &HgAutomate::mustMkdir($runDir);

  my $workhorse = &HgAutomate::chooseWorkhorse();
  my $whatItDoes = "It runs trfBig on the entire (small) genome in one pass.";
  my $bossScript = new HgRemoteScript("$runDir/doTrf.csh", $workhorse,
				      $runDir, $whatItDoes);

  my $trf409Option = "";
  my $trfCmd = "trf";
  if ($trf409 ne 0) {
     $trf409Option = "-l=$trf409";
     $trfCmd = "trf.4.09";
  }
  $bossScript->add(<<_EOF_
$HgAutomate::setMachtype
twoBitToFa $unmaskedSeq stdout \\
| $clusterBin/trfBig $trf409Option -trf=$clusterBin/$trfCmd \\
      stdin /dev/null -bedAt=simpleRepeat.bed -tempDir=/scratch/tmp
_EOF_
  );
  $bossScript->execute();
} # doSingle

sub doTrf {
  if ($useCluster) {
    &doCluster();
  } else {
    &doSingle();
  }
} # doTrf


#########################################################################
# * step: filter [fileServer]
sub doFilter {
  my $runDir = "$buildDir";
  if ($useCluster) {
    &HgAutomate::checkExistsUnlessDebug('trf', 'filter',
					"$buildDir/run.cluster/run.time");
  }
  my $whatItDoes = "It filters simpleRepeat.bed > trfMask.bed.\n";
  if ($useCluster) {
    $whatItDoes =
      "It concatenates .bed files from cluster run into simpleRepeat.bed.\n"
	. $whatItDoes;
  }
  if ($chromBased) {
    $whatItDoes .=
"It splits trfMask.bed into per-chrom files for bigZips download generation.";
  }
  my $fileServer = &HgAutomate::chooseFileServer($runDir);
  my $bossScript = new HgRemoteScript("$runDir/doFilter.csh", $fileServer,
				      $runDir, $whatItDoes);

  # Use symbolic link created in cluster step:
  my $partDir = "$buildDir/TrfPart";
  if ($useCluster) {
    $bossScript->add(<<_EOF_
cat $partDir/???/*.bed > simpleRepeat.bed
endsInLf simpleRepeat.bed
if (\$status) then
  echo Uh-oh -- simpleRepeat.bed fails endsInLf.  Look at $partDir/ bed files.
  exit 1
endif
_EOF_
    );
  }
  $bossScript->add(<<_EOF_
if ( -s simpleRepeat.bed ) then
  awk '{if (\$5 <= 12) print;}' simpleRepeat.bed > trfMask.bed
  awk 'BEGIN{OFS="\\t"}{name=substr(\$16,0,16);\$4=name;printf "%s\\n", \$0}' \\
    simpleRepeat.bed | sort -k1,1 -k2,2n > simpleRepeat.bed16.bed
  twoBitInfo $unmaskedSeq stdout | sort -k2nr > tmp.chrom.sizes
  bedToBigBed -tab -type=bed4+12 -as=\$HOME/kent/src/hg/lib/simpleRepeat.as \\
    simpleRepeat.bed16.bed tmp.chrom.sizes simpleRepeat.bb
  rm -f tmp.chrom.sizes simpleRepeat.bed16.bed tmp.chrom.sizes
else
  echo empty simpleRepeat.bed - no repeats found
endif
_EOF_
  );
  if ($chromBased) {
    $bossScript->add(<<_EOF_
if ( -s trfMask.bed ) then
  splitFileByColumn trfMask.bed trfMaskChrom/
else
  echo empty trfMask.bed - no repeats found
endif
_EOF_
    );
  }

  $bossScript->execute();
} # doFilter


#########################################################################
# * step: load [dbHost]
sub doLoad {
  my $runDir = "$buildDir";
  &HgAutomate::checkExistsUnlessDebug('filter', 'load',
				      "$buildDir/simpleRepeat.bed");

  my $whatItDoes = "It loads simpleRepeat.bed into the simpleRepeat table.";
  my $bossScript = new HgRemoteScript("$runDir/doLoad.csh", $dbHost,
				      $runDir, $whatItDoes);

  $bossScript->add(<<_EOF_
if ( -s "simpleRepeat.bed" ) then
  hgLoadBed $db simpleRepeat simpleRepeat.bed \\
        -sqlTable=\$HOME/kent/src/hg/lib/simpleRepeat.sql
  featureBits $db simpleRepeat >& fb.simpleRepeat
  cat fb.simpleRepeat
else
  echo empty simpleRepeat.bed - no repeats found
endif
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
  $bossScript->add(<<_EOF_
if ( -d "TrfPart" || -l "TrfPart" ) then
  rm -fr TrfPart/*
  rm -fr TrfPart
endif
if (-d /hive/data/genomes/$db/TrfPart) then
  rmdir /hive/data/genomes/$db/TrfPart
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

if (! $opt_trf409 ) {
  die "Error: must supply -trf409 argument, e.g.: -trf409=6\n";
}

# Now that we know the $db, figure out our paths:
my $date = `date +%Y-%m-%d`;
chomp $date;
$buildDir = $opt_buildDir ? $opt_buildDir :
  "$HgAutomate::clusterData/$db/$HgAutomate::trackBuild/simpleRepeat.$date";
$unmaskedSeq = $opt_unmaskedSeq ? $opt_unmaskedSeq :
  "$HgAutomate::clusterData/$db/$db.unmasked.2bit";

$trf409 = $opt_trf409 ? $opt_trf409 : "";

if (! -e $unmaskedSeq) {
  die $opt_unmaskedSeq ? "Error: -unmaskedSeq $unmaskedSeq does not exist.\n" :
    "Error: required file $unmaskedSeq does not exist. " .
      "(use -unmaskedSeq <file> ?)\n";
}
die "Error: -unmaskedSeq filename must end in .2bit (got $unmaskedSeq).\n"
  if ($unmaskedSeq !~ /\.2bit$/);
if ($unmaskedSeq !~ /^\//) {
  my $pwd = `pwd`;
  chomp $pwd;
  $unmaskedSeq = "$pwd/$unmaskedSeq";
}

my $pipe = &HgAutomate::mustOpen("twoBitInfo $unmaskedSeq stdout |");
my $seqCount = 0;
my $genomeSize = 0;
while (<$pipe>) {
  chomp;
  my (undef, $size) = split;
  $genomeSize += $size;
  $seqCount++;
  if ($seqCount > $HgAutomate::splitThreshold &&
      $genomeSize > $singleRunSize) {
    # No need to keep counting -- we know our boolean answers.
    last;
  }
}
close($pipe);
die "Could not open pipe from twoBitInfo $unmaskedSeq"
  unless ($genomeSize > 0 && $seqCount > 0);

$chromBased = ($seqCount <= $HgAutomate::splitThreshold);
$useCluster = ($genomeSize > $singleRunSize);
&HgAutomate::verbose(2, "\n$db is " .
		     ($chromBased ? "chrom-based" : "scaffold-based") . ".\n" .
		     "Total genome size is " .
		     ($useCluster ? "> $singleRunSize; cluster & cat." :
		                    "<= $singleRunSize; single run.") .
		     "\n\n");

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
if ($stepper->stepPrecedes('trf', $stopStep)) {
  my $buildDirRel = $buildDir;
  $buildDirRel =~ s/^$HgAutomate::clusterData\/$db\///;
  &HgAutomate::verbose(1, <<_EOF_
 *** Please check the log file to see if trf had warnings that we
     should pass on to Gary Benson (ideally with a small test case).
 *** IMPORTANT: Developer, it is up to you to make use of trfMask.bed!
     Here is a typical command sequence, to be run after RepeatMasker
     (or WindowMasker etc.) has completed:

    cd $HgAutomate::clusterData/$db
    twoBitMask $db.rmsk.2bit -add $buildDirRel/trfMask.bed $db.2bit

 *** Again, it is up to you to incorporate trfMask.bed into the final 2bit!
_EOF_
  );
}
&HgAutomate::verbose(1, "\n");
