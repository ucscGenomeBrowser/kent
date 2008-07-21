#!/usr/bin/env perl

# DO NOT EDIT the /cluster/bin/scripts copy of this file --
# edit ~/kent/src/hg/utils/automation/doRecipBest.pl instead.

# This script should probably be folded back into doBlastzChainNet.pl
# eventually.

# $Id: doRecipBest.pl,v 1.11 2008/07/21 18:07:16 braney Exp $

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
    /;

# Specify the steps supported with -continue / -stop:
my $stepper = new HgStepManager(
    [ { name => 'recipBest',   func => \&doRecipBest },
      { name => 'download', func => \&doDownload },
    ]
				);

# Option defaults:
my $dbHost = 'hgwdev';

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
_EOF_
  ;
  print STDERR &HgAutomate::getCommonOptionHelp('dbHost' => $dbHost,
						'workhorse' => '');
  print STDERR "
Automates addition of reciprocal best chains/nets to regular chains/nets which
have already been created using doBlastzChainNet.pl.  Steps:
    recipBest: Net in both directions to get reciprocal best.
    download: Make a reciprocalBest subdir of the existing download dir.
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
my %defVars = ();
# Command line args: tDb qDb
my ($tDb, $qDb);
# Other:
my ($QDb);
my ($buildDir);
my ($splitRef);

sub checkOptions {
  # Make sure command line options are valid/supported.
  my $ok = GetOptions(@HgStepManager::optionSpec,
		      'buildDir=s',
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

  my $t2bit = "$HgAutomate::clusterData/$tDb/$tDb.2bit";
  my $q2bit = "$HgAutomate::clusterData/$qDb/$qDb.2bit";
  $bossScript->add(<<_EOF_
# Swap $tDb-best chains to be $qDb-referenced:
chainStitchId $tDb.$qDb.over.chain.gz stdout \\
| chainSwap stdin stdout \\
| chainSort stdin $qDb.$tDb.tBest.chain

# Net those on $qDb to get $qDb-ref'd reciprocal best net:
chainPreNet $qDb.$tDb.tBest.chain \\
  $HgAutomate::clusterData/{$qDb,$tDb}/chrom.sizes stdout \\
| chainNet -minSpace=1 -minScore=0 \\
  stdin $HgAutomate::clusterData/{$qDb,$tDb}/chrom.sizes stdout /dev/null \\
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
  $HgAutomate::clusterData/{$tDb,$qDb}/chrom.sizes stdout \\
| chainNet -minSpace=1 -minScore=0 \\
  stdin $HgAutomate::clusterData/{$tDb,$qDb}/chrom.sizes stdout /dev/null \\
| netSyntenic stdin stdout \\
| gzip -c > $tDb.$qDb.rbest.net.gz

# Clean up the one temp file and make md5sum:
rm $qDb.$tDb.tBest.chain
md5sum *.rbest.*.gz > md5sum.rbest.txt

# Create files for testing coverage of *.rbest.*.
netToBed -maxGap=1 $qDb.$tDb.rbest.net.gz $qDb.$tDb.rbest.net.bed
netToBed -maxGap=1 $tDb.$qDb.rbest.net.gz $tDb.$qDb.rbest.net.bed
chainToPsl $qDb.$tDb.rbest.chain.gz \\
  $HgAutomate::clusterData/{$qDb,$tDb}/chrom.sizes \\
  $q2bit $t2bit \\
  $qDb.$tDb.rbest.chain.psl
chainToPsl $tDb.$qDb.rbest.chain.gz \\
  $HgAutomate::clusterData/{$tDb,$qDb}/chrom.sizes \\
  $t2bit $q2bit \\
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
    $t2bit $q2bit stdout \\
    | axtSort stdin stdout \\
    | gzip -c > axtRBestNet/\$f:t:r.$tDb.$qDb.net.axt.gz
  end

# Make rbest mafNet for multiz: one .maf per $tDb seq.
mkdir mafRBestNet
foreach f (axtRBestNet/*.$tDb.$qDb.net.axt.gz)
    axtToMaf -tPrefix=$tDb. -qPrefix=$qDb. \$f \\
        $HgAutomate::clusterData/{$tDb,$qDb}/chrom.sizes \\
            stdout \\
      | gzip -c > mafRBestNet/\$f:t:r:r:r:r:r.maf.gz
end
_EOF_
    );
  } else {
  $bossScript->add(<<_EOF_
# Make rbest net axt's download
mkdir ../axtRBestNet
netToAxt $tDb.$qDb.rbest.net.gz $tDb.$qDb.rbest.chain.gz \\
    $t2bit $q2bit stdout \\
    | axtSort stdin stdout \\
    | gzip -c > ../axtRBestNet/$tDb.$qDb.rbest.axt.gz

# Make rbest mafNet for multiz
mkdir ../mafRBestNet
axtToMaf -tPrefix=$tDb. -qPrefix=$qDb. ../axtRBestNet/$tDb.$qDb.rbest.axt.gz \\
        $HgAutomate::clusterData/{$tDb,$qDb}/chrom.sizes  \\
                stdout \\
      | gzip -c > ../mafRBestNet/$tDb.$qDb.rbest.maf.gz
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
  my $fh = &HgAutomate::mustOpen(">$fname");
  print $fh <<_EOF_
This directory contains reciprocal-best netted chains for $tDb-$qDb.

 - $tDb.$qDb.rbest.net.gz: $tDb-referenced recip.best net to $qDb.
 - $tDb.$qDb.rbest.chain.gz: chains extracted from the recip.best
   net.  These can be passed to the liftOver program to translate coords
   from $tDb to $qDb through the recip.best net.

 - $qDb.$tDb.rbest.net.gz: $qDb-referenced recip.best net.
 - $qDb.$tDb.rbest.chain.gz: recip.best "liftOver" chains.

_EOF_
  ;
  close($fh);
} # makeRbestReadme

sub doDownload {
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
ln -s $buildDir/axtRBestNet/*.axt.gz axtRBestNet/
_EOF_
  );
  $bossScript->execute();
} # doDownload

#########################################################################
# main

#$opt_debug = 1;

# Prevent "Suspended (tty input)" hanging:
&HgAutomate::closeStdin();

# Make sure we have valid options and correct number of args
&checkOptions();
&usage(1) if (scalar(@ARGV) != 2);
($tDb, $qDb) = @ARGV;

$QDb = ucfirst($qDb);
$splitRef =  (`wc -l < $HgAutomate::clusterData/$tDb/chrom.sizes` 
                        <= $HgAutomate::splitThreshold);

# Establish what directory we will work in.
$buildDir = $opt_buildDir ? $opt_buildDir :
  "$HgAutomate::clusterData/$tDb/$HgAutomate::trackBuild/blastz.$qDb";

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

