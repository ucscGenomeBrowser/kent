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
      { name => 'makeBigGp', func => \&doMakeBigGp },
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
    makeGp:     Transform the filtered PSL into a genePred file
    makeBigGp:  Construct the bigGenePred from the genePred file
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
  if (! -e "$runDir/run.augustus/run.time") {
    die "doMakeGp: the previous step augustus did not complete \n" .
      "successfully ($buildDir/run.augustus/run.time does not exist).\nPlease " .
      "complete the previous step: -continue=augustus\n";
  } elsif (-e "$runDir/$db.augustus.bb" ) {
    die "doMakeGp: looks like this was run successfully already\n" .
      "($db.augustus.bb exists).  Either run with -continue load or cleanup\n" .
	"or move aside/remove $runDir/$db.augustus.bb\nand run again.\n";
  }

  my $whatItDoes = "Makes genePred file from augustus gff output.";
  my $bossScript = newBash HgRemoteScript("$runDir/makeGp.bash", $workhorse,
				      $runDir, $whatItDoes);

  my $dbCheck = "-db=$db";
  $dbCheck = "" if (0 == $noDbGenePredCheck);

  $bossScript->add(<<_EOF_
export db=$db
find ./run.augustus/gtf -type f | grep ".gtf.gz\$" \\
  | sed -e 's#/# _D_ #g; s#\\.# _dot_ #g;' \\
    | sort -k11,11 -k13,13n \\
     | sed -e 's# _dot_ #.#g; s# _D_ #/#g' | xargs zcat \\
       | $augustusDir/scripts/join_aug_pred.pl \\
          | grep -P "\\t(CDS|exon|stop_codon|start_codon|tts|tss)\\t" \\
            > \$db.augustus.gtf
gtfToGenePred -genePredExt -infoOut=\$db.info \$db.augustus.gtf \$db.augustus.gp
genePredCheck $dbCheck \$db.augustus.gp
genePredToBigGenePred \$db.augustus.gp stdout | sort -k1,1 -k2,2n > \$db.augustus.bgp
bedToBigBed -type=bed12+8 -tab -as=$ENV{'HOME'}/kent/src/hg/lib/bigGenePred.as \$db.augustus.bgp partition/\$db.chrom.sizes \$db.augustus.bb
getRnaPred -genePredExt -keepMasking -genomeSeqs=$maskedSeq \$db \$db.augustus.gp all \$db.augustusGene.rna.fa
getRnaPred -genePredExt -peptides -genomeSeqs=$maskedSeq \$db \$db.augustus.gp all \$db.augustusGene.faa
_EOF_
  );
  $bossScript->execute();
} # doMakeGp

#########################################################################
# * step: load [dbHost]
sub doLoadAugustus {
  my $runDir = $buildDir;
  &HgAutomate::mustMkdir($runDir);
  my $tableName = "augustusGene";

  if (! -e "$runDir/$db.augustus.gp") {
    die "doLoadAugustus: the previous step makeGp did not complete \n" .
      "successfully ($db.augustus.gp does not exists).\nPlease " .
      "complete the previous step: -continue=-makeGp\n";
  } elsif (-e "$runDir/fb.$db.$tableName.txt" ) {
    die "doLoadAugustus: looks like this was run successfully already\n" .
      "(fb.$db.$tableName.txt exists).  Either run with -continue cleanup\n" .
	"or move aside/remove\n$runDir/fb.$db.$tableName.txt and run again.\n";
  }
  my $whatItDoes = "Loads $db.augustus.gp.";
  my $bossScript = newBash HgRemoteScript("$runDir/loadAugustus.bash", $dbHost,
				      $runDir, $whatItDoes);

  my $dbCheck = "-db=$db";
  $dbCheck = "" if (0 == $noDbGenePredCheck);

  $bossScript->add(<<_EOF_
export db="$db"
export table="$tableName"
genePredCheck $dbCheck \$db.augustus.gp
hgLoadGenePred \$db -genePredExt \$table \$db.augustus.gp
genePredCheck $dbCheck \$table
featureBits \$db \$table > fb.\$db.\$table.txt 2>&1
checkTableCoords -verboseBlocks -table=\$table \$db
cat fb.\$db.\$table.txt
_EOF_
  );
  $bossScript->execute();
} # doLoad

#########################################################################
# * step: cleanup [workhorse]
sub doCleanup {
  my $runDir = $buildDir;

  if (-e "$runDir/augustus.gtf.gz" ) {
    die "doCleanup: looks like this was run successfully already\n" .
      "(augustus.gtf.gz exists).  Investigate the run directory:\n" .
	" $runDir/\n";
  }
  my $whatItDoes = "It cleans up or compresses intermediate files.";
  my $bossScript = newBash HgRemoteScript("$runDir/cleanup.bash", $workhorse,
				      $runDir, $whatItDoes);
  $bossScript->add(<<_EOF_
export db="$db"
rm -fr $buildDir/fasta
rm -fr $buildDir/run.augustus/err/
rm -f $buildDir/run.augustus/batch.bak
rm -fr $buildDir/run.augustus/augErr
rm -f $buildDir/\$db.augustus.bgp
gzip $buildDir/\$db.augustus.gtf
gzip $buildDir/\$db.augustus.gp
gzip $buildDir/\$db.augustusGene.rna.fa
gzip $buildDir/\$db.augustusGene.faa
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

