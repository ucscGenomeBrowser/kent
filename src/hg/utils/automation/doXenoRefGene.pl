#!/usr/bin/env perl

# DO NOT EDIT the /cluster/bin/scripts copy of this file --
# edit ~/kent/src/hg/utils/automation/doXenoRefGene.pl instead.

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
    $opt_mrnas
    $opt_noDbGenePredCheck
    /;

# Specify the steps supported with -continue / -stop:
my $stepper = new HgStepManager(
    [ { name => 'splitTarget',   func => \&doSplitTarget },
      { name => 'blatRun', func => \&doBlatRun },
      { name => 'filterPsl', func => \&doFilterPsl },
      { name => 'makeGp', func => \&doMakeGp },
      { name => 'cleanup', func => \&doCleanup },
    ]
				);

# Option defaults:
my $bigClusterHub = 'ku';
my $workhorse = 'hgwdev';
my $dbHost = 'hgwdev';
my $defaultWorkhorse = 'hgwdev';
my $maskedSeq = "$HgAutomate::clusterData/\$db/\$db.2bit";
my $mrnas = "/hive/data/genomes/asmHubs/VGP/xenoRefSeq";
my $noDbGenePredCheck = 1;    # default yes, use -db for genePredCheck
my $mrnas = "/hive/data/genomes/asmHubs/VGP/xenoRefSeq";
my $augustusDir = "/hive/data/outside/augustus/augustus-3.3.1";
my $augustusConfig="$augustusDir/config";

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
                          $HgAutomate::clusterData/\$db/$HgAutomate::trackBuild/xenoRefGene
                          (necessary when continuing at a later date).
    -maskedSeq seq.2bit   Use seq.2bit as the masked input sequence instead
                          of default ($maskedSeq).
    -mrnas </path/to/xenoRefSeqMrna> - location of xenoRefMrna.fa.gz
                expanded directory of mrnas/ and xenoRefMrna.sizes, default
                $mrnas
    -noDbGenePredCheck    do not use -db= on genePredCheck, there is no real db
_EOF_
  ;

  print STDERR &HgAutomate::getCommonOptionHelp('dbHost' => $dbHost,
                                'bigClusterHub' => $bigClusterHub,
                                'workhorse' => $defaultWorkhorse);
  print STDERR "
Automates construction of a xeno RefSeq gene track from RefSeq mRNAs.  Steps:
    splitTarget split the masked target sequence into individual fasta sequences
    blatRun:    Run blat with the xenoRefSeq mRNAs query to target sequence
    filterPsl:  Run pslCDnaFilter on the blat psl results
    makeGp:     Transform the filtered PSL into a genePred file and create
                bigGenePred from the genePred file
    cleanup:    Removes hard-masked fastas and output from gsBig.
All operations are performed in the build directory which is
$HgAutomate::clusterData/\$db/$HgAutomate::trackBuild/augustus unless -buildDir is given.
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
		      'mrnas=s',
		      'noDbGenePredCheck',
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
# * step: splitTarget [workhorse]
sub doSplitTarget {
  # run faSplit on the masked 2bit target sequence and prepare the target.list
  my $runDir = "$buildDir";

  # First, make sure we're starting clean.
  if (-d "$runDir/target") {
    die "doXenoRefGene splitTarget step already done, remove directory 'target' to rerun,\n" .
      "or '-continue blatRun' to run next step.\n";
  }

  my $whatItDoes="split the masked 2bit file into fasta files for blat alignment processing.";
  my $bossScript = newBash HgRemoteScript("$runDir/doSplitTarget.bash", $workhorse,
				      $runDir, $whatItDoes);
  $bossScript->add(<<_EOF_
mkdir target
twoBitToFa $maskedSeq stdout | faSplit byname stdin target/
gzip target/*.fa
ls target | sed -e 's/.fa.gz//;' > target.list
faSize -detailed target/*.fa.gz | sort -k2,2nr > $db.chrom.sizes
_EOF_
  );

  $bossScript->execute();
} # doSplitTarget

#########################################################################
# * step: blatRun [bigClusterHub]
sub doBlatRun {
  # Set up and perform the cluster run to run the blat alignment of RefSeq
  #     mrnas to the split target fasta sequences.
  my $paraHub = $bigClusterHub;
  my $runDir = "$buildDir/blatRun";
  # First, make sure previous step has completed,
  #       and starting clean without this step result present:
  if (! -d "$buildDir/target" || ! -s "$buildDir/target.list") {
    die "doXenoRefGene the previous step 'splitTarget' did not complete \n" .
      "successfully (target.list or target/*.fa.gz does not exists).\nPlease " .
      "complete the previous step: -continue=-splitTarget\n";
  } elsif (-e "$runDir/run.time") {
    die "doXenoRefGene looks like this step was run successfully already " .
      "(run.time exists).\nEither run with -continue filterPsl or some later " .
	"stage,\nor move aside/remove $runDir/ and run again.\n";
  } elsif ((-e "$runDir/jobList") && ! $opt_debug) {
    die "doXenoRefGene looks like we are not starting with a clean " .
      "slate.\n\tclean\n  $runDir/\n\tand run again.\n";
  }
  &HgAutomate::mustMkdir($runDir);

  my $templateCmd = ("blatOne " . '$(root1) ' . '$(root2) '
                . "{check out exists result/" . '$(root1)/$(root2).psl}');
  &HgAutomate::makeGsub($runDir, $templateCmd);
 `touch "$runDir/para_hub_$paraHub"`;

  my $whatItDoes = "runs blat mRNAs query sequence bundle to one target sequence";
  my $bossScript = newBash HgRemoteScript("$runDir/blatOne", $paraHub,
				      $runDir, $whatItDoes);

  $bossScript->add(<<_EOF_
export queryDir="$mrnas/mrnas";

export target=\$1
export query=\$2
export result=\$3
mkdir -p `dirname \$result`

blat -noHead -q=rnax -t=dnax -mask=lower ../target/\$target.fa.gz \$queryDir/\$query.fa.gz \$result

_EOF_
  );

  $whatItDoes = "Operate the blat run of the mRNAs query sequence to the target split sequence.";
  $bossScript = newBash HgRemoteScript("$runDir/runBlat.bash", $paraHub,
				      $runDir, $whatItDoes);
  my $paraRun = &HgAutomate::paraRun();
  $bossScript->add(<<_EOF_

chmod +x blatOne
gensub2 ../target.list $mrnas/query.list gsub jobList

$paraRun
_EOF_
  );
  $bossScript->execute();
} # doBlatRun

#########################################################################
# * step: filterPsl [workhorse]
sub doFilterPsl {
  my $runDir = $buildDir;
  &HgAutomate::mustMkdir($runDir);

  # First, make sure we're starting clean.
  if (! -e "$runDir/blatRun/run.time") {
    die "doFilterPsl the previous step blatRun did not complete \n" .
      "successfully ($buildDir/blatRun/run.time does not exist).\nPlease " .
      "complete the previous step: -continue=blatRun\n";
  } elsif (-e "$runDir/$db.xenoRefGene.psl" ) {
    die "doFilterPsl looks like this was run successfully already\n" .
      "($db.xenoRefGene.psl exists).  Either run with -continue makeGp or cleanup\n" .
	"or move aside/remove $runDir/$db.xenoRefGene.psl\nand run again.\n";
  }

  my $whatItDoes = "Filters the raw psl results from the blatRun.";
  my $bossScript = newBash HgRemoteScript("$runDir/filterPsl.bash", $workhorse,
				      $runDir, $whatItDoes);

  $bossScript->add(<<_EOF_
export db="$db"
find ./blatRun/result -type f | xargs cat \\
    | gnusort -S100G --parallel=32 -k10,10 > \$db.all.psl

pslCDnaFilter -minId=0.35 -minCover=0.25  -globalNearBest=0.0100 -minQSize=20 \\
  -ignoreIntrons -repsAsMatch -ignoreNs -bestOverlap \\
    \$db.all.psl \$db.xenoRefGene.psl

pslCheck -targetSizes=\$db.chrom.sizes \\
  -querySizes=/hive/data/genomes/asmHubs/VGP/xenoRefSeq/xenoRefMrna.sizes \\
     \$db.xenoRefGene.psl
_EOF_
  );
  $bossScript->execute();
} # doFilterPsl

#########################################################################
# * step: make gp [workhorse]
sub doMakeGp {
  my $runDir = $buildDir;
  &HgAutomate::mustMkdir($runDir);

  # First, make sure we're starting clean.
  if (! -e "$runDir/$db.xenoRefGene.psl") {
    die "doMakeGp: the previous step filterPsl did not complete \n" .
      "successfully ($buildDir/$db.xenoRefGene.psl does not exist).\nPlease " .
      "complete the previous step: -continue=filterPsl\n";
  } elsif (-e "$runDir/$db.xenoRefGene.bb" ) {
    die "doMakeGp: looks like this was run successfully already\n" .
      "($db.xenoRefGene.bb exists).  Either run with -continue cleanup\n" .
	"or move aside/remove $runDir/$db.xenoRefGene.bb\nand run again.\n";
  }

  my $whatItDoes = "Makes bigGenePred.bb file from filterPsl output.";
  my $bossScript = newBash HgRemoteScript("$runDir/makeGp.bash", $workhorse,
				      $runDir, $whatItDoes);

  $bossScript->add(<<_EOF_
export db=$db
grep NR_ \$db.xenoRefGene.psl > NR.psl
grep NM_ \$db.xenoRefGene.psl > NM.psl
mrnaToGene -cdsDb=hgFixed NM.psl NM.gp
mrnaToGene -noCds NR.psl NR.gp
cat NM.gp NR.gp | genePredSingleCover stdin \$db.xenoRefGene.gp
genePredCheck -db=\$db -chromSizes=\$db.chrom.sizes \$db.xenoRefGene.gp
genePredToBigGenePred -geneNames=$mrnas/geneOrgXref.txt \$db.xenoRefGene.gp \\
   stdout | sort -k1,1 -k2,2n > \$db.bgpInput
sed -e 's#Alternative/human readable gene name#species of origin of the mRNA#' \\
  \$HOME/kent/src/hg/lib/bigGenePred.as > xenoRefGene.as
bedToBigBed -extraIndex=name,geneName -type=bed12+8 -tab -as=xenoRefGene.as \\
   \$db.bgpInput \$db.chrom.sizes \$db.xenoRefGene.bb
_EOF_
  );
  $bossScript->execute();
} # doMakeGp

#########################################################################
# * step: cleanup [workhorse]
sub doCleanup {
  my $runDir = $buildDir;

  if (-e "$runDir/$db.xenoRefGene.gp.gz" ) {
    die "doCleanup: looks like this was run successfully already\n" .
      "($db.xenoRefGene.gp.gz exists).  Investigate the run directory:\n" .
	" $runDir/\n";
  }
  my $whatItDoes = "It cleans up or compresses intermediate files.";
  my $bossScript = newBash HgRemoteScript("$runDir/cleanup.bash", $workhorse,
				      $runDir, $whatItDoes);
  $bossScript->add(<<_EOF_
export db="$db"
rm -fr $buildDir/target/
rm -fr $buildDir/blatRun/err/
rm -fr $buildDir/blatRun/result/
rm -f $buildDir/blatRun/batch.bak
rm -f $buildDir/NM.gp
rm -f $buildDir/NR.gp
rm -f $buildDir/NM.psl
rm -f $buildDir/NR.psl
rm -f $buildDir/\$db.bgpInput
gzip $buildDir/\$db.all.psl &
gzip $buildDir/\$db.xenoRefGene.psl &
gzip $buildDir/\$db.xenoRefGene.gp
wait
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

$noDbGenePredCheck = $opt_noDbGenePredCheck ? 0 : $noDbGenePredCheck;

# Establish what directory we will work in.
$buildDir = $opt_buildDir ? $opt_buildDir :
  "$HgAutomate::clusterData/$db/$HgAutomate::trackBuild/augustus";
$maskedSeq = $opt_maskedSeq ? $opt_maskedSeq :
  "$HgAutomate::clusterData/$db/$db.2bit";
$mrnas = $opt_mrnas ? $opt_mrnas : $mrnas;

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

