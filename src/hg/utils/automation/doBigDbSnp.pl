#!/usr/bin/env perl

# DO NOT EDIT the /cluster/bin/scripts copy of this file --
# edit ~/kent/src/hg/utils/automation/doBigDbSnp.pl instead.

# Copyright (C) 2019 The Regents of the University of California

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
    $opt_assemblyList
    $opt_buildDir
    /;

# Specify the steps supported with -continue / -stop:
my $stepper = new HgStepManager(
    [ { name => 'split',   func => \&doSplit },
      { name => 'convert',   func => \&doConvert },
      { name => 'mergeToChrom',   func => \&doMergeToChrom },
      { name => 'mergeChroms',   func => \&doMergeChroms },
      { name => 'fixHg19ChrM',   func => \&doFixHg19ChrM },
      { name => 'check',   func => \&doCheck },
      { name => 'bigBed',   func => \&doBigBed },
      { name => 'install',   func => \&doInstall },
      { name => 'cleanup', func => \&doCleanup },
    ]
				);

# Files that must exist in $topDir:
my $refSeqToUcsc = 'refSeqToUcsc.tab';
my $equivRegions = 'equivRegions.tab';

# Option defaults:
my $assemblyList = 'GRCh37.p13,GRCh38.p13';
my $dbHost = 'hgwdev';
my $bigClusterHub = 'ku';
my $smallClusterHub = 'hgwdev';
my $workhorse = 'hgwdev';

my $outRoot = 'dbSnp';

my $base = $0;
$base =~ s/^(.*\/)?//;

sub usage {
  # Usage / help / self-documentation:
  my ($status, $detailed) = @_;
  # Basic help (for incorrect usage):
  print STDERR "
usage: $base topDir buildId freqSourceOrder
options:
";
  print STDERR $stepper->getOptionHelp();
  print STDERR <<_EOF_
    -assemblyList list    Comma-separated list of assemblies used by dbSNP
                          default: $assemblyList
    -buildDir dir         Use dir instead of default topDir/bigDbSnp.\$date
                          (necessary when continuing at a later date).
_EOF_
  ;
  print STDERR &HgAutomate::getCommonOptionHelp('dbHost' => $dbHost,
						'workhorse' => $workhorse,
						'fileServer' => '',
						'bigClusterHub' => $bigClusterHub,
						'smallClusterHub' => $smallClusterHub);
  print STDERR "
Convert dbSNP JSON into bigDbSnp and associated track files.

topDir is usually /hive/data/outside/dbSNP/NNN where NNN is 152 or greater.
topDir is expected to have a subdirectory json in which refsnp-*.json.bz2
files have already been downloaded, as well as files $refSeqToUcsc and $equivRegions
(see usage statement for dbSnpJsonToTab).

buildId is usually NNN where NNN is 152 or greater, same as topDir; it can also have a
suffix to distinguish it, e.g. 152Test.  The names of all result files contain $outRoot\$buildId.

freqSourceOrder is a comma-separated list of projects that submit frequency data to dbSNP
(see usage statement for dbSnpJsonToTab).

Steps:
    split: splits refsnp-*.json.bz2 files into chunks of 100,000 lines.
    convert: runs dbSnpJsonToTab on chunks.
    mergeToChrom: merges chunk result files into per-chrom results files.
    mergeChroms: merges per-chrom results files.
    fixHg19ChrM: if annotations on hg19 are included, then liftOver chrMT (NC_012920) to hg19 chrM.
    check: runs checkBigDbSnp to add ucscNotes about overlapping items and clustering anomalies.
    bigBed: Converts BED4+ .bigDbSnp files into bigBed.
    install: installs links to files in /gbdb.
    cleanup: Removes or compresses intermediate files.
All operations are performed in the build directory which is
topDir/bigDbSnp.\$date unless -buildDir is given.
";
  # Detailed help (-help):
  print STDERR "
Assumptions:
1. $HgAutomate::clusterData/\$db/\$db.2bit contains sequence for \$db.
2. topDir/json/ contains downloaded files refsnp-*.json.bz2
3. topDir/ contains files refSeqToUcsc.tab and equivRegions.tab - see dbSnpJsonToTab usage
" if ($detailed);
  print "\n";
  exit $status;
}


# Globals:
# Command line args: db
my ($topDir, $buildId, $freqSourceOrder);
# Other:
my ($buildDir, $jsonDir, @dbList, $secondsStart, $secondsEnd);

sub checkOptions {
  # Make sure command line options are valid/supported.
  my $ok = GetOptions(@HgStepManager::optionSpec,
                      'assemblyList=s',
		      'buildDir=s',
                      'buildId=s',
                      'freqSourceOrder=s',
		      @HgAutomate::commonOptionSpec,
		      );
  usage(1) if (!$ok);
  usage(0, 1) if ($opt_help);
  if ($opt_assemblyList) {
    $assemblyList= $opt_assemblyList;
  }
  # buildDir default depends on topDir (undetermined at this point) and is handled in main
  HgAutomate::processCommonOptions();
  my $err = $stepper->processOptions();
  usage(1) if ($err);
  $dbHost = $opt_dbHost if ($opt_dbHost);
}

sub grcToDb($) {
  # dbSNP is only ever going to produce JSON for various patch levels of GRCh38 and 37.
  my ($grc) = @_;
  my $db;
  if ($grc =~ /^GRCh38/) {
    $db = 'hg38';
  } elsif ($grc =~ /^GRCh37/) {
    $db = 'hg19';
  } else {
    die "Expected GRC assembly to start with 'GRCh37' or 'GRCh38' but got '$grc'";
  }
  return $db;
}

#########################################################################
# * step: split [smallCluster]
sub doSplit {
  my $runDir = "$buildDir/run.split";
  HgAutomate::mustMkdir($runDir);
  my $outDir = "$buildDir/split";
  HgAutomate::mustMkdir($outDir);

  my $splitScript = "$runDir/splitJson.sh";
  my $fh = HgAutomate::mustOpen(">$splitScript");
  print $fh <<EOF
#!/bin/bash
set -beEu -o pipefail
jsonIn=\$1
N=100000
prefix=$outDir/\$(basename \$jsonIn .json.bz2)

bzcat \$jsonIn | split -l \$N --filter='bzip2 > \$FILE.bz2' - \$prefix
EOF
    ;
  close($fh);
  system("chmod a+x $splitScript") == 0 || die "Unable to chmod $splitScript";
  HgAutomate::makeGsub($runDir, "$splitScript {check in exists+ \$(path1)}");

  my $whatItDoes = "It splits per-chrom JSON files into 100,000 line chunks.";
  my $bossScript = new HgRemoteScript("$runDir/doSplit.csh", $smallClusterHub,
				      $runDir, $whatItDoes);
  my $paraRun = HgAutomate::paraRun();
  my $gensub2 = HgAutomate::gensub2();
  $bossScript->add(<<_EOF_
ls -1S $jsonDir/refsnp-{chr*,other}.json.bz2 > jsonList
$gensub2 jsonList single gsub jobList
$paraRun
_EOF_
    );
  $bossScript->execute();
} # doSplit


#########################################################################
# * step: convert [bigClusterHub]
sub doConvert {
  my $runDir = "$buildDir/run.convert";
  HgAutomate::mustMkdir($runDir);
  my $outDir = "$buildDir/splitProcessed";
  HgAutomate::mustMkdir($outDir);

  my $convertScript = "$runDir/jsonToTab.sh";
  my $fh = HgAutomate::mustOpen(">$convertScript");
  print $fh <<EOF
#!/bin/bash
set -beEu -o pipefail
# jsonIn needs to be absolute path
jsonIn=\$1

tmpDir=\$(mktemp -d /dev/shm/dbSnpJsonToTab.XXXXXXXX)
pushd \$tmpDir

outRoot=\$(basename \$jsonIn .bz2)
chromOutDir=$outDir/\$(echo \$outRoot | sed -e 's/..\$//;')

bzcat \$jsonIn \\
| dbSnpJsonToTab -freqSourceOrder=$freqSourceOrder \\
    -equivRegions=$topDir/$equivRegions \\
    $assemblyList $topDir/$refSeqToUcsc stdin \$outRoot

# For sorting.  I expected that this would be set already from my shell, but apparently not:
export LC_COLLATE=C

# Discard the last two bigDbSnp columns -- they only have 0s.  The real values will be added
# later by bedJoinTabOffset.
EOF
    ;
  foreach my $grc (split(',', $assemblyList)) {
    my $db = grcToDb($grc);
    print $fh <<EOF
cut -f1-15  \$outRoot.$grc.bigDbSnp \\
| sort -k1,1 -k2n,2n \\
| bzip2 \\
  > \$outRoot.$db.sorted.bigDbSnp.bz2
sort -k1,1 -k2n,2n \$outRoot.$grc.badCoords.bed \\
| bzip2 \\
  > \$outRoot.$db.sorted.badCoords.bed.bz2
EOF
      ;
  }
  print $fh <<EOF

sort \${outRoot}Details.tab | bzip2 > \${outRoot}Details.tab.bz2
sort \${outRoot}Errors.tab | bzip2 > \${outRoot}Errors.tab.bz2
sort \${outRoot}Merged.tab | bzip2 > \${outRoot}Merged.tab.bz2
sort \${outRoot}Warnings.tab | bzip2 > \${outRoot}Warnings.tab.bz2

popd

mkdir -p \$chromOutDir
cp -p \$tmpDir/\$outRoot*.bz2 \$chromOutDir/

rm -rf \$tmpDir
EOF
    ;
  close($fh);
  system("chmod a+x $convertScript") == 0 || die "Unable to chmod $convertScript";

  my $whatItDoes = "It converts dbSNP JSON to bigDbSnp, dbSnpDetails and other files.";
  my $bossScript = new HgRemoteScript("$runDir/doConvert.csh", $bigClusterHub,
				      $runDir, $whatItDoes);

  HgAutomate::makeGsub($runDir, "$convertScript {check in exists+ \$(path1)}");
  my $paraRun = HgAutomate::paraRun();
  my $gensub2 = HgAutomate::gensub2();
  $bossScript->add(<<_EOF_
ls -1S $buildDir/split/ref*.bz2 > splitList
$gensub2 splitList single gsub jobList
$paraRun
_EOF_
  );
  $bossScript->execute();
} # doConvert


#########################################################################
# * step: mergeToChrom [smallClusterHub]
sub doMergeToChrom {
  my $runDir = "$buildDir/run.mergeToChrom";
  HgAutomate::mustMkdir($runDir);
  my $outDir = "$buildDir/mergedToChrom";
  HgAutomate::mustMkdir($outDir);

  my $sortMergeBzBedScript = "$runDir/sortMergeBzBed.sh";
  my $fh = HgAutomate::mustOpen(">$sortMergeBzBedScript");
  print $fh <<EOF
#!/bin/bash
set -beEu -o pipefail
bzBedList=\$1
outFile=\$2

tmpDir=\$(mktemp -d /dev/shm/dbSnpMergeSortBed.XXXXXXXX)
pushd \$tmpDir

cp /dev/null bedList
for bz in \$(cat \$bzBedList); do
    bed=\$(basename \$bz .bz2)
    bzcat \$bz > \$bed
    echo \$bed >> bedList
done

export LC_COLLATE=C
sort --merge -k1,1 -k2n,2n \$(cat bedList) > \$outFile

popd
rm -rf \$tmpDir
EOF
    ;
  close($fh);
  system("chmod a+x $sortMergeBzBedScript") == 0 || die "Unable to chmod $sortMergeBzBedScript";

  my $sortMergeBzScript = "$runDir/sortMergeBz.sh";
  $fh = HgAutomate::mustOpen(">$sortMergeBzScript");
  print $fh <<EOF
#!/bin/bash
set -beEu -o pipefail
bzList=\$1
outFile=\$2

tmpDir=\$(mktemp -d /dev/shm/dbSnpMergeSort.XXXXXXXX)
pushd \$tmpDir

cp /dev/null txtList
for bz in \$(cat \$bzList); do
    txt=\$(basename \$bz .bz2)
    bzcat \$bz > \$txt
    echo \$txt >> txtList
done

export LC_COLLATE=C
sort --merge -u \$(cat txtList) > \$outFile

popd
rm -rf \$tmpDir
EOF
    ;
  close($fh);
  system("chmod a+x $sortMergeBzScript") == 0 || die "Unable to chmod $sortMergeBzScript";

  my $whatItDoes = "It merge-sorts the results from split-up JSON files into per-chromosome files.";
  my $bossScript = newBash HgRemoteScript("$runDir/doMergeToChrom.sh", $smallClusterHub,
                                          $runDir, $whatItDoes);

  my $paraRun = HgAutomate::paraRun();
  $bossScript->add(<<_EOF_
# One merge per "chrom" per type of dbSnpJsonToTab output
for jsonFile in \$(ls -1S $jsonDir/refsnp-{chr*,other}.json.bz2); do
    prefix=\$(basename \$jsonFile .json.bz2)
    echo \$prefix
_EOF_
                  );
  foreach my $db (@dbList) {
    $bossScript->add(<<_EOF_
    ls -1S $buildDir/splitProcessed/\$prefix/\$prefix??.$db.*bigDbSnp* > \$prefix.$db.bigDbSnp.list
    ls -1S $buildDir/splitProcessed/\$prefix/\$prefix??.$db.*badCoords* > \$prefix.$db.badCoords.list
_EOF_
                    );
  }
  my $dbListStr = join(',', @dbList);
  $bossScript->add(<<_EOF_
    ls -1S $buildDir/splitProcessed/\$prefix/\$prefix??Details.* > \$prefix.details.list
    ls -1S $buildDir/splitProcessed/\$prefix/\$prefix??Errors.* > \$prefix.errors.list
    ls -1S $buildDir/splitProcessed/\$prefix/\$prefix??Merged.* > \$prefix.merged.list
    ls -1S $buildDir/splitProcessed/\$prefix/\$prefix??Warnings.* > \$prefix.warnings.list
done

cp /dev/null jobList
for list in *.{$dbListStr}.bigDbSnp.list; do
    prefix=\$(basename \$list .bigDbSnp.list)
    echo "./sortMergeBzBed.sh {check in line+ \$PWD/\$list} {check out line+ $outDir/\$prefix.bigDbSnp}" >> jobList
done
for list in *.details.list; do
    prefix=\$(basename \$list .list)
    echo "./sortMergeBz.sh {check in line+ \$PWD/\$list} {check out line+ $outDir/\$prefix.tab}" >> jobList
done
# OK for these to be empty (check out line instead of line+):
for list in *.{$dbListStr}.badCoords.list; do
    prefix=\$(basename \$list .badCoords.list)
    echo "./sortMergeBzBed.sh {check in line+ \$PWD/\$list} {check out line $outDir/\$prefix.badCoords.bed}" >> jobList
done
for list in *.errors.list *.merged.list *.warnings.list; do
    prefix=\$(basename \$list .list)
    echo "./sortMergeBz.sh {check in line+ \$PWD/\$list} {check out line $outDir/\$prefix.tab}" >> jobList
done
$paraRun;
_EOF_
  );
  $bossScript->execute();
} # doMergeToChrom


#########################################################################
# * step: mergeChroms [workhorse]
sub doMergeChroms {
  my $runDir = $buildDir;
  my $inDir = "mergedToChrom";
  HgAutomate::mustMkdir("$runDir/joined");

  my $whatItDoes = "It merges chrom-level result files.";
  my $bossScript = newBash HgRemoteScript("$runDir/doMergeChroms.sh", $workhorse,
                                          $runDir, $whatItDoes);

  $bossScript->add(<<_EOF_
# Merge all chroms' *Merged.tab to the final Merged.tab file in background,
# likewise for errors, warnings, and badCoords which should all be relatively small and quick.
pids=""
time sort --merge -u $inDir/*.merged.tab > ${outRoot}Merged.tab &
pids+=" \$!"
time sort --merge -u $inDir/*.errors.tab > ${outRoot}Errors.tab &
pids+=" \$!"
time sort --merge -u $inDir/*.warnings.tab > ${outRoot}Warnings.tab &
pids+=" \$!"
_EOF_
  );
  foreach my $db (@dbList) {
    $bossScript->add(<<_EOF_
(time sort --merge -k1,1 -k2n,2n $inDir/*.$db.badCoords.bed | uniq > $db.$outRoot.badCoords.bed) &
pids+=" \$!"
_EOF_
                    );
  }
  $bossScript->add(<<_EOF_

# Merge all chroms' *Details.tab to the final Details.tab file
time sort --merge -u $inDir/*.details.tab > ${outRoot}Details.tab

for pid in \$pids; do
  if wait \$pid; then
    echo pid \$pid done
  else
    echo pid \$pid FAILED
    exit 1
  fi
done

# Compress & index Details.tab with bgzip in background.  Leave original file uncompressed for
# bedJoinTabOffset.
time bgzip -iI ${outRoot}Details.tab.gz.gzi -c ${outRoot}Details.tab > ${outRoot}Details.tab.gz &
pids=\$!

# parallel job of bedJoinTabOffset on each chrom's .bigDbSnp and ${outRoot}Details.tab
# bedJoinTabOffset builds a massive hash in memory (file offsets of >650M lines of Details),
# so limit the number of concurrent processes to 10.
time (ls -1S $inDir/refsnp-*.*.bigDbSnp |
      parallel --max-procs 10 --ungroup \\
        bedJoinTabOffset -verbose=2 ${outRoot}Details.tab {} joined/{/})

# Now mergeSort all chrom's data together.  Don't use sort -u because with -k it only
# compares keys, not the whole line.
_EOF_
  );
  foreach my $db (@dbList) {
    $bossScript->add(<<_EOF_
(time sort --merge -k1,1 -k2n,2n joined/*.$db.bigDbSnp | uniq > $db.$outRoot.bigDbSnp) &
pids+=" \$!"
_EOF_
                    );
  }
  $bossScript->add(<<_EOF_
for pid in \$pids; do
  if wait \$pid; then
    echo pid \$pid done
  else
    echo pid \$pid FAILED
    exit 1
  fi
done
_EOF_
                  );
  $bossScript->execute();
} # doMergeChroms


#########################################################################
# * step: fixHg19ChrM [workhorse]
sub doFixHg19ChrM {
  my $runDir = $buildDir;
  if (grep(/hg19/, @dbList)) {
    my $whatItDoes = "It does a liftOver from chrMT (old name NC_012920) to hg19 chrM.";
    my $bossScript = newBash HgRemoteScript("$runDir/doFixHg19ChrM.sh", $workhorse,
                                            $runDir, $whatItDoes);
    $bossScript->add(<<_EOF_
# For hg19, liftOver chrMT annotations to hg19 chrM.
sed -e 's/NC_012920 /chrMT /' \\
  /hive/data/outside/dbSNP/131/human/NC_012920ToChrM.over.chain \\
  > hg19.mitoLiftover.chain
# For liftOver, convert 0-base fully-closed to 0-based half-open because liftOver
# doesn't deal with 0-base items.
mv hg19.$outRoot.bigDbSnp hg19.preChrMFix.$outRoot.bigDbSnp
time (grep ^chrMT hg19.preChrMFix.$outRoot.bigDbSnp \\
      | awk -F"\t" 'BEGIN{OFS="\t";} {\$3 += 1; print;}' \\
      | liftOver -tab -bedPlus=3 stdin \\
          hg19.mitoLiftover.chain stdout chrM.unmapped \\
      | awk -F"\t" 'BEGIN{OFS="\t";} {\$3 -= 1; print;}' \\
      | sort -k2n,2n \\
        > hg19.chrM.$outRoot.bigDbSnp)
wc -l hg19.chrM.$outRoot.bigDbSnp chrM.unmapped
time grep -v ^chrMT hg19.preChrMFix.$outRoot.bigDbSnp \\
     | sort --merge -k1,1 -k2n,2n - hg19.chrM.$outRoot.bigDbSnp \\
       > hg19.$outRoot.bigDbSnp
_EOF_
                    );
    $bossScript->execute()
  };
} # doFixHg19ChrM


#########################################################################
# * step: check [workhorse]
sub doCheck {
  my $runDir = $buildDir;

  my $whatItDoes = "It runs checkBigDbSnp on merged bigDbSnp files.";
  my $bossScript = newBash HgRemoteScript("$runDir/doCheck.sh", $workhorse,
                                          $runDir, $whatItDoes);

  foreach my $db (@dbList) {
    $bossScript->add(<<_EOF_
cut -f 4 $db.$outRoot.badCoords.bed | sort -u > $db.badCoords.ids.txt
_EOF_
                    );
  }
  $bossScript->add(<<_EOF_
pids=""
_EOF_
                  );
  foreach my $db (@dbList) {
    $bossScript->add(<<_EOF_
time checkBigDbSnp -mapErrIds=$db.badCoords.ids.txt \\
       $db.$outRoot.bigDbSnp $HgAutomate::clusterData/$db/$db.2bit $db.$outRoot.checked.bigDbSnp &
echo \$!
pids+=" \$!"
_EOF_
                    );
  }
  $bossScript->add(<<_EOF_
for pid in \$pids; do
  if wait \$pid; then
    echo pid \$pid done
  else
    echo pid \$pid FAILED
    exit 1
  fi
done
_EOF_
                  );
  $bossScript->execute();
} # doCheck


#########################################################################
# * step: bigBed [workhorse]
sub doBigBed {
  my $runDir = $buildDir;

  # Helper script to make Mult, Common and ClinVar subsets and convert to bigBed for one db.
  my $makeSubsetsScript = "$runDir/makeSubsets.sh";
  my $fh = HgAutomate::mustOpen(">$makeSubsetsScript");
  print $fh <<_EOF_
#!/bin/bash
set -beEu -o pipefail
db=\$1
time $Bin/categorizeBigDbSnp.pl \$db \$db.$outRoot.checked.bigDbSnp
pids=""
for subset in Mult Common ClinVar; do
  time bedToBigBed -tab -as=\$HOME/kent/src/hg/lib/bigDbSnp.as -type=bed4+ -extraIndex=name \\
            \$db.\$subset.bigDbSnp /hive/data/genomes/\$db/chrom.sizes \$db.$outRoot.\$subset.bb &
  pids+=" \$!";
done
for pid in \$pids; do
  if wait \$pid; then
    echo pid \$pid done
  else
    echo pid \$pid FAILED
    exit 1
  fi
done
_EOF_
    ;
  close($fh);
  system("chmod a+x $makeSubsetsScript") == 0 || die "Unable to chmod $makeSubsetsScript";

  my $whatItDoes = "It runs bedToBigBed on merged & checked bigDbSnp files and makes ".
        "Mult, Common and ClinVar subsets.";
  my $bossScript = newBash HgRemoteScript("$runDir/doBigBed.sh", $workhorse,
                                          $runDir, $whatItDoes);

  $bossScript->add(<<_EOF_
pids=""
_EOF_
                  );
  # Increase max memory allocation from default (on 64-bit machines) of 16GB (exceeded b154):
  my $maxAlloc = 64 * 1024 * 1024 * 1024;
  foreach my $db (@dbList) {
    $bossScript->add(<<_EOF_
time bedToBigBed -tab -as=\$HOME/kent/src/hg/lib/bigDbSnp.as -type=bed4+ -extraIndex=name \\
            -maxAlloc=$maxAlloc \\
            $db.$outRoot.checked.bigDbSnp /hive/data/genomes/$db/chrom.sizes $db.$outRoot.bb &
pids+=" \$!"
time bedToBigBed -tab -type=bed4 -extraIndex=name \\
            $db.$outRoot.badCoords.bed /hive/data/genomes/$db/chrom.sizes $db.${outRoot}BadCoords.bb &
pids+=" \$!"
$makeSubsetsScript $db &
pids+=" \$!"
_EOF_
                    );
  }
  $bossScript->add(<<_EOF_
for pid in \$pids; do
  if wait \$pid; then
    echo pid \$pid done
  else
    echo pid \$pid FAILED
    exit 1
  fi
done
_EOF_
                  );
  $bossScript->execute();
} # doBigBed


#########################################################################
# * step: install [dbHost]
sub doInstall {
  my $runDir = $buildDir;

  my $whatItDoes = "It installs files in /gbdb.";
  my $bossScript = newBash HgRemoteScript("$runDir/doInstall.sh", $workhorse,
				      $runDir, $whatItDoes);

  foreach my $db (@dbList) {
    $bossScript->add(<<_EOF_
ln -sf $buildDir/$db.$outRoot.bb /gbdb/$db/snp/$outRoot.bb
for subset in Mult Common ClinVar; do
  ln -sf $buildDir/$db.$outRoot.\$subset.bb /gbdb/$db/snp/${outRoot}\$subset.bb
done
ln -sf $buildDir/$db.${outRoot}BadCoords.bb /gbdb/$db/snp/${outRoot}BadCoords.bb
_EOF_
                    );
  }
  $bossScript->add(<<_EOF_
mkdir -p /gbdb/hgFixed/dbSnp
ln -sf $buildDir/${outRoot}Details.tab* /gbdb/hgFixed/dbSnp/
_EOF_
  );
  $bossScript->execute();
} # doInstall


#########################################################################
# * step: cleanup [workhorse]
sub doCleanup {
  my $runDir = "$buildDir";
  my $whatItDoes = "It cleans up or compresses intermediate files.";
  my $bossScript = new HgRemoteScript("$runDir/doCleanup.csh", $workhorse,
				      $runDir, $whatItDoes);
  $bossScript->add(<<_EOF_
bzip2 *.bigDbSnp
rm -rf merged splitProcessed joined
_EOF_
  );
  $bossScript->execute();
} # doCleanup


#########################################################################
# main

# Prevent "Suspended (tty input)" hanging:
HgAutomate::closeStdin();

# Make sure we have valid options and exactly 1 argument:
checkOptions();
usage(1) if (scalar(@ARGV) != 3);
$secondsStart = `date "+%s"`;
chomp $secondsStart;
($topDir, $buildId, $freqSourceOrder) = @ARGV;

# Establish what directory we will work in.
my $date = `date +%Y-%m-%d`;
chomp $date;
$buildDir = $opt_buildDir ? $opt_buildDir : "$topDir/bigDbSnp.$date";

$outRoot .= $buildId;
$jsonDir = "$topDir/json";
@dbList = map { grcToDb($_); } split(',', $assemblyList);

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

HgAutomate::verbose(1,
	"\n *** All done !$upThrough  Elapsed time: ${elapsedMinutes}m${elapsedSeconds}s\n");
HgAutomate::verbose(1,
	" *** Steps were performed in $buildDir\n");
HgAutomate::verbose(1, "\n");

