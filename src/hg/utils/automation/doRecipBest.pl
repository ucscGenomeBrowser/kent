#!/usr/bin/env perl

# DO NOT EDIT the /cluster/bin/scripts copy of this file --
# edit ~/kent/src/hg/utils/automation/doRecipBest.pl instead.

# This script should probably be folded back into doBlastzChainNet.pl
# eventually.

# $Id: doRecipBest.pl,v 1.12 2010/02/04 18:33:56 hiram Exp $

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
    $opt_load
    $opt_target2Bit
    $opt_query2Bit
    $opt_targetSizes
    $opt_querySizes
    $opt_skipDownload
    $opt_trackHub
    /;

# Specify the steps supported with -continue / -stop:
my $stepper = new HgStepManager(
    [ { name => 'recipBest',  func => \&doRecipBest },
      { name => 'download',   func => \&doDownload },
      { name => 'load',       func => \&loadRBest },
      { name => 'cleanup',    func => \&cleanUp },
    ]
				);

# Option defaults:
my $dbHost = 'hgwdev';

my ($dbExists, $qDbExists);

my $base = $0;
$base =~ s/^(.*\/)?//;

sub usage {
  # Usage / help / self-documentation:
  my ($status, $detailed) = @_;
  # Basic help (for incorrect usage):
  print STDERR "
usage: $base tDb qDb
options:
";
  print STDERR $stepper->getOptionHelp();
  print STDERR <<_EOF_
    -buildDir dir         Use dir instead of default
                          $HgAutomate::clusterData/\$tDb/$HgAutomate::trackBuild/blastz.\$qDb
    -load                 load the resulting chainNet into database
    -target2Bit path      path to target.2bit file
    -query2Bit path       path to query.2bit file
    -targetSizes path     path to target chrom.sizes file
    -querySizes path      path to query chrom.sizes file
    -skipDownload         do not construct the downloads directory
    -trackHub             construct big* files for track hub
_EOF_
  ;
  print STDERR &HgAutomate::getCommonOptionHelp('dbHost' => $dbHost,
						'workhorse' => '');
  print STDERR "
Automates addition of reciprocal best chains/nets to regular chains/nets which
have already been created using doBlastzChainNet.pl.  Steps:
    recipBest: Net in both directions to get reciprocal best.
    download: Make a reciprocalBest subdir of the existing download dir.
    load:     load reciprocal best chain/net tables.
    cleanup   remove temporary files created during this process.
All work is done in the axtChain subdir of the build directory:
$HgAutomate::clusterData/\$tDb/$HgAutomate::trackBuild/blastz.\$qDb unless -buildDir is given.
";
  # Detailed help (-help):
  print STDERR "
Assumptions:
1. $HgAutomate::clusterData/\$db/\$db.2bit contains RepeatMasked sequence for
   database/assembly \$db (for both \$tDb and \$qDb).
2. $HgAutomate::clusterData/\$db/chrom.sizes contains all sequence names and sizes from
   \$db.2bit (for both \$tDb and \$qDb).
3. The buildDir contains axtChain/\$tDb.\$qDb.over.chain.gz and the download dir
   goldenPath/\$tDb/vs\$QDb already exists.
" if ($detailed);
  print "\n";
  exit $status;
}

# Globals:
# Command line args: tDb qDb
my ($tDb, $qDb);
# Other:
my ($QDb, $buildDir, $splitRef, $target2Bit, $query2Bit, $targetSizes, $querySizes);

sub checkOptions {
  # Make sure command line options are valid/supported.
  my $ok = GetOptions(@HgStepManager::optionSpec,
		      'buildDir=s',
		      'load',
		      'target2Bit=s',
		      'query2Bit=s',
		      'targetSizes=s',
		      'querySizes=s',
                      "skipDownload",
                      "trackHub",
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
# * step: recipBest [workhorse]
sub doRecipBest {
  my $runDir = "$buildDir/axtChain";
  &HgAutomate::mustMkdir($runDir);
  &HgAutomate::checkExistsUnlessDebug("doBlastzChainNet.pl", "recipBest",
				      "$runDir/$tDb.$qDb.over.chain.gz");
  &HgAutomate::checkCleanSlate("recipBest", "download",
			       "$runDir/$qDb.$tDb.rbest.net.gz");
  my $whatItDoes =
    "It nets in both directions to get reciprocal best chains and nets.";
  my $workhorse = &HgAutomate::chooseWorkhorse();
  my $bossScript = new HgRemoteScript("$runDir/doRecipBest.csh", $workhorse,
				      $runDir, $whatItDoes);

  $bossScript->add(<<_EOF_
# Swap $tDb-best chains to be $qDb-referenced:
chainStitchId $tDb.$qDb.over.chain.gz stdout \\
| chainSwap stdin stdout \\
| chainSort stdin $qDb.$tDb.tBest.chain

# Net those on $qDb to get $qDb-ref'd reciprocal best net:
chainPreNet $qDb.$tDb.tBest.chain \\
  $querySizes $targetSizes stdout \\
| chainNet -minSpace=1 -minScore=0 \\
  stdin $querySizes $targetSizes stdout /dev/null \\
| netSyntenic stdin stdout \\
| gzip -c > $qDb.$tDb.rbest.net.gz

# Extract $qDb-ref'd reciprocal best chain:
netChainSubset $qDb.$tDb.rbest.net.gz $qDb.$tDb.tBest.chain stdout \\
| chainStitchId stdin stdout \\
| gzip -c > $qDb.$tDb.rbest.chain.gz

# Swap to get $tDb-ref'd reciprocal best chain:
chainSwap $qDb.$tDb.rbest.chain.gz stdout \\
| chainSort stdin stdout \\
| gzip -c > $tDb.$qDb.rbest.chain.gz

# Net those on $tDb to get $tDb-ref'd reciprocal best net:
chainPreNet $tDb.$qDb.rbest.chain.gz \\
  $targetSizes $querySizes stdout \\
| chainNet -minSpace=1 -minScore=0 \\
  stdin $targetSizes $querySizes stdout /dev/null \\
| netSyntenic stdin stdout \\
| gzip -c > $tDb.$qDb.rbest.net.gz

# Clean up the one temp file and make md5sum:
rm $qDb.$tDb.tBest.chain
md5sum *.rbest.*.gz > md5sum.rbest.txt

# Create files for testing coverage of *.rbest.*.
netToBed -maxGap=1 $qDb.$tDb.rbest.net.gz $qDb.$tDb.rbest.net.bed
netToBed -maxGap=1 $tDb.$qDb.rbest.net.gz $tDb.$qDb.rbest.net.bed
chainToPsl $qDb.$tDb.rbest.chain.gz \\
  $querySizes $targetSizes \\
  $query2Bit $target2Bit \\
  $qDb.$tDb.rbest.chain.psl
chainToPsl $tDb.$qDb.rbest.chain.gz \\
  $targetSizes $querySizes \\
  $target2Bit $query2Bit \\
  $tDb.$qDb.rbest.chain.psl

# Verify that all coverage figures are equal:
set tChCov = `awk '{print \$19;}' $tDb.$qDb.rbest.chain.psl | sed -e 's/,/\\n/g' | awk 'BEGIN {N = 0;} {N += \$1;} END {printf "\%d\\n", N;}'`
set qChCov = `awk '{print \$19;}' $qDb.$tDb.rbest.chain.psl | sed -e 's/,/\\n/g' | awk 'BEGIN {N = 0;} {N += \$1;} END {printf "\%d\\n", N;}'`
set tNetCov = `awk 'BEGIN {N = 0;} {N += (\$3 - \$2);} END {printf "\%d\\n", N;}' $tDb.$qDb.rbest.net.bed`
set qNetCov = `awk 'BEGIN {N = 0;} {N += (\$3 - \$2);} END {printf "\%d\\n", N;}' $qDb.$tDb.rbest.net.bed`
if (\$tChCov != \$qChCov) then
  echo "Warning: $tDb rbest chain coverage \$tChCov != $qDb \$qChCov"
endif
if (\$tNetCov != \$qNetCov) then
  echo "Warning: $tDb rbest net coverage \$tNetCov != $qDb \$qNetCov"
endif
if (\$tChCov != \$tNetCov) then
  echo "Warning: $tDb rbest chain coverage \$tChCov != net cov \$tNetCov"
endif

mkdir experiments
mv *.bed *.psl experiments
_EOF_
    );

    # Create axt and maf files
if ($splitRef) {
  $bossScript->add(<<_EOF_
# Make rbest net axt's download: one .axt per $tDb seq.
netSplit $tDb.$qDb.rbest.net.gz rBestNet
chainSplit rBestChain $tDb.$qDb.rbest.chain.gz
cd ..
mkdir axtRBestNet
foreach f (axtChain/rBestNet/*.net)
    netToAxt \$f axtChain/rBestChain/\$f:t:r.chain \\
    $target2Bit $query2Bit stdout \\
    | axtSort stdin stdout \\
    | gzip -c > axtRBestNet/\$f:t:r.$tDb.$qDb.net.axt.gz
  end
cd axtRBestNet
md5sum *.axt.gz > md5sum.txt
cd ..

# Make rbest mafNet for multiz: one .maf per $tDb seq.
mkdir mafRBestNet
foreach f (axtRBestNet/*.$tDb.$qDb.net.axt.gz)
    axtToMaf -tPrefix=$tDb. -qPrefix=$qDb. \$f \\
        $targetSizes $querySizes \\
            stdout \\
      | gzip -c > mafRBestNet/\$f:t:r:r:r:r:r.maf.gz
end
_EOF_
    );
    if ($opt_trackHub) {
      $bossScript->add(<<_EOF_
mkdir -p bigMaf
echo "##maf version=1 scoring=blastz" > bigMaf/$tDb.$qDb.rbestNet.maf
zegrep -h -v "^#" mafRBestNet/*.maf.gz >> bigMaf/$tDb.$qDb.rbestNet.maf
echo "##eof maf" >> bigMaf/$tDb.$qDb.rbestNet.maf
gzip bigMaf/$tDb.$qDb.rbestNet.maf
_EOF_
      );
    }
  } else {
  $bossScript->add(<<_EOF_
# Make rbest net axt's download
mkdir ../axtRBestNet
netToAxt $tDb.$qDb.rbest.net.gz $tDb.$qDb.rbest.chain.gz \\
    $target2Bit $query2Bit stdout \\
    | axtSort stdin stdout \\
    | gzip -c > ../axtRBestNet/$tDb.$qDb.rbest.axt.gz
# Make rbest mafNet for multiz
mkdir ../mafRBestNet
axtToMaf -tPrefix=$tDb. -qPrefix=$qDb. ../axtRBestNet/$tDb.$qDb.rbest.axt.gz \\
        $targetSizes $querySizes  \\
                stdout \\
      | gzip -c > ../mafRBestNet/$tDb.$qDb.rbest.maf.gz
cd ../mafRBestNet
md5sum *.maf.gz > md5sum.txt
cd ../axtRBestNet
md5sum *.axt.gz > md5sum.txt
_EOF_
    );
    if ($opt_trackHub) {
      $bossScript->add(<<_EOF_
mkdir -p ../bigMaf
cd ../bigMaf
ln -s ../mafRBestNet/$tDb.$qDb.rbest.maf.gz ./$tDb.$qDb.rbestNet.maf.gz
_EOF_
      );
    }
  }
  if ($opt_trackHub) {
      $bossScript->add(<<_EOF_
cd $buildDir/bigMaf
wget -O bigMaf.as 'http://genome-source.soe.ucsc.edu/gitlist/kent.git/raw/master/src/hg/lib/bigMaf.as'
wget -O mafSummary.as 'http://genome-source.soe.ucsc.edu/gitlist/kent.git/raw/master/src/hg/lib/mafSummary.as'
mafToBigMaf $tDb $tDb.$qDb.rbestNet.maf.gz stdout \\
  | sort -k1,1 -k2,2n > $tDb.$qDb.rbestNet.txt
bedToBigBed -type=bed3+1 -as=bigMaf.as -tab  $tDb.$qDb.rbestNet.txt \\
  $targetSizes $tDb.$qDb.rbestNet.bb
hgLoadMafSummary -minSeqSize=1 -test $tDb $tDb.$qDb.rbestNet.summary \\
        $tDb.$qDb.rbestNet.maf.gz
cut -f2- $tDb.$qDb.rbestNet.summary.tab | sort -k1,1 -k2,2n \\
        > $tDb.$qDb.rbestNet.summary.bed
bedToBigBed -type=bed3+4 -as=mafSummary.as -tab \\
        $tDb.$qDb.rbestNet.summary.bed \\
        $targetSizes $tDb.$qDb.rbestNet.summary.bb
rm -f $tDb.$qDb.rbestNet.txt $tDb.$qDb.rbestNet.summary.tab \\
        $tDb.$qDb.rbestNet.summary.bed
_EOF_
      );
  }

  $bossScript->execute();
} # doRecipBest

#########################################################################
# * step: download [dbHost]

sub makeRbestReadme {
  my ($fname) = @_;
  if ($opt_debug) {
    return;
  }
  my $axtNet = $splitRef ?
    "axtRBestNet/*.$tDb.$qDb.net.axt.gz reciprocal best alignments in
   AXT format" :
    "axtRBestNet/$tDb.$qDb.rbest.axt.gz reciprocal best alignments in
   AXT format";

  my $fh = &HgAutomate::mustOpen(">$fname");
  print $fh <<_EOF_
This directory contains reciprocal-best netted chains for $tDb-$qDb.

 - $tDb.$qDb.rbest.net.gz: $tDb-referenced recip.best net to $qDb.
 - $tDb.$qDb.rbest.chain.gz: chains extracted from the recip.best
   net.  These can be passed to the liftOver program to translate coords
   from $tDb to $qDb through the recip.best net.

 - $qDb.$tDb.rbest.net.gz: $qDb-referenced recip.best net.
 - $qDb.$tDb.rbest.chain.gz: recip.best "liftOver" chains.

 - $axtNet

See also, description of AXT format::
    http://genome.ucsc.edu/goldenPath/help/axt.html
description of CHAIN format:
    http://genome.ucsc.edu/goldenPath/help/chain.html
description of NET format:
    http://genome.ucsc.edu/goldenPath/help/net.html

_EOF_
  ;
  close($fh);
} # makeRbestReadme

sub doDownload {
  return if ($opt_skipDownload);
  my $runDir = "$buildDir/axtChain";
  my $whatItDoes = "It makes a download (sub)dir.";
  my $bossScript = new HgRemoteScript("$runDir/doRBDownload.csh", $dbHost,
				      $runDir, $whatItDoes);

  my $downloadDir = "$HgAutomate::goldenPath/$tDb/vs$QDb";
  &HgAutomate::checkExistsUnlessDebug("recipBest", "download",
				      "$runDir/$tDb.$qDb.rbest.net.gz",
				      $downloadDir);
  my $readme = "$runDir/README.rbest.txt";
  &makeRbestReadme($readme);

  $bossScript->add(<<_EOF_
mkdir $downloadDir/reciprocalBest
cd $downloadDir/reciprocalBest
ln -s $runDir/*.rbest.*.gz .
ln -s $runDir/md5sum.rbest.txt md5sum.txt
ln -s $readme README.txt
mkdir axtRBestNet
ln -s $buildDir/axtRBestNet/*.axt.gz $buildDir/axtRBestNet/md5sum.txt axtRBestNet/
_EOF_
  );
  $bossScript->execute();
} # doDownload

sub loadRBest {
  if (! $opt_load) {
    &HgAutomate::verbose(1, "# specify -load to perform load step\n");
    return;
  }
  # Load chains; add repeat/gap stats to net; load nets.
  my $runDir = "$buildDir/axtChain";
  my $QDbLink = "chainRBest$QDb" . "Link";
  # First, make sure we're starting clean.
  if (-e "$buildDir/fb.$tDb.chainRBest.$QDb.txt") {
    die "loadRBest looks like this was run successfully already " .
      "(fb.$tDb.chainRBest.$QDb.txt exists).\n";
  }
  # Make sure previous stage was successful.
  my $successDir = "$runDir/$tDb.$qDb.rbest.net.gz";
  if (! -e $successDir && ! $opt_debug) {
    die "loadRBest looks like previous stage was not successful " .
      "(can't find $successDir).\n";
  }
  my $whatItDoes =
"It loads the recip best chain tables into $tDb, adds gap/repeat stats to the recip best .net file,
and loads the recip net table.";
  my $bossScript = new HgRemoteScript("$runDir/loadRBest.csh", $dbHost,
				      $runDir, $whatItDoes);
  $bossScript->add(<<_EOF_
# Load reciprocal best chains:
_EOF_
  );
  if (! $opt_trackHub && $dbExists) {
    $bossScript->add(<<_EOF_
cd $runDir
hgLoadChain -tIndex $tDb chainRBest$QDb $tDb.$qDb.rbest.chain.gz
_EOF_
    );

    if ($qDbExists) {
    $bossScript->add(<<_EOF_

# Add gap/repeat stats to the net file using database tables:
cd $runDir
netClass -verbose=0 -noAr $tDb.$qDb.rbest.net.gz $tDb $qDb stdout \\
    | gzip -c > $tDb.$qDb.rbest.classed.net.gz

# Load nets:
netFilter -minGap=10 $tDb.$qDb.rbest.classed.net.gz \\
| hgLoadNet -verbose=0 $tDb netRBest$QDb stdin
_EOF_
      );
     }

    $bossScript->add(<<_EOF_
cd $buildDir
featureBits $tDb $QDbLink >&fb.$tDb.chainRBest.$QDb.txt
cat fb.$tDb.chainRBest.$QDb.txt
_EOF_
      );
  } else {
      $bossScript->add(<<_EOF_
cd $runDir
hgLoadChain -test -noBin -tIndex $tDb chainRBest$QDb $tDb.$qDb.rbest.chain.gz
wget -O bigChain.as 'http://genome-source.soe.ucsc.edu/gitlist/kent.git/raw/master/src/hg/lib/bigChain.as'
wget -O bigLink.as 'http://genome-source.soe.ucsc.edu/gitlist/kent.git/raw/master/src/hg/lib/bigLink.as'
sed 's/.000000//' chain.tab | awk 'BEGIN {OFS="\\t"} {print \$2, \$4, \$5, \$11, 1000, \$8, \$3, \$6, \$7, \$9, \$10, \$1}' > chainRBest${QDb}.tab
bedToBigBed -type=bed6+6 -as=bigChain.as -tab chainRBest${QDb}.tab $targetSizes chainRBest${QDb}.bb
awk 'BEGIN {OFS="\\t"} {print \$1, \$2, \$3, \$5, \$4}' link.tab | sort -k1,1 -k2,2n > $QDbLink.tab
bedToBigBed -type=bed4+1 -as=bigLink.as -tab $QDbLink.tab $targetSizes $QDbLink.bb
set totalBases = `ave -col=2 $targetSizes | grep "^total" | awk '{printf "%d", \$2}'`
set basesCovered = `bedSingleCover.pl $QDbLink.tab | ave -col=4 stdin | grep "^total" | awk '{printf "%d", \$2}'`
set percentCovered = `echo \$basesCovered \$totalBases | awk '{printf "%.3f", 100.0*\$1/\$2}'`
printf "%d bases of %d (%s%%) in intersection\\n" "\$basesCovered" "\$totalBases" "\$percentCovered" > ../fb.$tDb.chainRBest.$QDb.txt
rm -f link.tab
rm -f chain.tab
_EOF_
      );
  }	# else if (! $opt_trackHub && $dbExists)

  $bossScript->execute();
}	#	sub loadRBest {}

sub cleanUp {
  my $runDir = "$buildDir";
  my $whatItDoes = "cleanup temporary files used by RBest procedure.";
  my $bossScript = newBash HgRemoteScript("$runDir/rBestCleanUp.bash", $dbHost,
				      $runDir, $whatItDoes);
  $bossScript->add(<<_EOF_
rm -fr axtChain/experiments
rm -f axtChain/bigChain.as axtChain/bigLink.as
rm -f bigMaf/bigMaf.as
rm -f bigMaf/mafSummary.as
rm -fr axtChain/rBestNet axtChain/rBestChain
rm -f axtChain/chainRBest*.tab
_EOF_
  );

  $bossScript->execute();
}	#	sub cleanUp {}

#########################################################################
# main

#$opt_debug = 1;

# Prevent "Suspended (tty input)" hanging:
&HgAutomate::closeStdin();

# Make sure we have valid options and correct number of args
&checkOptions();
&usage(1) if (scalar(@ARGV) != 2);
($tDb, $qDb) = @ARGV;

# may be working on a 2bit file that does not have a database browser
$dbExists = 0;
$dbExists = 1 if (&HgAutomate::databaseExists($dbHost, $tDb));
# may be working with a query that has no database
$qDbExists = 0;
$qDbExists = 1 if (&HgAutomate::databaseExists($dbHost, $qDb));

$QDb = ucfirst($qDb);

$target2Bit = "$HgAutomate::clusterData/$tDb/$tDb.2bit";
$query2Bit = "$HgAutomate::clusterData/$qDb/$qDb.2bit";
$target2Bit = $opt_target2Bit if ($opt_target2Bit);
$query2Bit = $opt_query2Bit if ($opt_query2Bit);
$targetSizes = "$HgAutomate::clusterData/$tDb/chrom.sizes";
$querySizes = "$HgAutomate::clusterData/$qDb/chrom.sizes";
$targetSizes = $opt_targetSizes if ($opt_targetSizes);
$querySizes = $opt_querySizes if ($opt_querySizes);

$splitRef =  (`wc -l < $targetSizes` <= $HgAutomate::splitThreshold);

# Establish what directory we will work in.
if ($opt_buildDir) {
    $buildDir = $opt_buildDir;
} else {
$buildDir = "$HgAutomate::clusterData/$tDb/$HgAutomate::trackBuild/blastz.$qDb";
if (! -d $buildDir) {
$buildDir = "$HgAutomate::clusterData/$tDb/$HgAutomate::trackBuild/lastz.$qDb";
}
die "can not find existing build directory:\n$buildDir\n";
}

# Do everything.
$stepper->execute();

# Tell the user anything they should know.
my $stopStep = $stepper->getStopStep();
my $upThrough = ($stopStep eq 'download') ? "" :
  "  (through the '$stopStep' step)";

&HgAutomate::verbose(1,
	"\n *** All done!$upThrough\n");
&HgAutomate::verbose(1,
	" *** Steps were performed in $buildDir\n");
&HgAutomate::verbose(1, "\n");

