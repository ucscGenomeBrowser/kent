#!/usr/bin/env perl

# DO NOT EDIT the /cluster/bin/scripts copy of this file --
# edit ~/kent/src/hg/utils/automation/doEnsGeneUpdate.pl instead.

# $Id: doEnsGeneUpdate.pl,v 1.6 2008/02/23 00:20:10 hiram Exp $

use Getopt::Long;
use warnings;
use strict;
use FindBin qw($Bin);
use lib "$Bin";
use EnsGeneAutomate;
use HgAutomate;
use HgRemoteScript;
use HgStepManager;
use File::Basename;


# Option variable names, both common and peculiar to this script:
use vars @HgAutomate::commonOptionVars;
use vars @HgStepManager::optionVars;
use vars qw/
    $opt_buildDir
    /;


# Specify the steps supported with -continue / -stop:
my $stepper = new HgStepManager(
    [ { name => 'download',   func => \&doDownload },
      { name => 'process',   func => \&doProcess },
      { name => 'load',   func => \&doLoad },
      { name => 'cleanup', func => \&doCleanup },
    ]
				);

# Option defaults:
my $dbHost = 'hgwdev';


my $base = $0;
$base =~ s/^(.*\/)?//;
my (@versionList) = &EnsGeneAutomate::ensVersionList();
my $versionListString = join(", ", @versionList);

sub usage {
  # Usage / help / self-documentation:
  my ($status, $detailed) = @_;
  # Basic help (for incorrect usage):
  print STDERR "
usage: $base ensGeneConfig.ra
options:
";
  print STDERR $stepper->getOptionHelp();
  print STDERR <<_EOF_
    -buildDir dir         Use dir instead of default
                          $HgAutomate::clusterData/\$db/$HgAutomate::trackBuild/ensGene.<ensVersion #>
                          (necessary if experimenting with builds).
_EOF_
  ;
  print STDERR &HgAutomate::getCommonOptionHelp('dbHost' => $dbHost,
						'workhorse' => '',
						'fileServer' => '',
						'bigClusterHub' => '',
						'smallClusterHub' => '');
  print STDERR "
Automates UCSC's Ensembl gene updates.  Steps:
    download: fetch GFF and protein data files from Ensembl FTP site
    process: parse the GFF file, create coding and non-coding gff files
		lift random contigs to UCSC random coordinates
    load: load the coding and non-coding tracks, and the proteins
    cleanup: Removes or compresses intermediate files.
All operations are performed in the build directory which is
$HgAutomate::clusterData/\$db/$HgAutomate::trackBuild/ensGene.<ensVersion #> unless -buildDir is given.
ensGeneConfig.ra describes the species, assembly and downloaded files.
To see detailed information about what should appear in config.ra,
run \"$base -help\".
";
  # Detailed help (-help):
  if ($detailed) {
  print STDERR <<_EOF_
-----------------------------------------------------------------------------
Required ensGeneConfig.ra settings:

db xxxYyyN
  - The UCSC database name and assembly identifier.  xxx is the first three
    letters of the genus and Yyy is the first three letters of the species.
    N is the sequence number of this species' build at UCSC.  For some
    species that we have been processing since before this convention, the
    pattern is xyN.
ensVersion NN
  - The Ensembl version to fetch, possible values: $versionListString

Optional ensGeneConfig.ra settings:

liftRandoms yes
  - Need to lift Ensembl contig coordinates to UCSC _random coordinates.
  - Will use UCSC ctgPos information about contigs to perform the lift.
nameTranslation "<sed translation pattern>"
  - sed translation statement to change Ensembl chrom numbers and MT to
    UCSC chrN and chrM chrom names.  Example:
    nameTranslation "s/^\([0-9XY][0-9]*\)/chr\1/; s/^MT/chrM/"
geneScaffolds yes
  - need to translate Ensembl GeneScaffold coordinates to UCSC scaffolds
    Will fetch and use the Ensembl MySQL tables seq_region and assembly
    to perform the translation

Assumptions:
1. $HgAutomate::clusterData/\$db/\$db.2bit contains RepeatMasked sequence for
   database/assembly \$db.
2. $HgAutomate::clusterData/\$db/chrom.sizes contains all sequence names and sizes from
   \$db.2bit.
3. The \$db.2bit files have already been distributed to cluster-scratch
   (/scratch/hg or /iscratch, /san etc).
4. anything else here ?
_EOF_
  }
  print "\n";
  exit $status;
}



# Globals:
my ($species, $ensGtfUrl, $ensGtfFile, $ensPepUrl, $ensPepFile, $ensMySqlUrl);
# Command line argument:
my $CONFIG;
# Required config parameters:
my ($db, $ensVersion);
# Conditionally required config parameters:
my ($liftRandoms, $nameTranslation, $geneScaffolds, $knownToEnsembl);
# Other globals:
my ($topDir, $chromBased);
my ($bedDir, $scriptDir, $endNotes);
# Other:
my ($buildDir);

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
# * step: load [workhorse]
sub doLoad {
  my $runDir = "$buildDir";
  if (! -d "$buildDir/process") {
    die "ERROR: load: directory: '$buildDir/process' does not exist.\n" .
      "The process step appears to have not been done.\n" .
	"Run with -continue=process before this step.\n";
  }

  my $whatItDoes = "load processed ensGene data into local database.";
  my $bossScript = new HgRemoteScript("$runDir/doLoad.csh", $dbHost,
				      $runDir, $whatItDoes);

  if (defined $geneScaffolds) {
      $bossScript->add(<<_EOF_
hgLoadGenePred -skipInvalid -genePredExt $db ensGene process/$db.allGenes.gp.gz
zcat process/ensemblGeneScaffolds.oryCun1.bed.gz \\
    | sed -e "s/GeneScaffold/GS/" | hgLoadBed $db ensemblGeneScaffold stdin
_EOF_
      );
  } else {
  $bossScript->add(<<_EOF_
hgLoadGenePred -genePredExt $db ensGene process/$db.allGenes.gp.gz
_EOF_
      );
  }

  $bossScript->add(<<_EOF_
zcat download/$ensPepFile \\
        | sed -e 's/^>.* transcript:/>/; s/ CCDS.*\$//;' | gzip > ensPep.txt.gz
zcat ensPep.txt.gz \\
    | ~/kent/src/utils/faToTab/faToTab.pl /dev/null /dev/stdin \\
         | sed -e '/^\$/d; s/\*\$//' | sort > ensPep.$db.fa.tab
hgPepPred $db tab ensPep ensPep.$db.fa.tab
# verify names in ensGene is a superset of names in ensPep
hgsql -e "select name from ensPep;" $db | sort > ensPep.name
hgsql -e "select name from ensGene;" $db | sort > ensGene.name
set pepCount = `cat ensPep.name | wc -l`
set commonCount = `comm -12 ensPep.name ensGene.name | wc -l`
echo "peptide name count: \$pepCount, common name count: \$commonCount"
if (\$pepCount != \$commonCount) then
    echo "ERROR: load: gene name set does not include peptide name set"
    exit 255
endif
_EOF_
  );
  if (defined $knownToEnsembl) {
      $bossScript->add(<<_EOF_
hgMapToGene $db ensGene knownGene knownToEnsembl
_EOF_
      );
  }
  $bossScript->execute();
} # doLoad

#########################################################################
# * step: process [workhorse]
sub doProcess {
  my $runDir = "$buildDir/process";
  # First, make sure we're starting clean.
  if (-d "$runDir") {
    die "ERROR: process: looks like this was run successfully already\n" .
      "($runDir exists)\nEither run with -continue=load or some later\n" .
	"stage, or move aside/remove\n$runDir\nand run again.\n";
  }
  &HgAutomate::mustMkdir($runDir);

  my $whatItDoes = "process downloaded data from Ensembl FTP site into locally usable data.";
  my $fileServer = &HgAutomate::chooseFileServer($runDir);
  my $lifting = 0;
  my $bossScript;
  if (defined($liftRandoms) && $liftRandoms =~ m/yes/i) {
      $lifting = 1;
      $bossScript = new HgRemoteScript("$runDir/doProcess.csh", $dbHost,
				      $runDir, $whatItDoes);
  } else {
      $bossScript = new HgRemoteScript("$runDir/doProcess.csh", $fileServer,
				      $runDir, $whatItDoes);
  }
  #  if lifting, create the lift file
  if ($lifting) {
      $bossScript->add(<<_EOF_
rm -f randoms.$db.lift
foreach C (`cut -f1 /cluster/data/$db/chrom.sizes | grep random`)
   set size = `grep \$C /cluster/data/$db/chrom.sizes | cut -f2`
   hgsql -N -e \\
"select chromStart,contig,size,chrom,\$size from ctgPos where chrom='\$C';" \\
	$db  | awk '{gsub("\\\\.[0-9]+", "", \$2); print }' >> randoms.mm9.lift
end
_EOF_
      );
  }
  #  somg
  $bossScript->add(<<_EOF_
zcat ../download/$ensGtfFile \\
_EOF_
  );
  #  translate Ensemble chrom names to UCSC chrom name
  if (defined($nameTranslation)) {
  $bossScript->add(<<_EOF_
	| sed -e $nameTranslation \\
_EOF_
  );
  }
  #  lift randoms if necessary
  if ($lifting) {
  $bossScript->add(<<_EOF_
	| liftUp -type=.gtf stdout randoms.$db.lift carry stdin \\
_EOF_
  );
  }
  #  result is protein coding set
  $bossScript->add(<<_EOF_
	| gzip > allGenes.gtf.gz
_EOF_
  );
  $bossScript->add(<<_EOF_
gtfToGenePred -genePredExt allGenes.gtf.gz stdout | gzip > $db.allGenes.gp.gz
_EOF_
  );
  if (defined $geneScaffolds) {
      $bossScript->add(<<_EOF_
mv $db.allGenes.gp.gz $db.allGenes.beforeLift.gp.gz
$Bin/ensGeneScaffolds.pl ../download/seq_region.txt.gz \\
	../download/assembly.txt.gz | gzip > $db.ensGene.lft.gz
liftAcross -warn -bedOut=ensemblGeneScaffolds.$db.bed $db.ensGene.lft.gz \\
	$db.allGenes.beforeLift.gp.gz $db.allGenes.gp >& liftAcross.err.out
gzip ensemblGeneScaffolds.$db.bed $db.allGenes.gp liftAcross.err.out
_EOF_
      );
  }
  $bossScript->execute();
} # doProcess

#########################################################################
# * step: download [workhorse]
sub doDownload {
  my $runDir = "$buildDir/download";
  # First, make sure we're starting clean.
  if (-d "$runDir") {
    die "ERROR: download: looks like this was run successfully already\n" .
      "($runDir exists)\nEither run with -continue=process or some later\n" .
	"stage, or move aside/remove\n$runDir\nand run again.\n";
  }
  &HgAutomate::mustMkdir($runDir);

  my $whatItDoes = "download data from Ensembl FTP site.";
  my $fileServer = &HgAutomate::chooseFileServer($runDir);
  my $bossScript = new HgRemoteScript("$runDir/doDownload.csh", $fileServer,
				      $runDir, $whatItDoes);

  $bossScript->add(<<_EOF_
wget --timestamping --user=anonymous --password=ucscGenomeBrowser\@.ucsc.edu \\
$ensGtfUrl \\
-O $ensGtfFile
wget --timestamping --user=anonymous --password=ucscGenomeBrowser\@.ucsc.edu \\
$ensPepUrl \\
-O $ensPepFile
_EOF_
  );
  if (defined $geneScaffolds) {
      $bossScript->add(<<_EOF_
wget --timestamping --user=anonymous --password=ucscGenomeBrowser\@.ucsc.edu \\
$ensMySqlUrl/seq_region.txt.gz \\
-O seq_region.txt.gz
wget --timestamping --user=anonymous --password=ucscGenomeBrowser\@.ucsc.edu \\
$ensMySqlUrl/assembly.txt.gz \\
-O assembly.txt.gz
_EOF_
      );
  }
  $bossScript->execute();
} # doDownload


#########################################################################
# * step: cleanup [fileServer]
sub doCleanup {
  my $runDir = "$buildDir";
  my $whatItDoes = "It cleans up or compresses intermediate files.";
  my $fileServer = &HgAutomate::chooseFileServer($runDir);
  my $bossScript = new HgRemoteScript("$runDir/doCleanup.csh", $fileServer,
				      $runDir, $whatItDoes);
  $bossScript->add(<<_EOF_
rm -f bed.tab ensPep.txt.gz ensPep.$db.fa.tab ensPep.name ensGene.name
_EOF_
  );
  $bossScript->execute();
} # doCleanup

sub requireVar {
  # Ensure that var is in %config and return its value.
  # Remove it from %config so we can check for unrecognized contents.
  my ($var, $config) = @_;
  my $val = $config->{$var}
    || die "ERROR: requireVar: $CONFIG is missing required variable \"$var\".\n" .
      "For a detailed list of required variables, run \"$base -help\".\n";
  delete $config->{$var};
  return $val;
} # requireVar

sub optionalVar {
  # If var has a value in %config, return it.
  # Remove it from %config so we can check for unrecognized contents.
  my ($var, $config) = @_;
  my $val = $config->{$var};
  delete $config->{$var} if ($val);
  return $val;
} # optionalVar

sub parseConfig {
  # Parse config.ra file, make sure it contains the required variables.
  my ($configFile) = @_;
  my %config = ();
  my $fh = &HgAutomate::mustOpen($configFile);
  while (<$fh>) {
    next if (/^\s*#/ || /^\s*$/);
    if (/^\s*(\w+)\s*(.*)$/) {
      my ($var, $val) = ($1, $2);
      if (! exists $config{$var}) {
	$config{$var} = $val;
      } else {
	die "ERROR: parseConfig: Duplicate definition for $var line $. of config file $configFile.\n";
      }
    } else {
      die "ERROR: parseConfig: Can't parse line $. of config file $configFile:\n$_\n";
    }
  }
  close($fh);

  # Required variables.
  $db = &requireVar('db', \%config);
  $ensVersion = &requireVar('ensVersion', \%config);
  $species = &HgAutomate::getSpecies($dbHost, $db);
  &HgAutomate::verbose(1,
	"\n db: $db, species: '$species'\n");

  # Optional variables.
  $liftRandoms = &optionalVar('liftRandoms', \%config);
  $nameTranslation = &optionalVar('nameTranslation', \%config);
  $geneScaffolds = &optionalVar('geneScaffolds', \%config);
  $knownToEnsembl = &optionalVar('knownToEnsembl', \%config);
  #	verify they actually do say yes
  if (defined($liftRandoms) && $liftRandoms !~ m/^yes$/i) {
	undef $liftRandoms;
  }
  if (defined($geneScaffolds) && $geneScaffolds !~ m/^yes$/i) {
	undef $geneScaffolds;
  }
  if (defined($knownToEnsembl) && $knownToEnsembl !~ m/^yes$/i) {
	undef $knownToEnsembl;
  }

  # Make sure no unrecognized variables were given.
  my @stragglers = sort keys %config;
  if (scalar(@stragglers) > 0) {
    die "ERROR: parseConfig: config file $CONFIG has unrecognized variables:\n"
      . "    " . join(", ", @stragglers) . "\n" .
      "For a detailed list of supported variables, run \"$base -help\".\n";
  }
  $topDir = "/cluster/data/$db";
} # parseConfig


#########################################################################
# main

# Prevent "Suspended (tty input)" hanging:
&HgAutomate::closeStdin();

# Make sure we have valid options and exactly 1 argument:
&checkOptions();
&usage(1) if (scalar(@ARGV) != 1);

($CONFIG) = @ARGV;
&parseConfig($CONFIG);
$bedDir = "$topDir/$HgAutomate::trackBuild";

# Force debug and verbose until this is looking pretty solid:
# $opt_debug = 1;
$opt_verbose = 3 if ($opt_verbose < 3);

# Establish what directory we will work in, tack on the ensembl version ID.
$buildDir = $opt_buildDir ? $opt_buildDir :
  "$HgAutomate::clusterData/$db/$HgAutomate::trackBuild/ensGene.$ensVersion";

($ensGtfUrl, $ensPepUrl, $ensMySqlUrl) =
	&EnsGeneAutomate::ensGeneVersioning($db, $ensVersion );
die "ERROR: download: can not find Ensembl FTP URL for UCSC database $db"
	if (!defined($ensGtfUrl));
$ensGtfFile = basename($ensGtfUrl);
$ensPepFile = basename($ensPepUrl);
&HgAutomate::verbose(2,"Ensembl GTF URL: $ensGtfUrl\n");
&HgAutomate::verbose(2,"Ensembl GTF File: $ensGtfFile\n");
&HgAutomate::verbose(2,"Ensembl PEP URL: $ensPepUrl\n");
&HgAutomate::verbose(2,"Ensembl PEP File: $ensPepFile\n");
&HgAutomate::verbose(2,"Ensembl MySql URL: $ensMySqlUrl\n");

# Do everything.
$stepper->execute();

# Tell the user anything they should know.
my $stopStep = $stepper->getStopStep();
my $upThrough = ($stopStep eq 'cleanup') ? "" :
  "  (through the '$stopStep' step)";

&HgAutomate::verbose(1,
	"\n *** All done!$upThrough\n");
&HgAutomate::verbose(1,
	" *** Steps were performed in $buildDir\n");
&HgAutomate::verbose(1, "\n");

