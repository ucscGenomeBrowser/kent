#!/usr/bin/env perl

# DO NOT EDIT the /cluster/bin/scripts copy of this file --
# edit ~/kent/src/hg/utils/automation/doCrispr.pl instead.

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
    $opt_forHub
    $opt_twoBit
    $opt_tableName
    /;

# Specify the steps supported with -continue / -stop:
my $stepper = new HgStepManager(
    [ { name => 'indexFa',   func => \&doIndexFa },
      { name => 'ranges',   func => \&doRanges },
      { name => 'guides',   func => \&doGuides },
      { name => 'specScoreJobList',   func => \&doSpecScoreJobList },
      { name => 'specScores',   func => \&doSpecScores },
      { name => 'effScores',   func => \&doEffScores },
      { name => 'offTargets',   func => \&doOffTargets },
      { name => 'load',   func => \&doLoad },
      { name => 'cleanup', func => \&doCleanup },
    ]
				);

# external database of crispr data:
my $crisporSrc = "/hive/data/outside/crisprTrack/crispor";
# path to the crispr track pipeline scripts
# look at ~/kent/src/hg/makeDb/crisprTrack/README.txt for
# more info about these scripts in there
my $crisprScripts = "/hive/data/outside/crisprTrack/scripts";

# Option defaults:
my $dbHost = 'hgwdev';
my $bigClusterHub = 'ku';
my $smallClusterHub = 'hgwdev-101';
my $fileServer = 'hgwdev';
my $workhorse = 'hgwdev';
my $defaultWorkhorse = 'hgwdev';
my $twoBit = "$HgAutomate::clusterData/\$db/\$db.2bit";
my $tableName = "crispr10K";	# table name in database to load
my $python = "/cluster/software/bin/python";

my $base = $0;
$base =~ s/^(.*\/)?//;

sub usage {
  # Usage / help / self-documentation:
  my ($status, $detailed) = @_;
  # Basic help (for incorrect usage):
  print STDERR "
usage: $base db
options:
    db - UCSC database name
";
  print STDERR $stepper->getOptionHelp();
  print STDERR <<_EOF_
    -buildDir dir         Use dir instead of default:
                              $HgAutomate::clusterData/\$db/$HgAutomate::trackBuild/crispr
    -forHub               This is being build for a hub, remove assumptions about
                              database-based genome
    -twoBit seq.2bit      Use seq.2bit as the input sequence instead
                              of default: ($twoBit).
    -tableName name       Use name as gbdb path name and base name for db
                              table name, default: $tableName(Ranges|Targets)
_EOF_
  ;
  print STDERR &HgAutomate::getCommonOptionHelp('dbHost' => $dbHost,
						'workhorse' => $defaultWorkhorse,
						'bigClusterHub' => $bigClusterHub,
						'fileServer' => $fileServer,
						'smallClusterHub' => $smallClusterHub);
  print STDERR "
Automates UCSC's crispr procedure.  Steps:
    indexFa Construct bwa index for source fasta, will be skipped when already
            available (workhorse)
    ranges: Construct ranges to work on, avoiding gaps (workhorse)
    guides: Extract guide sequences from ranges (smallClusterHub)
    specScoreJobList: Construct spec score jobList (workhorse)
    specScores: Compute spec scores (bigClusterHub)
    effScores: Compute the efficiency scores (bigClusterHub)
    offTargets: Converting off-targets (bigClusterHub)
    load: Construct bigBed file of results, link into /gbdb/<db>/<tableName>
          and load into database if database is present (dbHost)
    cleanup: Removes or compresses intermediate files. (fileServer)
All operations are performed in the build directory which is
$HgAutomate::clusterData/\$db/$HgAutomate::trackBuild/crispr unless -buildDir is given.
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
# Command line args: db
my ($db);
# Other:
my ($buildDir, $forHub, $secondsStart, $secondsEnd, $dbExists, $crisporGenomesDir, $genomeFname,
    $chromSizes);

sub checkOptions {
  # Make sure command line options are valid/supported.
  my $ok = GetOptions(@HgStepManager::optionSpec,
		      'buildDir=s',
		      'twoBit=s',
		      'tableName=s',
                      'forHub',
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
# * step: indexFa [workhorse]
sub doIndexFa {
  my $runDir = "$buildDir/indexFa";
  my $testDone = "${crisporGenomesDir}/${db}/genomeInfo.tab";

  &HgAutomate::mustMkdir($runDir);
  my $whatItDoes = "Construct bwa indexes for the new genome fasta.";
  my $bossScript = newBash HgRemoteScript("$runDir/runIndexFa.bash",
		$workhorse, $runDir, $whatItDoes);

  $bossScript->add(<<_EOF_
export PATH=$crisporSrc/tools/usrLocalBin:\$PATH
export TMPDIR=/dev/shm

if [ $forHub = 1 -a ! -s "$chromSizes" ] ; then
  twoBitInfo $twoBit $chromSizes
fi
if [ ! -s "$db.fa" ] ; then
  twoBitToFa -noMask $twoBit $db.fa
fi

if [ ! -s "$db.fa.fai" ]; then
  /cluster/bin/samtools-0.1.19/samtools faidx $db.fa &
fi

if [ ! -s "$testDone" ]; then
  if [ $forHub = 1 ] ; then
    time ($crisporSrc/tools/crisprAddGenome \\
        fasta $db.fa --baseDir $crisporGenomesDir \\
        --desc='$db|$db|$db|$db') > createIndex.log 2>&1 &
    rm -f $crisporSrc/genomes/$db
    ln -s $crisporGenomesDir/$db $crisporSrc/genomes/$db
  else
    time ($crisporSrc/tools/crisprAddGenome \\
        ucscLocal $db --baseDir $crisporSrc/genomes) > createIndex.log 2>&1 &
  fi
fi

wait

if [ ! -s "$db.fa.fai" ]; then
  printf "ERROR: step indexFa: samtools index of $db.fa has failed\\n" 1>&2
  exit 255
fi
if [ ! -s "$testDone" ]; then
   printf "ERROR: bwa index not created correctly\\n" 1>&2
   printf "result file does not exist:\\n" 1>&2
   printf "%s\\n" "$testDone"
   exit 255
fi

_EOF_
  );
  $bossScript->execute();
  if ( -s "${testDone}" ) {
     &HgAutomate::verbose(1,
         "# step indesFa is already completed, continuing...\n");
     return;
  }
} # doIndexFa

#########################################################################
# * step: ranges [workhorse]
sub doRanges {
  my $runDir = "$buildDir/ranges";
  my $inFaDir = "$buildDir/ranges/tmp";

  # First, make sure we're starting clean, or if done already, let
  #   it continue.
  if ( ! $opt_debug && ( -d "$runDir" ) ) {
    if (! -d "$inFaDir" && -s "$runDir/ranges.fa") {
      die "step ranges may have been attempted and failed," .
        "directory $runDir exists, however directory $inFaDir does not " .
        "exist.  Either run with -continue guides or some later " .
        "stage, or move aside/remove $runDir and run again, " .
        "or determine the failure to complete this step.\n";
   } else {
     &HgAutomate::verbose(1,
         "# step ranges is already completed, continuing...\n");
     return;
   }
  }

  &HgAutomate::mustMkdir($runDir);

  my $whatItDoes = "Construct ranges to work on around exons.";
  my $bossScript = newBash HgRemoteScript("$runDir/runRanges.bash",
		$workhorse, $runDir, $whatItDoes);

  $bossScript->add(<<_EOF_
printf "# Get sequence without any gaps.\\n" 1>&2
twoBitInfo -nBed $twoBit stdout | bedInvert.pl $chromSizes stdin > crisprRanges.bed

twoBitToFa -noMask -bedPos -bed=crisprRanges.bed $twoBit ranges.fa

printf "# split the sequence file into pieces for the cluster\\n" 1>&2
mkdir -p tmp
faSplit sequence ranges.fa 100 tmp/
_EOF_
  );
  $bossScript->execute();
} # doRanges

#########################################################################
sub doGuides {
# * step: guides [bigClusterHub]
  my $paraHub = $bigClusterHub;
  my $runDir = "$buildDir/guides";
  my $inFaDir = "$buildDir/ranges/tmp";

  # First, make sure we're starting clean, or if done already, let
  #   it continue.
  if ( ! $opt_debug && ( -d "$runDir" ) ) {
    if (-s "$runDir/run.time" && -s "$buildDir/allGuides.txt" && -s "$buildDir/allGuides.bed") {
     &HgAutomate::verbose(1,
         "# step guides is already completed, continuing...\n");
     return;
    } else {
      die "step guides may have been attempted and failed," .
        "directory $runDir exists, however the run.time result does not " .
        "exist.  Either run with -continue specScores or some later " .
        "stage, or move aside/remove $runDir and run again, " .
        "or determine the failure to complete this step.\n";
    }
  }

  &HgAutomate::mustMkdir($runDir);
  my $templateCmd = ("$python $crisprScripts/findGuides.py " .
		     "{check in exists $inFaDir/\$(file1)} " .
		     "{check in exists $chromSizes} " .
		     "tmp/\$(root1).bed " .
		     "{check out exists tmp/\$(root1).fa}" );
  &HgAutomate::makeGsub($runDir, $templateCmd);

  my $whatItDoes = "Construct guides for alignments.";
  my $bossScript = newBash HgRemoteScript("$runDir/runGuides.bash",
		$paraHub, $runDir, $whatItDoes);

  my $paraRun = &HgAutomate::paraRun();
  my $gensub2 = &HgAutomate::gensub2();
  `touch "$runDir/para_hub_$paraHub"`;
  $bossScript->add(<<_EOF_
mkdir -p tmp
find $inFaDir -type f | grep "\.fa\$" > fa.list
$gensub2 fa.list single gsub jobList
$paraRun
find tmp -name '*.fa' | xargs cat | grep -v "^>" > ../allGuides.txt
find tmp -name '*.bed' | xargs cat > ../allGuides.bed
_EOF_
  );
  $bossScript->execute();
} # doGuides

#########################################################################
sub doSpecScoreJobList {
# * step: specScoreJobList [workhorse]
  my $paraHub = $smallClusterHub;
  my $runDir = "$buildDir/specScores";
  my $indexDir = "$buildDir/indexFa";

  # First, make sure we're starting clean, or if done already, let
  #   it continue.
  if ( ! $opt_debug && ( -d "$runDir" ) ) {
    if (-s "$runDir/jobList") {
     &HgAutomate::verbose(1,
         "# step specScoreJobList is already completed, continuing...\n");
     return;
    } else {
      die "step specScoreJobList may have been attempted and failed," .
        "directory $runDir exists, however the 'jobList' result does not " .
        "exist.  Either run with -continue effScores or some later " .
        "stage, or move aside/remove $runDir and run again, " .
        "or determine the failure to complete this step.\n";
    }
  }

  &HgAutomate::mustMkdir($runDir);

  my $whatItDoes = "Compute spec scores.";
  my $bossScript = newBash HgRemoteScript("$runDir/runJobList.bash",
		$workhorse, $runDir, $whatItDoes);

  &HgAutomate::verbose(1,
         "# preparing the joblist for specificity alignment cluster run\n");

  $bossScript->add(<<_EOF_
mkdir -p tmp/inFa tmp/outGuides tmp/outOffs
$python $crisprScripts/splitGuidesSpecScore.py ../allGuides.txt tmp/inFa jobNames.txt
_EOF_
  );
  $bossScript->execute();
} # doSpecScoreJobList

#########################################################################
sub doSpecScores {
# * step: specScores [bigClusterHub]
  my $paraHub = $bigClusterHub;
  my $runDir = "$buildDir/specScores";
  my $indexDir = "$buildDir/indexFa";

  if (! $opt_debug && (! -s "$buildDir/allGuides.bed")) {
     die "ERROR: previous step 'guides' has not completed in order to run" .
          " this 'specScores' step.  Complete 'guides' step to create" .
          " result file 'allGuides.bed'";
  }
  if (! $opt_debug && (! -s "$runDir/jobNames.txt")) {
     die "ERROR: previous step 'specScoreJobList' has not completed in order" .
          " to run this 'specScores' step.  Complete 'specScoreJobList'" .
          " step to create result file 'specScores/jobNames.txt'";
  }

  # First, make sure we're starting clean, or if done already, let
  #   it continue.
  if ( ! $opt_debug ) {
    if (-s "$runDir/run.time" && -s "$buildDir/specScores.tab") {
     &HgAutomate::verbose(1,
         "# step specScores is already completed, continuing...\n");
     return;
    }
  }

  &HgAutomate::mustMkdir($runDir);
  my $templateCmd = ("$python $crisporSrc/fasterCrispor.py " .
		     "$db {check in exists \$(path1)} " .
		     "{check out exists tmp/outGuides/\$(root1).tab} " .
		     "-o tmp/outOffs/\$(root1).tab" );
  &HgAutomate::makeGsub($runDir, $templateCmd);

  my $whatItDoes = "Compute spec scores.";
  my $bossScript = newBash HgRemoteScript("$runDir/runSpecScores.bash",
		$paraHub, $runDir, $whatItDoes);

  &HgAutomate::verbose(1,
         "# preparing the specificity alignment cluster run\n");
  `touch "$runDir/para_hub_$paraHub"`;

  my $paraRun = &HgAutomate::paraRun();
  my $gensub2 = &HgAutomate::gensub2();
  $bossScript->add(<<_EOF_
# preload the /dev/shm on each parasol node with the fasta and index
parasol list machines | grep -v dead | awk '{print \$1}' | sort -u | while read M
do
  ssh "\${M}" "rsync -a --stats ${indexDir}/$db.fa ${indexDir}/$db.fa.fai /dev/shm/crispr10K.$db/ || true" < /dev/null
done
# and the /dev/shm on this parasol hub
rsync -a --stats ${indexDir}/$db.fa ${indexDir}/$db.fa.fai /dev/shm/crispr10K.$db/ < /dev/null
ln -s /dev/shm/crispr10K.$db/$db.fa* ./

$gensub2 jobNames.txt single gsub jobList
$paraRun
find tmp/outGuides -type f | xargs cut -f3-6 > ../specScores.tab
printf "# Number of specScores: %d\\n" "`cat ../specScores.tab | wc -l`" 1>&2
_EOF_
  );
  $bossScript->execute();
} # doSpecScores

#########################################################################
sub doEffScores {
# * step: effScores [bigClusterHub]
  my $paraHub = $bigClusterHub;
  my $runDir = "$buildDir/effScores";

  if (! $opt_debug && (! -s "$buildDir/allGuides.bed")) {
     die "ERROR: previous step 'guides' has not completed in order to run" .
          " this 'effScores' step.  Complete 'guides' step to create" .
          " result file 'allGuides.bed'";
  }

  # First, make sure we're starting clean, or if done already, let
  #   it continue.
  if ( ! $opt_debug && ( -d "$runDir" ) ) {
    if (-s "$runDir/run.time" && -s "$buildDir/effScores.tab") {
     &HgAutomate::verbose(1,
         "# step effScores is already completed, continuing...\n");
     return;
    } else {
      die "step effScores may have been attempted and failed," .
        "directory $runDir exists, however the run.time result does not " .
        "exist.  Either run with -continue offTargets or some later " .
        "stage, or move aside/remove $runDir and run again, " .
        "or determine the failure to complete this step.\n";
    }
  }

  &HgAutomate::mustMkdir($runDir);
  my $templateCmd = ("$python $crisprScripts/jobCalcEffScores.py " .
		     "$crisporSrc $db {check in exists \$(path1)} " .
		     "{check out exists tmp/out/\$(root1).tab} " );
  &HgAutomate::makeGsub($runDir, $templateCmd);

  my $whatItDoes = "Compute efficiency scores.";
  my $bossScript = newBash HgRemoteScript("$runDir/runEffScores.bash",
		$paraHub, $runDir, $whatItDoes);

  my $paraRun = &HgAutomate::paraRun();
  my $gensub2 = &HgAutomate::gensub2();
  `touch "$runDir/para_hub_$paraHub"`;
  $bossScript->add(<<_EOF_
$python $crisprScripts/splitGuidesEffScore.py $chromSizes ../allGuides.bed tmp jobNames.txt
$gensub2 jobNames.txt single gsub jobList
$paraRun
find tmp/out -type f | xargs cat > ../effScores.tab
printf "# Number of effScores: %d\\n" "`cat ../effScores.tab | wc -l`" 1>&2
_EOF_
  );
  $bossScript->execute();
} # doEffScores

#########################################################################
sub doOffTargets {
# * step: offTargets [bigClusterHub]
  my $paraHub = $bigClusterHub;
  my $runDir = "$buildDir/offTargets";
  my $specScores = "$buildDir/specScores";

  if (! $opt_debug && (! -s "$buildDir/specScores.tab")) {
     die "ERROR: previous step 'specScores' has not completed in order to run" .
          " this 'offTargets' step.  Complete 'specScores' step to create" .
          " result file 'specScores.tab'";
  }

  # First, make sure we're starting clean, or if done already, let
  #   it continue.
  if ( ! $opt_debug && ( -d "$runDir" ) ) {
    if (-s "$runDir/run.time" && -s "$buildDir/crisprDetails.tab" && -s "$buildDir/offtargets.offsets.tab" ) {
     &HgAutomate::verbose(1,
         "# step offTargets is already completed, continuing...\n");
     return;
    } else {
      die "step offTargets may have been attempted and failed," .
        "directory $runDir exists, however the run.time result does not " .
        "exist.  Either run with -continue load or some later " .
        "stage, or move aside/remove $runDir and run again, " .
        "or determine the failure to complete this step.\n";
    }
  }

  &HgAutomate::mustMkdir($runDir);
  my $templateCmd = ("$python $crisprScripts/convOffs.py " .
		     "$db {check in exists \$(path1)} " .
		     "{check out exists tmp/out/\$(root1).tab} " );
  &HgAutomate::makeGsub($runDir, $templateCmd);

  my $whatItDoes = "Compute efficiency scores.";
  my $bossScript = newBash HgRemoteScript("$runDir/runOffTargets.bash",
		$paraHub, $runDir, $whatItDoes);

  my $paraRun = &HgAutomate::paraRun();
  my $gensub2 = &HgAutomate::gensub2();
  `touch "$runDir/para_hub_$paraHub"`;
  $bossScript->add(<<_EOF_
# to allow the crisprScripts to find their python2.7 version:
export PATH=/cluster/software/bin:\$PATH

mkdir -p tmp/inFnames tmp/out/
find $specScores/tmp/outOffs -type f | sed -e 's#.*/tmp/#../specScores/tmp/#;' > otFnames.txt
splitFile otFnames.txt 20 tmp/inFnames/otJob
find ./tmp/inFnames -type f | sed -e 's#^./##;' > file.list
$gensub2 file.list single gsub jobList
$paraRun
$crisprScripts/catAndIndex tmp/out ../crisprDetails.tab ../offtargets.offsets.tab --headers=_mismatchCounts,_crisprOfftargets
_EOF_
  );
  $bossScript->execute();
} # doOffTargets

#########################################################################
sub doLoad {
# * step: load [dbHost]
  my $runDir = "$buildDir";

  # First, make sure we're starting clean, or if done already, let
  #   it continue.
  if ( ! $opt_debug && ( -d "$runDir" ) ) {
    if (-s "$runDir/crispr.bb") {
     &HgAutomate::verbose(1,
         "# step load is already completed, continuing...\n");
     return;
    }
  }

  &HgAutomate::mustMkdir($runDir);

  my $whatItDoes = "Construct bigBed file and load into database.";
  my $bossScript = newBash HgRemoteScript("$runDir/loadUp.bash",
		$dbHost, $runDir, $whatItDoes);

  $bossScript->add(<<_EOF_
# creating the bigBed file
$python $crisprScripts/createBigBed.py $db allGuides.bed specScores.tab effScores.tab offtargets.offsets.tab
_EOF_
  );
  if (!$forHub) {
    $bossScript->add(<<_EOF_
# now link it into gbdb
mkdir -p /gbdb/$db/$tableName/
ln -sf `pwd`/crispr.bb /gbdb/$db/$tableName/crispr.bb
ln -sf `pwd`/crisprDetails.tab /gbdb/$db/$tableName/crisprDetails.tab
_EOF_
     );
  }
  if ($dbExists && !$forHub) {
    $bossScript->add(<<_EOF_
hgBbiDbLink $db ${tableName}Targets /gbdb/$db/$tableName/crispr.bb
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
  my $bossScript = newBash HgRemoteScript("$runDir/doCleanup.bash", $fileServer,
				      $runDir, $whatItDoes);
  # Verify previous step is complete
  #   it continue.
  if ( ! $opt_debug && ( -d "$runDir" ) ) {
    if (! -s "$runDir/crispr.bb") {
     die "# previous step load has not completed\n";
    }
    if ( -s "specScores/$db.fa.gz" ) {
     &HgAutomate::verbose(1,
         "# step cleanup is already completed, continuing...\n");
     return;
    }
  }

  $bossScript->add(<<_EOF_
if [ -s "specScores/$db.fa.gz" ]; then
  printf "# step cleanup has already completed.\\n"
  exit 0
fi
printf "#\tdisk space before cleaning\\n"
df -h .
rm -fr ranges/tmp &
rm -fr ranges/ranges.fa &
rm -fr guides/err &
rm -fr guides/tmp &
rm -f guides/batch.bak &
rm -f specScores/batch.bak &
rm -f specScores/$db.fa specScores/$db.fa.fai &
rm -f indexFa/$db.fa indexFa/$db.fa.fai &
rm -fr specScores/err &
rm -fr specScores/tmp &
rm -fr effScores/err &
rm -fr effScores/tmp &
rm -f effScores/batch.bak &
rm -fr offTargets/err &
rm -fr offTargets/tmp &
rm -f offTargets/batch.bak &
gzip specScores.tab effScores.tab offtargets.offsets.tab &
wait
ssh $bigClusterHub parasol list machines | grep -v dead | awk '{print \$1}' | sort -u | while read M
do
  ssh "\${M}" "rm -fr /dev/shm/crispr10K.$db" < /dev/null
done
ssh $bigClusterHub "rm -fr /dev/shm/crispr10K.$db" < /dev/null
printf "#\tdisk space after cleaning\\n"
df -h .
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
$workhorse = $opt_workhorse if ($opt_workhorse);
$bigClusterHub = $opt_bigClusterHub if ($opt_bigClusterHub);
$smallClusterHub = $opt_smallClusterHub if ($opt_smallClusterHub);
$fileServer = $opt_fileServer if ($opt_fileServer);
$dbHost = $opt_dbHost if ($opt_dbHost);
&HgAutomate::verbose(1, "# bigClusterHub $bigClusterHub\n");
&HgAutomate::verbose(1, "# smallClusterHub $smallClusterHub\n");
&HgAutomate::verbose(1, "# fileServer $fileServer\n");
&HgAutomate::verbose(1, "# dbHost $dbHost\n");
&HgAutomate::verbose(1, "# workhorse $workhorse\n");
$secondsStart = `date "+%s"`;
chomp $secondsStart;

# required command line arguments:
($db) = @ARGV;

# Establish directory to work in:
$buildDir = $opt_buildDir ? $opt_buildDir :
    "$HgAutomate::clusterData/$db/$HgAutomate::trackBuild/crispr";
$forHub = $opt_forHub ? 1 : 0;
$twoBit = $opt_twoBit ? $opt_twoBit : "$HgAutomate::clusterData/$db/$db.2bit";
&HgAutomate::verbose(1, "# 2bit file: $twoBit\n");
die "can not find 2bit file: '$twoBit'" if ( ! -s $twoBit);
$tableName = $opt_tableName ? $opt_tableName : $tableName;
&HgAutomate::verbose(1, "# tableName $tableName\n");

if ($forHub) {
    $crisporGenomesDir  = "$buildDir/genomes";
    $chromSizes = "$buildDir/indexFa/chrom.sizes";
} else {
    $crisporGenomesDir = "$crisporSrc/genomes";
    $chromSizes = "/hive/data/genomes/$db/chrom.sizes";
}
$genomeFname = "$crisporGenomesDir/$db/$db.fa.bwt";


# may be working on a 2bit file that does not have a database browser
$dbExists = 0;
$dbExists = 1 if (&HgAutomate::databaseExists($dbHost, $db));

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
	"\n *** All done !$upThrough  Elapsed time: ${elapsedMinutes}m${elapsedSeconds}s\n");
&HgAutomate::verbose(1,
	" *** Steps were performed in $buildDir\n");
&HgAutomate::verbose(1, "\n");

