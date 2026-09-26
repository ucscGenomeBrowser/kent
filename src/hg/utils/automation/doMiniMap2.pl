#!/usr/bin/env perl

# DO NOT EDIT the /cluster/bin/scripts copy of this file --
# edit ~/kent/src/hg/utils/automation/doMiniMap2.pl instead.

# doMiniMap2.pl -- same-species / same-haplotype liftOver pipeline,
# modeled directly on doSameSpeciesLiftOver.pl, but using minimap2
# instead of blat as the alignment engine.
#
# Intended use: two very similar same-species assemblies where blat's
# -fastMap query-chunking dance isn't needed and minimap2's splice-free
# asm5/asm10/asm20 presets do a better job with the larger indels and
# structural differences you see between the two haplotypes of a
# diploid (trio-binned or hifiasm/verkko dual-assembly) genome, or
# between two closely related strain assemblies.
#
# Requires the kent command pafToPsl (PAF+cigar -> PSL) so the existing
# axtChain/chainNet/netChainSubset toolchain can be reused unchanged.

use Getopt::Long;
use warnings;
use strict;
use FindBin qw($Bin);
use lib "$Bin";
use HgAutomate;
use HgRemoteScript;
use HgStepManager;
use AssemblyDivergence qw(mashDistance choosePipeline
			   $mashAsm5Max $mashAsm10Max $mashLastzMin $mashWarnMax);

# Option variable names, both common and peculiar to this script:
use vars @HgAutomate::commonOptionVars;
use vars @HgStepManager::optionVars;
use vars qw/
    $opt_buildDir
    $opt_target2Bit
    $opt_targetSizes
    $opt_query2Bit
    $opt_querySizes
    $opt_minimapPreset
    $opt_minimapCpu
    $opt_chainRam
    $opt_chainCpu
    $opt_regenerateMash
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
my $ramG = '32g';	# minimap2 index + 8 threads on a whole genome needs headroom
my $cpu = 1;
# minimapPreset is normally left undef and picked automatically by
# estimateDivergence() (mash distance -> asm5/asm10/asm20); -minimapPreset
# overrides that and skips the mash run entirely.
my $minimapPreset;
my $minimapCpu = 8;		# -t N threads given to each minimap2 job
my $chainRam = '16g';		# -chainRam=Ng argument
my $chainCpu = 1;		# -chainCpu=N argument
# mash distance thresholds ($mashAsm5Max/$mashAsm10Max/$mashLastzMin/
# $mashWarnMax) used to pick a preset (or reject the pair outright) come
# from AssemblyDivergence.pm, shared with mashDistance.pl and anything
# else that needs to triage a pair between this pipeline and
# doBlastzChainNet.pl.

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
                          $HgAutomate::clusterData/\$fromDb/$HgAutomate::trackBuild/mm2.\$toDb.\$date
                          (necessary when continuing at a later date).
    -target2Bit /path/target.2bit  Full path to target sequence (fromDb)
    -query2Bit /path/query.2bit    Full path to query sequence (toDb)
    -targetSizes /path/target.chrom.sizes  Full path to target chrom.sizes (fromDb)
    -querySizes  /path/query.chrom.sizes   Full path to query chrom.sizes (toDb)
    -minimapPreset asm5|asm10|asm20  minimap2 -x preset to use.  By default
                          this is chosen automatically via AssemblyDivergence.pm
                          (same logic as the standalone mashDistance.pl): mash
                          sketches \$fromDb and \$toDb and picks a preset from
                          the mash distance between them (< $mashAsm5Max -> asm5,
                          < $mashAsm10Max -> asm10, < $mashLastzMin -> asm20).
                          At or above $mashLastzMin this script refuses to run
                          -- that pair looks too diverged for minimap2's asm*
                          presets; use doBlastzChainNet.pl instead, or give
                          -minimapPreset to skip the mash run and force a
                          specific preset anyway.
    -minimapCpu N         Threads given to each minimap2 cluster job (-t N),
                          default: $minimapCpu
    -chainRam  Ng  Cluster ram size for chain step, default: -chainRam=$chainRam
    -chainCpu  N   Cluster CPUs number for chain step, default: -chainCpu=$chainCpu
    -regenerateMash       Force the mash divergence check to re-sketch and
                          overwrite even if a cached .msh (GenArk mashSketch/
                          cache or \$buildDir/mashDistance.txt) already
                          exists.  Ignored if -minimapPreset is also given,
                          since that skips the mash run entirely.
_EOF_
  ;
  print STDERR &HgAutomate::getCommonOptionHelp('dbHost' => $dbHost,
						'workhorse' => '',
						'fileServer' => '',
						'ram' => $ramG,
						'cpu' => $cpu,
						'bigClusterHub' => '');
  print STDERR "
Automates a same-species/same-haplotype liftOver (minimap2/chain/net)
pipeline, patterned after doSameSpeciesLiftOver.pl but using minimap2
in place of blat -fastMap:
    align: Aligns the assemblies using minimap2 -cx \$minimapPreset on a
           big cluster, one job per target sequence against the full
           query genome, then converts PAF+cigar to PSL with pafToPsl.
    chain: Chains the alignments on a big cluster.
    net:   Nets the alignments, uses netChainSubset to extract liftOver chains.
    load:  Installs liftOver chain files, calls hgAddLiftOverChain on $dbHost.
    cleanup: Removes or compresses intermediate files.
All operations are performed in the build directory which is
$HgAutomate::clusterData/\$fromDb/$HgAutomate::trackBuild/mm2.\$toDb.\$date unless -buildDir is given.
";
  # Detailed help (-help):
  print STDERR "
Assumptions:
1. /scratch/data/\$db/\$db.2bit contains sequence for database/assembly \$db.
2. $HgAutomate::clusterData/\$db/chrom.sizes contains all sequence names and sizes from
   \$db.2bit.
3. The \$db.2bit files have already been distributed to cluster-scratch
   (/scratch/data/<db>/).
4. pafToPsl is on \$PATH on the machine that runs the align cluster jobs.
5. mash is on \$PATH on the machine that runs this script (used once, up
   front, to auto-select -minimapPreset unless it is given explicitly).
6. fromDb and toDb are two haplotypes/assemblies of the same species/
   individual -- this is not a general any-vs-any pipeline.  For that,
   use doBlastzChainNet.pl.
" if ($detailed);
  print "\n";
  exit $status;
}


# Globals:
# Command line args: tDb=fromDb, qDb=toDb
my ($tDb, $qDb);

# Other:
my ($buildDir);
my ($tSeq, $tSizes, $qSeq, $qSizes, $QDb, $fileServer);
my ($liftOverChainDir, $liftOverChainFile, $liftOverChainPath, $dbExists);

sub checkOptions {
  # Make sure command line options are valid/supported.
  my $ok = GetOptions(@HgStepManager::optionSpec,
		      'buildDir=s',
		      'target2Bit=s',
		      'targetSizes=s',
		      'query2Bit=s',
		      'querySizes=s',
		      'minimapPreset=s',
		      'minimapCpu=i',
		      'chainRam=s',
		      'chainCpu=i',
		      'regenerateMash',
		      @HgAutomate::commonOptionSpec,
		      );
  &usage(1) if (!$ok);
  &usage(0, 1) if ($opt_help);
  &HgAutomate::processCommonOptions();
  my $err = $stepper->processOptions();
  usage(1) if ($err);
  $dbHost = $opt_dbHost if ($opt_dbHost);
  if ($opt_minimapPreset) {
    $minimapPreset = $opt_minimapPreset;
    if ($minimapPreset !~ /^asm(5|10|20)$/) {
      die "-minimapPreset must be one of asm5, asm10, asm20 (got '$minimapPreset')\n";
    }
  }
  # else: leave $minimapPreset undef -- estimateDivergence() will set it
  # from a mash distance once $tSeq/$qSeq/$buildDir are known.
}


sub getClusterSeqs {
  # Choose cluster and look for already-installed 2bit files on appropriate
  # cluster-scratch storage.  Exit with an error message if we can't find them.
  my $paraHub = $opt_bigClusterHub ? $opt_bigClusterHub :
    &HgAutomate::chooseClusterByBandwidth();
  my ($tSeqScratch, $qSeqScratch);
  if ($opt_target2Bit) {
    $tSeqScratch = $opt_target2Bit
  } else {
    my @okFilesystems =
      &HgAutomate::chooseFilesystemsForCluster($paraHub, 'in');
    if ( -e "/scratch/data/$tDb/$tDb.2bit" ) {
        $tSeqScratch = "/scratch/data/$tDb/$tDb.2bit";
    } else {
      foreach my $fs (@okFilesystems) {
	&HgAutomate::verbose(1, "checking $fs/$tDb/$tDb.2bit\n");
        if (&HgAutomate::machineHasFile($paraHub, "$fs/$tDb/$tDb.2bit")) {
          $tSeqScratch = "$fs/$tDb/$tDb.2bit";
          last;
        }
      }
    }
    if (! defined $tSeqScratch) {
     die "align: can't find $tDb/$tDb.2bit in " .
       join("/, ", @okFilesystems) . "/ -- please distribute.\n";
    }
  }

  if ($opt_query2Bit) {
        $qSeqScratch = $opt_query2Bit;
  } else {
    my @okFilesystems =
      &HgAutomate::chooseFilesystemsForCluster($paraHub, 'in');
    if ( -e "/scratch/data/$qDb/$qDb.2bit" ) {
        $qSeqScratch = "/scratch/data/$qDb/$qDb.2bit";
    } else {
      foreach my $fs (@okFilesystems) {
        if (&HgAutomate::machineHasFile($paraHub, "$fs/$qDb/$qDb.2bit")) {
          $qSeqScratch = "$fs/$qDb/$qDb.2bit";
          last;
        }
      }
    }
    if (! defined $qSeqScratch) {
      die "align: can't find $qDb/$qDb.2bit in " .
        join("/, ", @okFilesystems) . "/ -- please distribute.\n";
    }
  }
  &HgAutomate::verbose(1, "Using $paraHub, $tSeqScratch and $qSeqScratch\n");
  return ($paraHub, $tSeqScratch, $qSeqScratch);
} # getClusterSeqs



#########################################################################
# * step: align [bigClusterHub]
sub doAlign {
  my $runDir = "$buildDir/run.mm2";
  &HgAutomate::mustMkdir($runDir);

  my $pafDir = "$runDir/paf";
  &HgAutomate::checkCleanSlate('align', 'chain', $pafDir, 'run.time');

  my ($paraHub, $tSeqScratch, $qSeqScratch) = &getClusterSeqs();

  # Unlike blat -fastMap, minimap2 does not need the query pre-split into
  # small chunks or an .ooc repeat mask -- it indexes and aligns whole
  # sequences directly.  We only partition on the target side, one job
  # per target sequence (same trick doSameSpeciesLiftOver.pl uses: pick a
  # chunkSize at least as large as the biggest target sequence so nothing
  # actually gets split), and align each target job against the *whole*
  # query genome in one shot.

  # script for a single job: convert target 2bit spec(s) to fasta, align
  # against the pre-built whole-query fasta, convert PAF -> PSL.
  # NOTE: partitionSequence.pl may bundle several small target sequences
  # into one $runDir/tParts/partNNN.lst file instead of handing us a bare
  # 2bit spec directly (it does this whenever a genome has lots of tiny
  # scaffolds -- see its own usage message) -- path1 in that case is a
  # *relative* path like "tParts/part010.lst", so it has to be resolved
  # against $runDir before we cd elsewhere, and expanded into one
  # twoBitToFa call per line rather than treated as a single 2bit spec.
  my $fh = &HgAutomate::mustOpen(">$runDir/job.sh");
  print $fh <<_EOF_
#!/bin/bash
set -beEu -o pipefail

targetSpec=\$1
outPsl=\$2

if [[ "\$targetSpec" == *.lst ]]; then
  targetSpec="$runDir/\$targetSpec"
fi

unset TMPDIR
if [ -d "/data/tmp" ]; then
  export TMPDIR="/data/tmp"
elif [ -d "/scratch/tmp" ]; then
  export TMPDIR="/scratch/tmp"
else
  tmpSz=`df --output=avail -k /tmp | tail -1`
  shmSz=`df --output=avail -k /dev/shm | tail -1`
  if [ "\$shmSz" -gt "\$tmpSz" ]; then
    mkdir -p /dev/shm/tmp
    chmod 777 /dev/shm/tmp
    export TMPDIR="/dev/shm/tmp"
  else
    export TMPDIR="/tmp"
  fi
fi

# Use local disk for output, and move the final result to \$outPsl
# when done, to minimize I/O.
tmpDir=`mktemp -d -p \$TMPDIR doMm2.XXXXXX`
pushd \$tmpDir > /dev/null

: > target.fa
if [[ "\$targetSpec" == *.lst ]]; then
  while read -r spec; do
    twoBitToFa "\$spec" stdout
  done < "\$targetSpec" > target.fa
else
  twoBitToFa \$targetSpec target.fa
fi

minimap2 -cx $minimapPreset --secondary=no -t $minimapCpu \\
    target.fa $runDir/query.fa > tmpOut.paf

pafToPsl -tSizes=$tSizes -qSizes=$qSizes tmpOut.paf tmpOut.psl

mv tmpOut.psl \$outPsl

popd > /dev/null
rm -rf \$tmpDir
_EOF_
  ;
  close($fh);
  &HgAutomate::run("chmod a+x $runDir/job.sh");

  &HgAutomate::makeGsub($runDir,
			'job.sh $(path1) {check out line ' .
			 $pafDir . '/$(file1).psl}');

  my $paraRun = &HgAutomate::paraRun($ramG, $minimapCpu);
  my $whatItDoes = "It performs a cluster run of minimap2 -cx $minimapPreset.";
  my $bossScript = newBash HgRemoteScript("$runDir/doAlign.bash", $paraHub,
				      $runDir, $whatItDoes);

  # Don't allow target sequences to be split -- we align (and chain) whole
  # target sequences against the whole query, we never lift target coords
  # back up.  Use the max target seq size as the chunkSize for
  # partitionSequence.pl on the target.
  my $tpSize = `awk '{print \$2;}' $tSizes | sort -nr | head -1`;
  chomp $tpSize;
  my $minTpSize = 10000000;
  $tpSize = $minTpSize if ($tpSize < $minTpSize);

  my $gensub2 = &HgAutomate::gensub2();
  $bossScript->add(<<_EOF_
# Convert the whole query 2bit to fasta once; every target job aligns
# against this same file.
twoBitToFa $qSeqScratch query.fa

# Compute partition (coordinate ranges) for cluster job.  This does
# not need to be run on the build fileserver because it does not actually
# split any sequences -- it merely computes ranges based on the chrom.sizes.
rm -rf tParts
$Bin/partitionSequence.pl $tpSize 0 $tSeqScratch \\
   $tSizes 2000 \\
  -lstDir=tParts > t.lst

mkdir $pafDir

$gensub2 t.lst single gsub jobList

$paraRun
_EOF_
  );
  $bossScript->execute();
} # doAlign


#########################################################################
# * step: chain [smallClusterHub]

sub doChain {
  my $runDir = "$buildDir/run.chain";
  &HgAutomate::mustMkdir($runDir);

  my $pafDir = "$buildDir/run.mm2/paf";
  my $mm2DoneFile = "$buildDir/run.mm2/run.time";
  &HgAutomate::checkCleanSlate('chain', 'net', 'chainRaw');
  &HgAutomate::checkExistsUnlessDebug('align', 'chain',
				      $pafDir, $mm2DoneFile);

  my ($paraHub, $tSeqScratch, $qSeqScratch) = &getClusterSeqs();

  # One PSL per target sequence already (align step did not split further),
  # so the chain job list is simply the contents of $pafDir/*.psl.
  &HgAutomate::run("ls $pafDir/*.psl | xargs -n 1 basename > $runDir/pslParts.lst");

  # script for a single job: chain one target-sequence's PSL.
  my $fh = &HgAutomate::mustOpen(">$runDir/job.sh");
  print $fh <<_EOF_
#!/bin/bash
set -beEu -o pipefail

inPsl=\$1
outChain=\$2

unset TMPDIR
if [ -d "/data/tmp" ]; then
  export TMPDIR="/data/tmp"
elif [ -d "/scratch/tmp" ]; then
  export TMPDIR="/scratch/tmp"
else
  tmpSz=`df --output=avail -k /tmp | tail -1`
  shmSz=`df --output=avail -k /dev/shm | tail -1`
  if [ "\$shmSz" -gt "\$tmpSz" ]; then
    mkdir -p /dev/shm/tmp
    chmod 777 /dev/shm/tmp
    export TMPDIR="/dev/shm/tmp"
  else
    export TMPDIR="/tmp"
  fi
fi

tmpOut=`mktemp -p \$TMPDIR doMm2.chain.XXXXXX`

axtChain -verbose=0 -linearGap=medium -psl $pafDir/\$inPsl \\
    $tSeqScratch $qSeqScratch stdout \\
| chainBridge -linearGap=medium stdin $tSeqScratch $qSeqScratch \\
    \$tmpOut
mv \$tmpOut \$outChain
chmod 664 \$outChain
_EOF_
  ;
  close($fh);
  &HgAutomate::run("chmod a+x $runDir/job.sh");

  &HgAutomate::makeGsub($runDir,
			'job.sh $(path1) ' .
			'{check out line+ chainRaw/$(path1).chain}');
  my $whatItDoes = "It does a cluster run to chain the minimap2 alignments.";
  my $bossScript = newBash HgRemoteScript("$runDir/doChain.bash", $paraHub,
				      $runDir, $whatItDoes);
  my $paraRun = &HgAutomate::paraRun($chainRam, $chainCpu);
  my $gensub2 = &HgAutomate::gensub2();
  $bossScript->add(<<_EOF_
mkdir chainRaw

$gensub2 pslParts.lst single gsub jobList
$paraRun
_EOF_
  );
  $bossScript->execute();
} # doChain


#########################################################################
# * step: net [workhorse]
sub doNet {
  my $runDir = "$buildDir/run.chain";
  my @outs = ("$runDir/$tDb.$qDb.all.chain.gz",
	      "$runDir/$tDb.$qDb.noClass.net.gz");
  &HgAutomate::checkCleanSlate('net', 'load', @outs);
  &HgAutomate::checkExistsUnlessDebug('chain', 'net', "$runDir/chainRaw/");

  my $whatItDoes =
"It nets the chained minimap2 alignments and runs netChainSubset to produce
liftOver chains.";
  my $mach = &HgAutomate::chooseWorkhorse();
  my $bossScript = newBash HgRemoteScript("$runDir/doNet.bash", $mach,
				      $runDir, $whatItDoes);
  my $chromBased = (`wc -l < $tSizes` <= $HgAutomate::splitThreshold);
  my $lump = $chromBased ? "" : "-lump=100";
  $bossScript->add(<<_EOF_
unset TMPDIR
if [ -d "/data/tmp" ]; then
  export TMPDIR="/data/tmp"
elif [ -d "/scratch/tmp" ]; then
  export TMPDIR="/scratch/tmp"
else
  tmpSz=`df --output=avail -k /tmp | tail -1`
  shmSz=`df --output=avail -k /dev/shm | tail -1`
  if [ "\$shmSz" -gt "\$tmpSz" ]; then
    mkdir -p /dev/shm/tmp
    chmod 777 /dev/shm/tmp
    export TMPDIR="/dev/shm/tmp"
  else
    export TMPDIR="/tmp"
  fi
fi
# Use local scratch disk... this can be quite I/O intensive:
tmpDir=`mktemp -d -p \$TMPDIR doMm2.net.XXXXXX`

# Merge up the hierarchy and assign unique chain IDs:
chainMergeSort chainRaw/*.chain \\
| chainSplit $lump \$tmpDir/chainSplit stdin
endsInLf \$tmpDir/chainSplit/*.chain

mkdir \$tmpDir/netSplit \$tmpDir/overSplit
for f in \$tmpDir/chainSplit/*.chain; do
  split=\$(basename "\$f" .chain)
  chainNet \$f \\
    $tSizes $qSizes \\
    \$tmpDir/netSplit/\$split.net /dev/null
  netChainSubset \$tmpDir/netSplit/\$split.net \$f stdout \\
  | chainStitchId stdin \$tmpDir/overSplit/\$split.chain
done
endsInLf \$tmpDir/netSplit/*.net
endsInLf \$tmpDir/overSplit/*.chain

cat \$tmpDir/chainSplit/*.chain | gzip -c > $tDb.$qDb.all.chain.gz
cat \$tmpDir/netSplit/*.net     | gzip -c > $tDb.$qDb.noClass.net.gz

cat \$tmpDir/overSplit/*.chain | gzip -c > $buildDir/$liftOverChainFile
# make quickLift chain:
chainSwap  $buildDir/$liftOverChainFile stdout \\
   | chainToBigChain stdin $buildDir/$tDb.$qDb.quick.chain.txt \\
         $buildDir/$tDb.$qDb.quick.link.txt

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
  my $bossScript = newBash HgRemoteScript("$runDir/doLoad.bash", $dbHost,
				      $runDir, $whatItDoes);

  if ($dbExists) {
    $bossScript->add(<<_EOF_
# Link to standardized location of liftOver files:
mkdir -p $liftOverChainDir
rm -f $liftOverChainPath
ln -s $buildDir/$liftOverChainFile $liftOverChainPath
tmpFile=`mktemp -t -p /tmp tmpMd5.XXXXXX`
grep -v $liftOverChainFile $liftOverChainDir/md5sum.txt > \$tmpFile || true
md5sum $buildDir/$liftOverChainFile | sed -e "s#$buildDir/##;" >> \$tmpFile
sort \$tmpFile > $liftOverChainDir/md5sum.txt
rm -f \$tmpFile

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
  } else {
    $bossScript->add(<<_EOF_
hgLoadChain -test -noBin -tIndex $tDb chain$QDb $buildDir/$liftOverChainFile
wget --no-check-certificate -O bigChain.as 'https://raw.githubusercontent.com/ucscGenomeBrowser/kent/refs/heads/master/src/hg/lib/bigChain.as'
wget --no-check-certificate -O bigLink.as 'https://raw.githubusercontent.com/ucscGenomeBrowser/kent/refs/heads/master/src/hg/lib/bigLink.as'
sed 's/.000000//' chain.tab | awk 'BEGIN {OFS="\\t"} {print \$2, \$4, \$5, \$11, 1000, \$8, \$3, \$6, \$7, \$9, \$10, \$1}' > chain${QDb}.tab
bedToBigBed -type=bed6+6 -as=bigChain.as -tab chain${QDb}.tab $tSizes chain${QDb}.bb
awk 'BEGIN {OFS="\\t"} {print \$1, \$2, \$3, \$5, \$4}' link.tab | sort -k1,1 -k2,2n > chain${QDb}Link.tab
bedToBigBed -type=bed4+1 -as=bigLink.as -tab chain${QDb}Link.tab $tSizes chain${QDb}Link.bb

bedToBigBed -type=bed6+6 -as=bigChain.as -tab $tDb.$qDb.quick.chain.txt $qSizes $tDb.$qDb.quick.bb
bedToBigBed -type=bed4+1 -as=bigLink.as -tab $tDb.$qDb.quick.link.txt $qSizes $tDb.$qDb.quickLink.bb

totalBases=`ave -col=2 $tSizes | grep "^total" | awk '{printf "%d", \$2}'`
basesCovered=`bigBedInfo chain${QDb}Link.bb | grep "basesCovered" | cut -d' ' -f2 | tr -d ','`
percentCovered=`echo \$basesCovered \$totalBases | awk '{printf "%.3f", 100.0*\$1/\$2}'`
printf "%d bases of %d (%s%%) in intersection\\n" "\$basesCovered" "\$totalBases" "\$percentCovered" > fb.$tDb.chain.${QDb}Link.txt

qBases=`ave -col=2 $qSizes | grep "^total" | awk '{printf "%d", \$2}'`
qCovered=`bigBedInfo $tDb.$qDb.quickLink.bb | grep "basesCovered" | cut -d' ' -f2 | tr -d ','`
qPerCent=`echo \$qCovered \$qBases | awk '{printf "%.3f", 100.0*\$1/\$2}'`
printf "%d bases of %d (%s%%) in intersection\\n" "\$qCovered" "\$qBases" "\$qPerCent" > fb.$tDb.quick${QDb}Link.txt
rm -f link.tab chain.tab bigChain.as bigLink.as chain${QDb}.tab chain${QDb}Link.tab $tDb.$qDb.quick.chain.txt $tDb.$qDb.quick.link.txt

_EOF_
    );
  }
  $bossScript->execute();
} # doLoad


#########################################################################
# * step: cleanup [fileServer]
sub doCleanup {
  my $runDir = "$buildDir";
  my $whatItDoes = "It cleans up or compresses intermediate files.";
  $fileServer = &HgAutomate::chooseFileServer($runDir);
  my $bossScript = newBash HgRemoteScript("$runDir/doCleanup.bash", $fileServer,
				      $runDir, $whatItDoes);
  $bossScript->add(<<_EOF_
rm -f run.mm2/query.fa
rm -rf run.mm2/paf/
rm -rf run.chain/chainRaw/
rm -f mashSketch.a.msh mashSketch.b.msh
_EOF_
  );
  $bossScript->execute();
} # doCleanup


sub getSeqAndSizes {
  if ($opt_target2Bit) {
    $tSeq = $opt_target2Bit
  } else {
    # Test assumptions about 2bit and chrom.sizes files.
    $tSeq = "/scratch/data/$tDb/$tDb.2bit";
    if (! -e $tSeq) {
      # allow it to exist here too:
      my $fs = "$HgAutomate::clusterData";
	&HgAutomate::verbose(1, "checking $fs/$tDb/$tDb.2bit\n");
        if (-e "$fs/$tDb/$tDb.2bit") {
          $tSeq = "$fs/$tDb/$tDb.2bit";
        }
    }
  }

  if ($opt_targetSizes) {
    $tSizes = $opt_targetSizes;
  } else {
    $tSizes = "$HgAutomate::clusterData/$tDb/chrom.sizes";
  }

  if ($opt_query2Bit) {
    $qSeq = $opt_query2Bit;
  } else {
    $qSeq = "/scratch/data/$qDb/$qDb.2bit";
    if (! -e $qSeq) {
      # allow it to exist here too:
      my $fs = "$HgAutomate::clusterData";
	&HgAutomate::verbose(1, "checking $fs/$qDb/$qDb.2bit\n");
        if (-e "$fs/$qDb/$qDb.2bit") {
          $qSeq = "$fs/$qDb/$qDb.2bit";
        }
    }
  }

  if ($opt_querySizes) {
    $qSizes = $opt_querySizes;
  } else {
    $qSizes = "$HgAutomate::clusterData/$qDb/chrom.sizes";
  }

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


sub estimateDivergence {
  # Pick -minimapPreset automatically via AssemblyDivergence.pm's mash
  # distance between tDb and qDb, unless the user already gave
  # -minimapPreset explicitly.  Cheap (mash sketch+dist on a whole genome
  # is seconds to low minutes), so we just run it locally rather than
  # turning it into its own cluster step.
  if ($minimapPreset) {
    &HgAutomate::verbose(1,
	"Using explicit -minimapPreset=$minimapPreset (mash not run).\n");
    return;
  }
  my $mashFile = "$buildDir/mashDistance.txt";
  if (-e $mashFile && ! $opt_regenerateMash) {
    # Reuse a previous estimate so -continue steps stay consistent with
    # whatever the align step (if already run) actually used.
    open(my $fh, "<", $mashFile) || die "Can't read $mashFile: $!\n";
    my $line = <$fh>;
    close($fh);
    if ($line =~ /^mashDistance=(\S+)\s+minimapPreset=(\S+)/) {
      $minimapPreset = $2;
      &HgAutomate::verbose(1, "Reusing cached mash distance $1 -> " .
	  "-minimapPreset=$minimapPreset from $mashFile\n");
      return;
    }
  }
  if ($opt_debug) {
    $minimapPreset = 'asm5';
    &HgAutomate::verbose(1, "-debug: skipping mash, using -minimapPreset=$minimapPreset\n");
    return;
  }
  &HgAutomate::verbose(1,
      "Estimating $tDb/$qDb divergence with mash to pick -minimapPreset ...\n");
  my $dist = eval { &AssemblyDivergence::mashDistance($tSeq, $qSeq, $buildDir, $opt_regenerateMash); };
  if ($@) {
    $minimapPreset = 'asm5';
    warn "estimateDivergence: $@" .
	 "falling back to -minimapPreset=$minimapPreset.  Pass " .
	 "-minimapPreset explicitly to pick it yourself and silence this.\n";
    return;
  }
  my ($pipeline, $preset, $warning) = &AssemblyDivergence::choosePipeline($dist);
  warn "$warning\n" if ($warning);
  if ($pipeline ne 'minimap2') {
    die "estimateDivergence: mash distance $dist between $tDb and $qDb " .
	"looks too diverged for doMiniMap2.pl -- use doBlastzChainNet.pl " .
	"instead, or re-run with -minimapPreset to force minimap2 anyway.\n";
  }
  $minimapPreset = $preset;
  open(my $fh, ">", $mashFile) || die "Can't write $mashFile: $!\n";
  print $fh "mashDistance=$dist minimapPreset=$minimapPreset\n";
  close($fh);
  &HgAutomate::verbose(1,
      "mash distance $dist between $tDb and $qDb -> -minimapPreset=$minimapPreset\n");
} # estimateDivergence

#########################################################################
# main

# Prevent "Suspended (tty input)" hanging:
&HgAutomate::closeStdin();

&checkOptions();

&usage(1) if (scalar(@ARGV) != 2);
($tDb, $qDb) = @ARGV;

# may be working on a 2bit file that does not have a database browser
$dbExists = 0;
$dbExists = 1 if (&HgAutomate::databaseExists($dbHost, $tDb));

&getSeqAndSizes();
$QDb = ucfirst($qDb);
$liftOverChainDir = "$HgAutomate::clusterData/$tDb/$HgAutomate::trackBuild/liftOver";
$liftOverChainFile = "${tDb}To${QDb}.over.chain.gz";
$liftOverChainPath = "$liftOverChainDir/$liftOverChainFile";
$chainRam = $opt_chainRam ? $opt_chainRam : $chainRam;
$chainCpu = $opt_chainCpu ? $opt_chainCpu : $chainCpu;
$minimapCpu = $opt_minimapCpu ? $opt_minimapCpu : $minimapCpu;
$ramG = $opt_ram ? $opt_ram : $ramG;

my $date = `date +%Y-%m-%d`;
chomp $date;
$buildDir = $opt_buildDir ? $opt_buildDir :
  "$HgAutomate::clusterData/$tDb/$HgAutomate::trackBuild/mm2.$qDb.$date";

if (! -d $buildDir) {
  if ($stepper->stepPrecedes('align', $stepper->getStartStep())) {
    die "$buildDir does not exist; try running again with -buildDir.\n";
  }
  &HgAutomate::mustMkdir($buildDir);
}

&estimateDivergence();

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
