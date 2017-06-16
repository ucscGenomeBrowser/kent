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
    $opt_twoBit
    /;

# Specify the steps supported with -continue / -stop:
my $stepper = new HgStepManager(
    [ { name => 'ranges',   func => \&doRanges },
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
my $workhorse = 'hgwdev';
my $defaultWorkhorse = 'hgwdev';
my $twoBit = "$HgAutomate::clusterData/\$db/\$db.2bit";

my $base = $0;
$base =~ s/^(.*\/)?//;

sub usage {
  # Usage / help / self-documentation:
  my ($status, $detailed) = @_;
  # Basic help (for incorrect usage):
  print STDERR "
usage: $base db geneTrack
options:
    db - UCSC database name
    geneTrack - name of track to use for source of genes, valid names:
                knownGene, ensGene, xenoRefGene
";
  print STDERR $stepper->getOptionHelp();
  print STDERR <<_EOF_
    -buildDir dir         Use dir instead of default:
                              $HgAutomate::clusterData/\$db/$HgAutomate::trackBuild/crispr
    -twoBit seq.2bit      Use seq.2bit as the input sequence instead
                              of default: ($twoBit).
_EOF_
  ;
  print STDERR &HgAutomate::getCommonOptionHelp('dbHost' => $dbHost,
						'workhorse' => $defaultWorkhorse,
						'bigClusterHub' => $bigClusterHub,
						'smallClusterHub' => '');
  print STDERR "
Automates UCSC's crispr procedure.  Steps:
    ranges: Construct ranges to work on around exons
    guides: Extract guide sequences from ranges
    cleanup: Removes or compresses intermediate files.
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
my ($db, $geneTrack);
# Other:
my ($buildDir, $secondsStart, $secondsEnd, $dbExists, $genomeFname,
    $chromSizes, $exonShoulder);

sub checkOptions {
  # Make sure command line options are valid/supported.
  my $ok = GetOptions(@HgStepManager::optionSpec,
		      'buildDir=s',
		      'twoBit=s',
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
# * step: ranges [workhorse]
sub doRanges {
  my $runDir = "$buildDir/ranges";
  my $inFaDir = "$buildDir/ranges/inFa";

  # First, make sure we're starting clean, or if done already, let
  #   it continue.
  if ( ! $opt_debug && ( -d "$runDir" ) ) {
    if (! -d "$inFaDir") {
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
printf "# getting genes\\n" 1>&2
hgsql $db -NB -e 'select name,chrom,strand,txStart,txEnd,cdsStart,cdsEnd,exonCount,exonStarts,exonEnds from '$geneTrack' where chrom not like "%_alt" and chrom not like "%hap%"; '  > genes.gp
printf "# Number of transcripts: %d\\n" "`cat genes.gp | wc -l`" 1>&2
printf "# break genes into exons and add 200 bp on each side\\n" 1>&2
genePredToBed genes.gp stdout | grep -v hap | grep -v chrUn \\
    | bedToExons stdin stdout \\
    | awk '{\$2=\$2-$exonShoulder; \$3=\$3+$exonShoulder; \$6="+"; print}' \\
    | bedClip stdin $chromSizes stdout | grep -v _alt \\
    | grep -i -v hap > ranges.bed

printf "# Get sequence, removing gaps. This can take 10-15 minutes\\n" 1>&2
printf "# featureBits of target ranges\\n" 1>&2
featureBits $db -not gap -bed=notGap.bed
featureBits $db ranges.bed notGap.bed -faMerge -fa=ranges.fa \\
    -minSize=20 -bed=stdout | cut -f-3 > crisprRanges.bed

printf "# split the sequence file into pieces for the cluster\\n" 1>&2
mkdir -p inFa/ outGuides
faSplit sequence ranges.fa 100 inFa/
_EOF_
  );
  $bossScript->execute();
} # doTemplate


#########################################################################
# * step: cleanup [fileServer]
sub doCleanup {
  my $runDir = "$buildDir";
  my $whatItDoes = "It cleans up or compresses intermediate files.";
  my $fileServer = &HgAutomate::chooseFileServer($runDir);
  my $bossScript = new HgRemoteScript("$runDir/doCleanup.csh", $fileServer,
				      $runDir, $whatItDoes);
  $bossScript->add(<<_EOF_
rm -rf run.template/raw/
rm -rf templateOtherBigTempFilesOrDirectories
gzip template
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
&usage(1) if (scalar(@ARGV) != 2);
$workhorse = $opt_workhorse if ($opt_workhorse);
$bigClusterHub = $opt_bigClusterHub if ($opt_bigClusterHub);
$dbHost = $opt_dbHost if ($opt_dbHost);
$secondsStart = `date "+%s"`;
chomp $secondsStart;

# required command line arguments:
($db, $geneTrack) = @ARGV;

# Establish directory to work in:
$buildDir = $opt_buildDir ? $opt_buildDir :
  "$HgAutomate::clusterData/$db/$HgAutomate::trackBuild/crispr";
$twoBit = $opt_twoBit ? $opt_twoBit : "$HgAutomate::clusterData/$db/$db.2bit";
&HgAutomate::verbose(1, "# 2bit file: $twoBit\n");
die "can not find 2bit file: '$twoBit'" if ( ! -s $twoBit);
$genomeFname = "$crisporSrc/genomes/$db/$db.fa.bwt";
die "can not find bwa index '$genomeFname'" if ( ! -s $genomeFname);
$chromSizes = "/hive/data/genomes/$db/chrom.sizes";
$exonShoulder = 10000;

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

