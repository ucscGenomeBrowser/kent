#!/usr/bin/env perl

# DO NOT EDIT the /cluster/bin/scripts copy of this file --
# edit ~/kent/src/hg/utils/automation/makeGenomeDb.pl instead.

# $Id: makeGenomeDb.pl,v 1.30 2010/04/13 23:18:44 hiram Exp $

use Getopt::Long;
use warnings;
use strict;
use FindBin qw($Bin);
use lib "$Bin";
use HgAutomate;
use HgRemoteScript;
use HgStepManager;


my $stepper = new HgStepManager(
    [ { name => 'seq',     func => \&makeBuildDir },
      { name => 'agp',     func => \&checkAgp },
      { name => 'db',      func => \&makeDb },
      { name => 'dbDb',    func => \&makeDbDb },
      { name => 'trackDb', func => \&makeTrackDb },
    ]
			       );

my $base = $0;
$base =~ s/^(.*\/)?//;

# Option defaults:
my $dbHost = 'hgwdev';
my $maxMitoSize = 25000;  ## Can be overridden in the config.ra file

sub usage {
  # Usage / help / self-documentation:
  my ($status, $detailed) = @_;
  # Basic help (for incorrect usage):
  print STDERR "
usage: $base config.ra
options:
";
  print STDERR $stepper->getOptionHelp();
  print STDERR &HgAutomate::getCommonOptionHelp('dbHost' => $dbHost,
						'workhorse' => '',
					        'fileServer' => '');
print STDERR <<_EOF_
    -splitGoldGap         split the gold/gap tables (default is not split)
    -noGoldGapSplit       default behavior, this option no longer required
    -forceDescription     construct a new description.html when -continue=trackDb
_EOF_
  ;
  print STDERR "
Automates the first steps of building a genome database:
    seq:     Translates fasta into /hive/data/genomes/\$db/\$db.unmasked.2bit .
    agp:     Checks consistency of fasta and AGP (or runs hgFakeAgp).
    db:      Creates the genome database and basic tables.
    dbDb:    Adds the genome database to central database tables.
    trackDb: Creates a trackDb/ checkout and adds -- but does not check in --
             template trackDb.ra and assembly description .html files.
config.ra describes the species, assembly and downloaded files.
To see detailed information about what should appear in config.ra,
run \"$base -help\".
";
  # Detailed help (-help):
  print STDERR "
-----------------------------------------------------------------------------
Required config.ra settings:

db xxxYyyN
  - The UCSC database name and assembly identifier.  xxx is the first three
    letters of the genus and Yyy is the first three letters of the species.
    N is the sequence number of this species' build at UCSC.  For some
    species that we have been processing since before this convention, the
    pattern is xyN.

scientificName Xxxxxx yyyyyy
  - The genus and species name.

assemblyDate Mmm. YYYY
  - The month and year in which this assembly was released by the sequencing
    center.

assemblyLabel XXXXXX
  - The detailed/long form of the sequencing center's label or version
    identifier for this release (e.g. 'Genome Center at Washington University,
    St. Louis, Genus_species 1.2.3').

assemblyShortLabel XXXXXX
  - The abbreviated form of the sequencing center's label or version identifier
    for this release (e.g. 'WUGSC 1.2.3').

photoCreditURL http://some.path/to/photo/credit.html
  - used to construct a courtesy URL reference to the source for the
    photograph on the gateway page

photoCreditName string for photo credit
  - used to label the photoCreditURL under the picture on the gateway page

ncbiGenomeId nnnnn
  - A numeric NCBI identifier for the genome information reference at
    https://www.ncbi.nlm.nih.gov/genome/nnnnn

ncbiAssemblyId nnnnn
  - A numeric NCBI identifier for the assembly. To determine this, do an
    NCBI Assembly query at https://www.ncbi.nlm.nih.gov/assembly/,
    using the scientific name \"Xxxxxx yyyyyy\" and choose a project ID that
    match the assembly name.

ncbiAssemblyName xxxxxrr
  - The assembly name used in the ftp path such as \"catChrV17e\" in
    ftp://ftp.ncbi.nlm.nih.gov/genbank/genomes/Eukaryotes/vertebrates_mammals/Felis_catus/catChrV17e/
    It is identical to the name returned from the NCBI Assembly query mention above.
    NOTE: this is NOT an NCBI identifier, this name was supplied by the
    assembly provider.

ncbiBioProject nnnnn
  - The NCBI bioproject number to construct the URL:
    https://www.ncbi.nlm.nih.gov/bioproject/nnnnn

ncbiBioSample nnnnn
  - A numeric NCBI identifier for the genome information reference at
    https://www.ncbi.nlm.nih.gov/biosample/nnnnn

genBankAccessionID GCA_nnn
  - The NCBI assembly accession identification from the ASSEMBLY_INFO file
    in the downloads from the FTP site ftp.ncbi.nlm.nih.gov/genbank/genomes

orderKey NN
  - A priority number (for the central database's dbDb.orderKey column)
    that will determine db's relative position in the assembly menu.

mitoAcc XXXXXXX
  - A numeric Genbank identifier (\"gi number\") for the complete
    mitochondrial sequence of this species, or \"none\" to disable fetching
    and inclusion in the build.  To determine this, do an Entrez Nucleotide
    query on \"Xxxxxx yyyyyy mitochondrion complete genome\" and choose a
    result that looks best (there may be multiple complete sequences in
    Genbank, or there may be none in which case just say \"none\").

fastaFiles /path/to/downloaded.fa
  - A complete path, possibly with wildcard characters, to FASTA sequence
    files which have already been downloaded from the sequencing center.

dbDbSpeciesDir dirName
  - The name of the subdirectory of trackDb/ in which the \$db subdirectory
    will be added.  For vertebrates, this is often a lower-cased common name,
    but various patterns have been used especially for non-vertebrates.

taxId ncbiTaxId
  - The NCBI numeric taxonomy id.  If a pseudo-genome, like a reconstruction
    is being built, specify 0.

-----------------------------------------------------------------------------
Conditionally required config.ra settings:

1. Required when AGP is not included in the assembly.  In this case we run
   hgFakeAgp to deduce gap annotations from runs of Ns in the sequence.
   The faGapSizes program shows a histogram that emphasizes overrepresented
   round-number sizes, which are decent hints for these parameters.

fakeAgpMinContigGap NN
  - hgFakeAgp -minContigGap param.  Minimum size for a run of Ns that
    will be considered a bridged gap with type \"contig\".

fakeAgpMinScaffoldGap NN
  - hgFakeAgp -minScaffoldGap param.  Minimum size for a run of Ns that
    will be considered an unbridged gap with type \"scaffold\".


2. Required when the central database table genomeClade does not yet have a
   row for this species:

clade cccccc
  - UCSC's \"clade\" category for this species, as specified in the central
    database tables clade and genomeClade.

genomeCladePriority NN
  - Relative priority of this species compared to others in the same clade.
    This central database query can be helpful in picking a value:
    select * from genomeClade where clade = '\$clade' order by priority;


-----------------------------------------------------------------------------
Optional config.ra settings:

commonName Xxxx
  - Common name to use as the menu label for this species (and in central
    database's genome column) instead of abbreviated scientific name.

agpFiles /path/to/downloaded.agp
  - A complete path, possibly with wildcard characters, to AGP files
    which have already been downloaded from the sequencing center.

qualFiles [/path/to/downloaded.qual | /path/to/qacAgpLift-ed.qac]
  - A complete path, possibly with wildcard characters, to quality score
    files which have already been downloaded from the sequencing center,
    or a complete path to a single .qac file (in case you need to pre-process
    qual files with qaToQac | qacAgpLift, for example).

mitoSize N
  - to override the internal default of max size for mitochondrial
    sequence of $maxMitoSize e.g. for yeast: mitoSize 90000

subsetLittleIds Y
  - ok if agp little ids (col6) are a subset of fasta sequences
    rather than requiring an exact match

doNotCheckDuplicates Y
  - do not stop build if duplicate sequences are found in genome

" if ($detailed);
  print STDERR "\n";
  exit $status;
} # usage


# Globals:
# Command line argument:
my $CONFIG;
# Command line option vars:
use vars @HgAutomate::commonOptionVars;
use vars @HgStepManager::optionVars;
# specific command line options
use vars qw/
    $opt_splitGoldGap
    $opt_noGoldGapSplit
    $opt_forceDescription
    /;

# Required config parameters:
my ($db, $scientificName, $assemblyDate, $assemblyLabel, $assemblyShortLabel, $orderKey, $photoCreditURL, $photoCreditName, $ncbiGenomeId, $providerAssemblyName, $ncbiAssemblyId, $ncbiBioProject, $ncbiBioSample, $genBankAccessionID,
    $mitoAcc, $fastaFiles, $dbDbSpeciesDir, $taxId);
# Conditionally required config parameters:
my ($fakeAgpMinContigGap, $fakeAgpMinScaffoldGap,
    $clade, $genomeCladePriority);
# Optional config parameters:
my ($commonName, $agpFiles, $doNotCheckDuplicates, $qualFiles, $mitoSize, $subsetLittleIds);
# Other globals:
my ($gotMito, $gotAgp, $gotQual, $topDir, $chromBased, $forceDescription);
my ($bedDir, $scriptDir, $endNotes);

sub checkOptions {
  # Make sure command line options are valid/supported.
  my $ok = GetOptions(@HgStepManager::optionSpec,
		      @HgAutomate::commonOptionSpec,
		      "splitGoldGap",
		      "noGoldGapSplit",
		      "forceDescription",
		     );
  &usage(1) if (!$ok);
  &usage(0, 1) if ($opt_help);
  &HgAutomate::processCommonOptions();
  my $err = $stepper->processOptions();
  usage(1) if ($err);
  $dbHost = $opt_dbHost if (defined $opt_dbHost);
  $forceDescription = 0;
  $forceDescription = 1 if (defined $opt_forceDescription)
} # checkOptions

sub requireVar {
  # Ensure that var is in %config and return its value.
  # Remove it from %config so we can check for unrecognized contents.
  my ($var, $config) = @_;
  my $val = $config->{$var}
    || die "Error: $CONFIG is missing required variable \"$var\".\n" .
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
	die "Duplicate definition for $var line $. of config file $configFile.\n";
      }
    } else {
      die "Can't parse line $. of config file $configFile:\n$_\n";
    }
  }
  close($fh);
  # Required variables.
  $db = &requireVar('db', \%config);
  $scientificName = &requireVar('scientificName', \%config);
  $assemblyDate = &requireVar('assemblyDate', \%config);
  $assemblyLabel = &requireVar('assemblyLabel', \%config);
  $assemblyShortLabel = &requireVar('assemblyShortLabel', \%config);
  $orderKey = &requireVar('orderKey', \%config);
  $mitoAcc = &requireVar('mitoAcc', \%config);
  $fastaFiles = &requireVar('fastaFiles', \%config);
  $dbDbSpeciesDir = &requireVar('dbDbSpeciesDir', \%config);
  $taxId = &requireVar('taxId', \%config);
  $photoCreditURL = &requireVar('photoCreditURL', \%config);
  $photoCreditName = &requireVar('photoCreditName', \%config);
  $ncbiGenomeId = &requireVar('ncbiGenomeId', \%config);
  $providerAssemblyName = &requireVar('ncbiAssemblyName', \%config);
  $ncbiAssemblyId = &requireVar('ncbiAssemblyId', \%config);
  $ncbiBioProject = &requireVar('ncbiBioProject', \%config);
  $ncbiBioSample = &requireVar('ncbiBioSample', \%config);
  $genBankAccessionID = &requireVar('genBankAccessionID', \%config);
  # Conditionally required variables -- optional here, but they might be
  # required later on in some cases.
  $fakeAgpMinContigGap = &optionalVar('fakeAgpMinContigGap', \%config);
  $fakeAgpMinScaffoldGap = &optionalVar('fakeAgpMinScaffoldGap', \%config);
  $clade = &optionalVar('clade', \%config);
  $genomeCladePriority = &optionalVar('genomeCladePriority', \%config);
  # Optional variables.
  $commonName = &optionalVar('commonName', \%config);
  $commonName =~ s/^(\w)(.*)/\u$1\L$2/;  # Capitalize only the first word
  $agpFiles = &optionalVar('agpFiles', \%config);
  $doNotCheckDuplicates = &optionalVar('doNotCheckDuplicates', \%config);
  $qualFiles = &optionalVar('qualFiles', \%config);
  $mitoSize = &optionalVar('mitoSize', \%config);
  $subsetLittleIds = &optionalVar('subsetLittleIds', \%config);
  # Make sure no unrecognized variables were given.
  my @stragglers = sort keys %config;
  if (scalar(@stragglers) > 0) {
    die "Error: config file $CONFIG has unrecognized variables:\n" .
      "    " . join(", ", @stragglers) . "\n" .
      "For a detailed list of supported variables, run \"$base -help\".\n";
  }
  $gotMito = ($mitoAcc ne 'none');
  $gotAgp = (defined $agpFiles);
  $gotQual = (defined $qualFiles);
  $topDir = "/cluster/data/$db";
} # parseConfig


#########################################################################
# * step: seq [workhorse]

sub mkClusterDataLink {
  # Make a /cluster/data/$tDb/ -> /cluster/store?/$tDb/ if it doesn't exist
  if (! -e "$topDir") {
    my $clusterStore = &HgAutomate::choosePermanentStorage();
    &HgAutomate::mustMkdir("$clusterStore/$db");
    # Don't use HgAutomate::run because this needs to happen despite -debug:
    (system("ln -s $clusterStore/$db $topDir") == 0)
      || die "Couldn't ln -s $clusterStore/$db $topDir\n";
  }
} # mkClusterDataLink

sub getMito {
  # Get the mitochondrion from Genbank (if specified) and rename it chrM
  if ($gotMito) {
    my $whatItDoes =
	"It fetches mitochondrial sequence from GenBank and renames it chrM.";
    # Use $dbHost because this needs to wget and some of our workhorses
    # can't do that, and this is computationally cheap.
    my $bossScript = new HgRemoteScript("$scriptDir/getMito.csh",
					$dbHost, $topDir, $whatItDoes,
					$CONFIG);
    if ($mitoSize) {
	$maxMitoSize = $mitoSize;
    }
    my $mitoFile = "$topDir/M/$mitoAcc.fa";
    $bossScript->add(<<_EOF_
mkdir M
wget -O $mitoFile \\
  'https://www.ncbi.nlm.nih.gov/sviewer/viewer.fcgi?db=nuccore&dopt=fasta&sendto=on&id=$mitoAcc'

# old url  'https://eutils.ncbi.nlm.nih.gov/entrez/eutils/efetch.fcgi?db=nuccore&rettype=fasta&id=$mitoAcc&retmode=text'


# Make sure there's exactly one fasta record:
if (`grep '^>' $mitoFile | wc -l` != 1) then
  echo "getMito: $mitoFile"
  echo "         does not have exactly one fasta record"
  exit 1
endif

# Make sure what we got is of about the right size:
set mSize = `faSize $mitoFile | grep bases | awk '{print \$1;}'`
if (\$mSize < 10000 || \$mSize > $maxMitoSize) then
  echo "getMito: $mitoFile"
  echo "         fasta size \$mSize is out of expected range [10000, $maxMitoSize]"
  exit 1
endif

# Make sure the fasta header looks plausible:
set keyCount = 0
set header = `grep '^>' $mitoFile`
foreach keyword ('$scientificName' mitochondr complete genome)
  set count = `echo \$header | grep -i "\$keyword" | wc -l`
  set keyCount = `expr \$keyCount + \$count`
end
if (\$keyCount < 2) then
  echo "getMito: $mitoFile"
  echo "         fasta header does not have very many expected keywords"
  echo "         (looking for '$scientificName' mitochondr complete genome)"
  echo "         Here is the header:"
  echo \$header
  exit 1
endif

# Make chrM.fa with >chrM:
sed -e 's/^>.*/>chrM/' $mitoFile > $topDir/M/chrM.fa
rm $mitoFile
_EOF_
    );
    $bossScript->execute();
  }
} # getMito

sub makeUnmasked2bit {
  my ($workhorse) = @_;
  my $whatItDoes =
"It installs assembly fasta (and agp, if given) files in the usual dirs.";
  my $bossScript = new HgRemoteScript("$scriptDir/makeUnmasked2bit.csh",
				      $workhorse, $topDir, $whatItDoes,
				      $CONFIG);

  my $chrM = $gotMito ? "$topDir/M/chrM.fa" : "";
  # Forget the fasta, just make an unmasked 2bit for now!
  # If we really need a fasta hierarchy, we can make it later, after
  # RepeatMasking!!  The whole point of the thing is to provide a
  # structure for the RM run and subsequent masking of the sequence.
  # But the RM+masking task can structure itself -- it can simply make
  # masked .2bit, and then subsequent jobs can do their own split and
  # distribute if necessary (or better yet just use 2bit specs).

  # possibilities for $fastaFiles:
  # {.fa, .fa.gz}
  # {many single-fasta, one multi-fasta, several multi-fasta}

  # If AGP is given:
  # 1. if first column of AGP matches sequence names, use as-is.
  # 2. if sixth column of AGP matches seqnames, assemble with agpToFa.

  # installation options:
  # 1. chrom-based (numSeqs <= 100): directories containing single-fasta,
  #    per-sequence files; randoms lumped into same directories with reals.
  # 2. scaffold-based: scaffolds.fa
  # Traditionally we have split to 5M in prep for RM -- but let the RM take
  # care of its own splitting (better yet, make it use 2bit specs).

  my $acat = "cat";
  my $fcat = "cat";
  my $sli = "";
  if (defined $subsetLittleIds && $subsetLittleIds eq "Y") {
    $sli = "-1 ";
  }
  foreach my $file (`ls $fastaFiles 2> /dev/null`) {
    if ($file =~ m/\.gz$/) {
      $fcat = "zcat";
      last;
    }
  }
  foreach my $file (`ls $agpFiles 2> /dev/null`) {
    if ($file =~ m/\.gz$/) {
      $acat = "zcat";
      last;
    }
  }
  if ($gotAgp) {
    $bossScript->add(<<_EOF_
# Get sorted IDs from fasta sequence files:
set fastaIds = `mktemp -p /tmp makeGenomeDb.fastaIds.XXXXXX`
$fcat $fastaFiles | grep '^>' | sed -e 's/^>.*gb\|/>/; s/\|.*//' | perl -wpe 's/^>(\\S+).*/\$1/' | sort \\
  > \$fastaIds

# Get sorted "big" (1st column) and "little" (6th column) IDs from AGP files:
set agpBigIds = `mktemp -p /tmp makeGenomeDb.agpIds.XXXXXX`
$acat $agpFiles | grep -v '^#' | awk '{print \$1;}' | sort -u \\
  > \$agpBigIds
set agpLittleIds = `mktemp -p /tmp makeGenomeDb.agpIds.XXXXXX`
$acat $agpFiles | grep -v '^#' | awk '((\$5 != "N") && (\$5 != "U")) {print \$6;}' | sort -u \\
  > \$agpLittleIds

# Compare fasta IDs to first and sixth columns of AGP:
set diffBigCount = `comm -3 \$fastaIds \$agpBigIds | wc -l`
set diffLittleCount = `comm $sli-3 \$fastaIds \$agpLittleIds | wc -l`

# If AGP "big" IDs match sequence IDs, use sequence as-is.
# If AGP "little" IDs match sequence IDs, or are a subset, assemble sequence with agpToFa.
set bigGenome = ""
#   big genomes are over 4Gb: 4*1024*1024*1024 = 4294967296
# requires -long argument to faToTwoBit
if (\$diffLittleCount == 0) then
  set agpTmp = `mktemp -p /tmp makeGenomeDb.agp.XXXXXX`
  $acat $agpFiles | grep -v '^#' > \$agpTmp
  set genomeSize = `$fcat $fastaFiles | sed -e 's/^>.*gb\|/>/; s/\|.*//' | agpToFa -simpleMultiMixed \$agpTmp all stdout stdin | faSize stdin | grep -w bases | awk '{print \$1}'`
  if ( \$genomeSize > 4294967295 ) then
    set bigGenome = "-long"
  endif
  $fcat $fastaFiles | sed -e 's/^>.*gb\|/>/; s/\|.*//' \\
  | agpToFa -simpleMultiMixed \$agpTmp all stdout stdin \\
  | faToTwoBit \$bigGenome -noMask stdin $chrM $db.unmasked.2bit
  rm -f \$agpTmp
else if (\$diffBigCount == 0) then
  set genomeSize = `$fcat $fastaFiles | sed -e 's/^>.*gb\|/>/; s/\|.*//' | faSize stdin | grep -w bases | awk '{print \$1}'`
  if ( \$genomeSize > 4294967295 ) then
    set bigGenome = "-long"
  endif
  $fcat $fastaFiles | sed -e 's/^>.*gb\|/>/; s/\|.*//' \\
  | faToTwoBit \$bigGenome -noMask stdin $chrM $db.unmasked.2bit
else
  echo "Error: IDs in fastaFiles ($fastaFiles)"
  echo "do not perfectly match IDs in either the first or sixth columns of"
  echo "agpFiles ($agpFiles)"
  echo "Please examine and then remove these temporary files:"
  echo "  \$fastaIds -- fasta IDs"
  echo "  \$agpBigIds -- AGP first column IDs ('big' sequences)"
  echo "  \$agpLittleIds -- AGP sixth column IDs ('little' sequences)"
  exit 1
endif
rm -f \$fastaIds \$agpBigIds \$agpLittleIds
_EOF_
    );
  } else {
    # No AGP -- just make an unmasked 2bit.
    $bossScript->add(<<_EOF_
set bigGenome = ""
#   big genomes are over 4Gb: 4*1024*1024*1024 = 4294967296
# requires -long argument to faToTwoBit
set genomeSize = `$fcat $fastaFiles | sed -e 's/^>.*gb\|/>/; s/\|.*//' | faSize stdin | grep -w bases | awk '{print \$1}'`
if ( \$genomeSize > 4294967295 ) then
  set bigGenome = "-long"
endif
$fcat $fastaFiles | sed -e 's/^>.*gb\|/>/; s/\|.*//' | \\
    faToTwoBit \$bigGenome -noMask stdin $chrM $db.unmasked.2bit
_EOF_
    );
  }

  # Having made the unmasked .2bit, make chrom.sizes and chromInfo.tab:
  # verify no dots allowed in chrom names
  if (! defined $doNotCheckDuplicates || ($doNotCheckDuplicates eq "N")) {
  $bossScript->add(<<_EOF_
twoBitDup $db.unmasked.2bit > jkStuff/twoBitDup.txt
if (`wc -l < jkStuff/twoBitDup.txt` > 0) then
  echo "ERROR: duplicate sequence found in $db.unmasked.2bit"
  exit 1
endif
_EOF_
  );
  }


  $bossScript->add(<<_EOF_
twoBitInfo $db.unmasked.2bit stdout | sort -k2nr > chrom.sizes

# if no dots in chrom names, should have only one kind of field size:
set noDots = `cut -f 1 chrom.sizes | awk -F'.' '{print NF}' | sort -u | wc -l`

if ( \$noDots != 1 ) then
  echo "ERROR: no dots allowed in chrom names !  e.g.:"
  grep "\." chrom.sizes | head -3
  exit 1
endif
# only one kind of field size and it must be simply a one:
set dotCount = `cut -f 1 chrom.sizes | awk -F'.' '{print NF}' | sort -u`
if ( \$dotCount != 1 ) then
  echo "ERROR: no dots allowed in chrom names !  e.g.:"
  grep "\." chrom.sizes | head -3
  exit 1
endif

rm -rf $HgAutomate::trackBuild/chromInfo
mkdir $HgAutomate::trackBuild/chromInfo
awk '{print \$1 "\t" \$2 "\t$HgAutomate::gbdb/$db/$db.2bit";}' chrom.sizes \\
  > $HgAutomate::trackBuild/chromInfo/chromInfo.tab
_EOF_
    );
  if ($gotAgp) {
    my $splitThreshold = $HgAutomate::splitThreshold;
    $bossScript->add(<<_EOF_
if (`wc -l < chrom.sizes` < $splitThreshold) then
  # Install per-chrom .agp files for download.
  $acat $agpFiles | grep -v '^#' \\
  | splitFileByColumn -col=1 -ending=.agp stdin $topDir -chromDirs
endif
_EOF_
      );
  }
  $bossScript->execute();

  # Now that we have created chrom.sizes (unless we're in -debug mode),
  # re-evaluate $chromBased for subsequent steps:
  if (-e "$topDir/chrom.sizes") {
    $chromBased = (`wc -l < $topDir/chrom.sizes` < $HgAutomate::splitThreshold);
  }
} # makeUnmasked2bit

sub makeBuildDir {
  # * step: seq [workhorse]
  &mkClusterDataLink();
  &HgAutomate::checkCleanSlate('seq', 'agp',
			       "$topDir/M", "$topDir/chrom.sizes");
  &HgAutomate::mustMkdir($scriptDir);
  &HgAutomate::mustMkdir($bedDir);
  my $workhorse = &HgAutomate::chooseWorkhorse();
  &getMito();
  &makeUnmasked2bit($workhorse);
} # makeBuildDir


#########################################################################
# * step: agp [workhorse]

sub requireFakeAgpParams {
  # When assembly does not include AGP, run hgFakeAgp and require the
  # developer to specify its parameters.  If not specified, run faGapSizes
  # to give some hints.
  my $problem = 0;
  if (! defined $fakeAgpMinContigGap) {
    warn "Error: $CONFIG is missing required variable " .
      "\"fakeAgpMinContigGap\".\n";
    $problem = 1;
  }
  if (! defined $fakeAgpMinScaffoldGap) {
    warn "Error: $CONFIG is missing required variable " .
      "\"fakeAgpMinScaffoldGap\".\n";
    $problem = 1;
  }
  if ($problem) {
    warn "\nWhen the assembly does not include AGP, $CONFIG must specify " .
      "fakeAgpMinContigGap and fakeAgpMinScaffoldGap.\n";
    warn "Printing a gap-size histogram from faGapSizes to stdout... " .
      "Check for overrepresented round numbers.\n";
    warn "See usage messages of hgFakeAgp and faGapSizes for more hints.\n\n";
    my $fileServer = &HgAutomate::chooseFileServer($topDir);
    print "\n";
    &HgAutomate::run("$HgAutomate::runSSH $fileServer nice " .
		     "twoBitToFa $topDir/$db.unmasked.2bit stdout " .
		     "\\| faGapSizes stdin -niceSizes=" .
		     "10,20,25,50,100,1000,2000,5000,10000,20000,50000");
    print "\n";
    exit 1;
    }
}

sub checkAgp {
  my $workhorse = &HgAutomate::chooseWorkhorse();
  # Check or generate AGP, depending on whether it was provided:
  if ($gotAgp) {
    &HgAutomate::checkCleanSlate('agp', 'db', "$topDir/$db.agp");
    my $whatItDoes = "It checks consistency of AGP and fasta.";
    my $bossScript = new HgRemoteScript("$scriptDir/checkAgpAndFa.csh",
					$workhorse, $topDir, $whatItDoes,
					$CONFIG);
    my $allAgp = "$topDir/$db.agp";
    # If we added chrM from GenBank, exclude it from fasta:
    my $exclude = ($mitoAcc eq 'none') ? "" : "-exclude=chrM";
    my $acat = "cat";
    foreach my $file (`ls $agpFiles 2> /dev/null`) {
	if ($file =~ m/\.gz$/) {
	    $acat = "zcat";
	    last;
	}
    }
    $bossScript->add(<<_EOF_
# When per-chrom AGP and fasta files are given, it would be much more
# efficient to run this one chrom at a time.  However, since the filenames
# are arbitrary, I'm not sure how to identify the pairings of AGP and fa
# files.  So cat 'em all together and check everything at once:

$acat $agpFiles | grep -v '^#' | sort -k1,1 -k2n,2n > $allAgp

set result = `checkAgpAndFa $exclude $allAgp $db.unmasked.2bit | tail -1`

if ("\$result" != 'All AGP and FASTA entries agree - both files are valid') then
  echo "Error: checkAgpAndFa failed\\!"
  echo "Last line of output: \$result"
  exit 1
endif
_EOF_
    );
    $bossScript->execute();
  } else {
    my $runDir = "$bedDir/hgFakeAgp";
    &HgAutomate::mustMkdir($runDir);
    &HgAutomate::checkCleanSlate('agp', 'db', "$runDir/$db.agp");
    &requireFakeAgpParams();
    my $whatItDoes = "It runs hgFakeAgp (because no AGP was provided).";
    my $bossScript = new HgRemoteScript("$runDir/doFakeAgp.csh",
					$workhorse, $runDir, $whatItDoes,
					$CONFIG);
    $bossScript->add(<<_EOF_
twoBitToFa $topDir/$db.unmasked.2bit stdout \\
| hgFakeAgp stdin $db.agp \\
  -minContigGap=$fakeAgpMinContigGap -minScaffoldGap=$fakeAgpMinScaffoldGap
_EOF_
    );
    $bossScript->execute();
  }

#*** Produce a gap report so we can say something sensible in gap.html.

} # checkAgp


#########################################################################
# * step: db [dbHost]

sub makeDb {
  # Create a database on hgwdev, grp, chromInfo, gold/gap.
  my $qual = (defined $qualFiles) ? " qual" : "";
  my $whatItDoes =
"It creates the genome database ($db) and loads the most basic tables:
chromInfo, grp, gap, gold,$qual and gc5Base.";
  my $bossScript = new HgRemoteScript("$scriptDir/makeDb.csh", $dbHost,
				      $topDir, $whatItDoes, $CONFIG);

  # Actually, build some basic track files on $workhorse, then load.
  $qual = (defined $qualFiles) ? " qual and" : "";
  $whatItDoes = "It generates$qual gc5Base track files for loading.";
  my $workhorse = &HgAutomate::chooseWorkhorse();
  my $horseScript = new HgRemoteScript("$scriptDir/makeTrackFiles.csh",
				       $workhorse,
				       $topDir, $whatItDoes, $CONFIG);
  # Build quality wiggle track files (if provided).
  if (defined $qualFiles) {
    $horseScript->add(<<_EOF_
# Translate qual files to wiggle encoding.  If there is a problem with
# sequence names, you may need to tweak the original qual sequence names
# and/or lift using qacAgpLft.
mkdir -p $bedDir/qual
cd $bedDir/qual
_EOF_
      );
    if ($qualFiles =~ /^\S+\.qac$/) {
      # Single .qac file:
      $horseScript->add(<<_EOF_
qacToWig -fixed $qualFiles stdout | gzip -c > $db.qual.wigVarStep.gz
_EOF_
        );
    } else {
      # Possible wildcard of qual file(s):
      $horseScript->add(<<_EOF_
if (`ls $qualFiles | grep \.gz | wc -l`) then
  set qcat = zcat
else
  set qcat = cat
endif
\$qcat $qualFiles \\
| qaToQac stdin stdout \\
| qacToWig -fixed stdin stdout | gzip -c > $db.qual.wigVarStep.gz
_EOF_
      );
    }
    $horseScript->add(<<_EOF_
wigToBigWig $db.qual.wigVarStep.gz ../../chrom.sizes $db.quality.bw
_EOF_
    );
  }

  # Build gc5Base files.
  $horseScript->add(<<_EOF_
# Make gc5Base wiggle files.
mkdir -p $bedDir/gc5Base
cd $bedDir/gc5Base
hgGcPercent -wigOut -doGaps -file=stdout -win=5 -verbose=0 $db \\
  $topDir/$db.unmasked.2bit | gzip -c > $db.gc5Base.wigVarStep.gz
wigToBigWig $db.gc5Base.wigVarStep.gz ../../chrom.sizes $db.gc5Base.bw
_EOF_
  );

  # Now start the database creation and loading commands.
  $bossScript->add (<<_EOF_
hgsql '' -e 'create database $db'
df -h /var/lib/mysql
hgsql $db < \${HOME}/kent/src/hg/lib/grp.sql
cut -f1 $HgAutomate::trackBuild/chromInfo/chromInfo.tab | awk '{if (length(\$0)>32) exit(1);}'  || echo Attention annotator: Your chromosome names are longer than 32 character. This will crash this script and lead to error messages by featureBits and everything that uses hdb.c
cut -f1 $HgAutomate::trackBuild/chromInfo/chromInfo.tab | awk '{print length(\$0)}' | sort -nr > $HgAutomate::trackBuild/chromInfo/t.chrSize
set chrSize = `head -1 $HgAutomate::trackBuild/chromInfo/t.chrSize`
sed -e "s/chrom(16)/chrom(\$chrSize)/" \${HOME}/kent/src/hg/lib/chromInfo.sql > $HgAutomate::trackBuild/chromInfo/chromInfo.sql
rm -f $HgAutomate::trackBuild/chromInfo/t.chrSize
hgLoadSqlTab $db chromInfo $HgAutomate::trackBuild/chromInfo/chromInfo.sql \\
  $HgAutomate::trackBuild/chromInfo/chromInfo.tab
_EOF_
  );

  my $allAgp = "$topDir/$db.agp";
  $allAgp = "$bedDir/hgFakeAgp/$db.agp" if (! $gotAgp);
  if ($chromBased) {
    if ($opt_splitGoldGap) {
      $bossScript->add(<<_EOF_
# Split AGP into per-chrom files/dirs so we can load split gold and gap tables.
cp /dev/null chrom.lst.tmp
foreach chr (`awk '{print \$1;}' chrom.sizes`)
  set c = `echo \$chr | sed -e 's/^chr//; s/_random\$//;'`
  mkdir -p \$c
  echo \$c >> chrom.lst.tmp
  awk '\$1 == "'\$chr'" {print;}' $allAgp > \$c/\$chr.agp
end
sort -u chrom.lst.tmp > chrom.lst
rm chrom.lst.tmp
_EOF_
      );
      $bossScript->add("hgGoldGapGl -noGl -chromLst=chrom.lst $db $topDir .\n");
    } else {
      $bossScript->add("hgGoldGapGl -noGl $db $allAgp\n");
    }
  } else {
    $bossScript->add("hgGoldGapGl -noGl $db $allAgp\n");
  }

  if ($gotAgp && $gotMito) {
    # When we load gold/gap from assembly AGP, but pull in chrM sequence
    # separately, chrM is conspicuously absent from gold/gap -- so add a fake
    # entry for it in gold (so featureBits gold --> 100%) and if split tables,
    # make a token chrM_gap table.  Use bin=585 (512+ 64 + 8 + 1), the
    # smallest bin that starts at 0.  The smallest bin is 128k bases, which
    # should always cover the entire mitochondrial genome (typically ~16k).
    my $bin = 585;
    my $mitoGold = ($mitoAcc =~ /^\d+$/) ? "gi|$mitoAcc" : $mitoAcc;
    if ($opt_splitGoldGap && ($chromBased || $opt_debug)) {
      my $defaultChrom = `head -1 $topDir/chrom.sizes | cut -f 1`;
      chomp $defaultChrom;
      $bossScript->add(<<_EOF_

# Add fake chrM_{gap,gold} to make featureBits happy.
set mSize = `faSize $topDir/M/chrM.fa | grep bases | awk '{print \$1;}'`
hgsql $db -e 'drop table chrM_gap,chrM_gold;'
hgsql $db -e \\
    'create table chrM_gap select * from ${defaultChrom}_gap where 0; \\
     create table chrM_gold select * from ${defaultChrom}_gold where 0; \\
     insert into chrM_gold \\
       values($bin,"chrM",0,'\$mSize',1,"F","$mitoGold",0,'\$mSize',"+");'
_EOF_
      );
    } else {
      $bossScript->add(<<_EOF_

# Add fake chrM entry in gold table to make featureBits happy.
set mSize = `faSize $topDir/M/chrM.fa | grep bases | awk '{print \$1;}'`
hgsql $db -e \\
    'insert into gold \\
        values($bin,"chrM",0,'\$mSize',1,"F","$mitoGold",0,'\$mSize',"+");'
_EOF_
      );
    }
    # may as well finally add the chrM entry to the agp file
    $bossScript->add(<<_EOF_
set lastId = `tail -1 $topDir/$db.agp | awk '{print \$4+1}'`
/bin/echo -e "chrM\t1\t\$mSize\t\$lastId\tF\t$mitoGold\t1\t\$mSize\t+" >> $topDir/$db.agp
_EOF_
      );
  }

  $bossScript->add(<<_EOF_
# verify gold and gap tables cover everything
featureBits -or -countGaps $db gold gap >&fb.$db.gold.gap.txt
cat fb.$db.gold.gap.txt
set allCovered = `awk '{print \$4-\$1}' fb.$db.gold.gap.txt`
if (\$allCovered != 0) then
    echo "ERROR: gold and gap tables do not cover whole genome"
    exit 255
endif
_EOF_
      );

  $bossScript->add(<<_EOF_

# Load gc5base
mkdir -p $HgAutomate::gbdb/$db/bbi/gc5BaseBw
rm -f $HgAutomate::gbdb/$db/bbi/gc5BaseBw/gc5Base.bw
ln -s $bedDir/gc5Base/$db.gc5Base.bw $HgAutomate::gbdb/$db/bbi/gc5BaseBw/gc5Base.bw
hgsql $db -e 'drop table if exists gc5BaseBw; \\
            create table gc5BaseBw (fileName varchar(255) not null); \\
            insert into gc5BaseBw values ("$HgAutomate::gbdb/$db/bbi/gc5BaseBw/gc5Base.bw");'
_EOF_
  );
  if (defined $qualFiles) {
    $bossScript->add(<<_EOF_

# Load qual
mkdir -p $HgAutomate::gbdb/$db/bbi/qualityBw
rm -f $HgAutomate::gbdb/$db/bbi/qualityBw/quality.bw
ln -s $bedDir/qual/$db.quality.bw $HgAutomate::gbdb/$db/bbi/qualityBw/quality.bw
hgsql $db -e 'drop table if exists qualityBw; \\
            create table qualityBw (fileName varchar(255) not null); \\
            insert into qualityBw values ("$HgAutomate::gbdb/$db/bbi/qualityBw/quality.bw");'
_EOF_
    );
  }
  $horseScript->execute();
  $bossScript->execute();
} # makeDb


#########################################################################
# * step: dbDb [dbHost]

sub requireCladeAndPriority {
  # If the genomeClade table in the central database does not have a row
  # for this $db's genome, require user to specify clade and priority.
  my ($genome) = @_;
  my $problem = 0;
  if (! defined $clade) {
    warn "Error: $CONFIG is missing required variable " .
      "\"clade\".\n";
    $problem = 1;
  }
  if (! defined $genomeCladePriority) {
    warn "Error: $CONFIG is missing required variable " .
      "\"genomeCladePriority\".\n";
    $problem = 1;
  }
  if ($problem) {
    warn "\nCentral database table genomeClade does not have a row for " .
      "${db}'s genome \"$genome\",\n";
    warn "so $CONFIG must specify clade and genomeCladePriority.\n";
    warn "Examine the contents of the genomeClade table to get an idea " .
      "where \"$genome\" fits in.\n\n";
    exit 1;
  }
}

sub getGenome {
  # Return what we'll use in the genome (and organism) column of dbDb.
  # This is the label that appears in the gateway menu.
  my $genome;
  if ($commonName) {
    $genome = $commonName;
  } else {
    $genome = $scientificName;
    $genome =~ s/^(\w)\w+\s+(\w+)$/$1. $2/;
  }
  return $genome;
}

sub makeDbDb {
# * step: dbDb [dbHost... or just direct to hgcentraltest??]
# - create hgcentraltest entry, (if necessary) defaultDb and genomeClade
  my $genome = &getGenome();
  my $defaultPos;
  my ($seq, $size) = $opt_debug ? ("chr1", 1000) :
    split(/\s/, `head -1 "$topDir/chrom.sizes"`);
  my $start = int($size / 2);
  $size = ($start + 9999) if ($size > ($start + 9999));
  $defaultPos = "$seq:$start-$size";

  my $dbDbInsert = "$topDir/dbDbInsert.sql";
  my $fh = &HgAutomate::mustOpen(">$dbDbInsert");
  print $fh <<_EOF_
DELETE from dbDb where name = "$db";
INSERT INTO dbDb
    (name, description, nibPath, organism,
     defaultPos, active, orderKey, genome, scientificName,
     htmlPath, hgNearOk, hgPbOk, sourceName, taxId)
VALUES
    ("$db", "$assemblyDate ($assemblyShortLabel/$db)", "$HgAutomate::gbdb/$db", "$genome",
     "$defaultPos", 1, $orderKey, "$genome", "$scientificName",
     "$HgAutomate::gbdb/$db/html/description.html", 0, 0, "$assemblyLabel",
    $taxId);
_EOF_
  ;
  close($fh);
  my $centDbSql = "$HgAutomate::runSSH $dbHost $HgAutomate::centralDbSql";
  &HgAutomate::run("$centDbSql < $dbDbInsert");

  # Add a row to defaultDb if this is the first usage of $genome.
  my $quotedGenome = $genome;
  $quotedGenome =~ s/'/'"'"'/g;
  my $sql = "'select count(*) from defaultDb where genome = \"$quotedGenome\"'";
  if (`echo $sql | $centDbSql` == 0) {
    $sql = "'INSERT INTO defaultDb (genome, name) " .
      "VALUES (\"$quotedGenome\", \"$db\")'";
    &HgAutomate::run("echo $sql | $centDbSql");
  }

  # If $genome does not already appear in genomeClade, warn user that
  # they will have to manually add it.
  $sql = "'select count(*) from genomeClade where genome = \"$quotedGenome\"'";
  if (`echo $sql | $centDbSql` == 0) {
    &requireCladeAndPriority($genome);
    $sql = "'INSERT INTO genomeClade (genome, clade, priority) " .
      "VALUES (\"$quotedGenome\", \"$clade\", $genomeCladePriority)'";
    &HgAutomate::run("echo $sql | $centDbSql");
  }
} # makeDbDb


#########################################################################
# * step: trackDb [dbHost]

sub makeDescription {
  # Make a template description.html (for the browser gateway page).
  my $sciUnderscore = $scientificName;
  $sciUnderscore =~ s/ /_/g;
  my $genome = &getGenome();
  my $anchorRoot = lc($genome);
  $anchorRoot =~ s/\. /_/;
  $anchorRoot =~ s/ /_/;
  my $imgExtn = "jpg";
  my $img = "/usr/local/apache/htdocs/images/$sciUnderscore.$imgExtn";
  if ( ! -s $img ) {
      $imgExtn = "png";
      $img = "/usr/local/apache/htdocs/images/$sciUnderscore.$imgExtn";
      if ( ! -s $img ) {
        $imgExtn = "gif";
        $img = "/usr/local/apache/htdocs/images/$sciUnderscore.$imgExtn";
          if ( ! -s $img ) {
            warn "missing htdocs/images/$sciUnderscore.{jpg|png|gif}\n\n";
            exit 1;
          }
      }
  }
  my $widthHeight = `identify $img | awk '{print \$3}'`;
  chomp $widthHeight;
  my ($width, $height) = split('x', $widthHeight);
  my $borderWidth = $width + 15;

  my $fh = &HgAutomate::mustOpen(">$topDir/html/description.html");
  print $fh <<_EOF_
<!-- Display image in righthand corner -->
<TABLE ALIGN=RIGHT BORDER=0 WIDTH=$borderWidth>
  <TR><TD ALIGN=RIGHT>
    <A HREF="https://www.ncbi.nlm.nih.gov/genome/$ncbiGenomeId" TARGET=_blank>
    <IMG SRC="../images/$sciUnderscore.$imgExtn" WIDTH=$width HEIGHT=$height ALT="$genome"></A>
  </TD></TR>
  <TR><TD ALIGN=RIGHT>
    <FONT SIZE=-1><em>$scientificName</em><BR>
    </FONT>
    <FONT SIZE=-2>
      (<A HREF="$photoCreditURL"
      TARGET=_blank>$photoCreditName</A>)
    </FONT>
  </TD></TR>
</TABLE>

<P>
<B>UCSC Genome Browser assembly ID:</B> $db<BR>
<B>Sequencing/Assembly provider ID:</B> $assemblyLabel $providerAssemblyName<BR>
<B>Assembly date:</B> $assemblyDate<BR>
<B>Accession ID:</B> $genBankAccessionID<BR>
_EOF_
  ;
  if ($ncbiGenomeId ne "n/a") {
  printf $fh "<B>NCBI Genome ID:</B> <A HREF='https://www.ncbi.nlm.nih.gov/genome/$ncbiGenomeId'
TARGET='_blank'>$ncbiGenomeId</A> ($scientificName)<BR>\n";
  } else {
  printf $fh "<B>NCBI Genome ID:</B> $ncbiGenomeId<BR>\n";
  }
  printf $fh "<B>NCBI Assembly ID:</B> <A HREF='https://www.ncbi.nlm.nih.gov/assembly/$ncbiAssemblyId'
TARGET='_blank'>$ncbiAssemblyId</A><BR>
<B>NCBI BioProject ID:</B> <A HREF='https://www.ncbi.nlm.nih.gov/bioproject/$ncbiBioProject'
TARGET='_blank'>$ncbiBioProject</A><BR>\n";
  if ($ncbiBioSample ne "n/a") {
    printf $fh "<B>NCBI BioSample ID:</B> <A HREF='https://www.ncbi.nlm.nih.gov/biosample/$ncbiBioSample'
TARGET='_blank'>$ncbiBioSample</A>\n";
  } else {
    printf $fh "<B>NCBI BioSample ID:</B> $ncbiBioSample<BR>\n";
  }
  print $fh <<_EOF_
</P>
<HR>
<P>
<B>Search the assembly:</B>
<UL>
<LI>
<B>By position or search term: </B> Use the &quot;position or search term&quot;
box to find areas of the genome associated with many different attributes, such
as a specific chromosomal coordinate range; mRNA, EST, or STS marker names; or
keywords from the GenBank description of an mRNA.
<A HREF="../goldenPath/help/query.html">More information</A>, including sample
queries.
<LI>
<B>By gene name: </B> Type a gene name into the &quot;search term&quot; box,
choose your gene from the drop-down list, then press &quot;submit&quot; to go
directly to the assembly location associated with that gene.
<A HREF="../goldenPath/help/geneSearchBox.html">More information</A>.
<LI>
<B>By track type: </B> Click the &quot;track search&quot; button
to find Genome Browser tracks that match specific selection criteria.
<A HREF="../goldenPath/help/trackSearch.html">More information</A>.
</UL>
</P>
<HR>
<P>
<B>Download sequence and annotation data:</B>
<UL>
<LI><A HREF="../goldenPath/help/ftp.html">Using rsync</A> (recommended)
<LI><A HREF="ftp://hgdownload.soe.ucsc.edu/goldenPath/$db/">Using FTP</A>
<LI><A HREF="http://hgdownload.soe.ucsc.edu/downloads.html#$anchorRoot">Using HTTP</A>
<LI><A HREF="../goldenPath/credits.html#${anchorRoot}_use">Data use conditions and
restrictions</A>
<LI><A HREF="../goldenPath/credits.html#${anchorRoot}_credits">Acknowledgments</A>
</UL>
</P>
_EOF_
  ;
  close($fh);
} # makeDescription

# from Perl Cookbook Recipe 2.17, print out large numbers with comma
# delimiters:
sub commify($) {
    my $text = reverse $_[0];
    $text =~ s/(\d\d\d)(?=\d)(?!\d*\.)/$1,/g;
    return scalar reverse $text
}

# definition of contig types in the AGP file
my %goldTypes = (
'A' => 'active finishing',
'D' => 'draft sequence',
'F' => 'finished sequence',
'O' => 'other sequence',
'P' => 'pre draft',
'W' => 'whole genome shotgun'
);
# definition of gap types in the AGP file
my %gapTypes = (
'clone' => 'gaps between clones in scaffolds',
'heterochromatin' => 'heterochromatin gaps',
'short_arm' => 'a gap inserted at the start of an acrocentric chromosome',
'telomere' => 'telomere gaps',
'repeat' => 'an unresolvable repeat',
'centromere' => 'gaps for centromeres are included when they can be reasonably localized',
'scaffold' => 'gaps between scaffolds in chromosome assemblies',
'contig' => 'gaps between contigs in scaffolds',
'other' => 'gaps added at UCSC to annotate strings of <em>N</em>s that were not marked in the AGP file',
'fragment' => 'gaps between whole genome shotgun contigs'
);


sub makeLocalTrackDbRa {
  # Make an assembly-level trackDb.ra, gap.html and gold.html.
  my @localFiles = qw( trackDb.ra gap.html gold.html );

  my $fh = &HgAutomate::mustOpen(">$topDir/html/trackDb.ra");
  print $fh <<_EOF_
# Local declaration so that local gold.html is picked up.
track gold override
html gold

# Local declaration so that local gap.html is picked up.
track gap override
html gap

_EOF_
  ;
  close($fh);

  $fh = &HgAutomate::mustOpen(">$topDir/html/gap.html");
  my $em = $commonName ? "" : "<em>";
  my $noEm = $commonName ? "" : "</em>";
  my $gapCount = `hgsql -N -e 'select count(*) from gap;' $db`;
  chomp $gapCount;
  if ($gotAgp) {
    print $fh <<_EOF_
<H2>Description</H2>
<P>
This track shows the gaps in the $assemblyDate $em\$organism$noEm genome assembly.
</P>
<P>
Genome assembly procedures are covered in the NCBI
<A HREF="https://www.ncbi.nlm.nih.gov/assembly/basics/"
TARGET=_blank>assembly documentation</A>.<BR>
NCBI also provides
<A HREF="https://www.ncbi.nlm.nih.gov/assembly/$ncbiAssemblyId"
TARGET="_blank">specific information about this assembly</A>.
</P>
<P>
The definition of the gaps in this assembly is from the
<A HREF="ftp://hgdownload.soe.ucsc.edu/goldenPath/$db/bigZips/$db.agp.gz"
TARGET=_blank>AGP file</A> delivered with the sequence.  The NCBI document
<A HREF="https://www.ncbi.nlm.nih.gov/assembly/agp/AGP_Specification/"
TARGET=_blank>AGP Specification</A> describes the format of the AGP file.
</P>
<P>
Gaps are represented as black boxes in this track.
If the relative order and orientation of the contigs on either side
of the gap is supported by read pair data,
it is a <em>bridged</em> gap and a white line is drawn
through the black box representing the gap.
</P>
_EOF_
    ;
    if ($gapCount > 0) {
      print $fh "<P>This assembly contains the following principal types of gaps:
<UL>\n";
    } else {
      print $fh "<P>This assembly has no annotated gaps.\n</P>\n";
    }
    open (GL, "hgsql -N -e 'select type from gap;' $db | sort | uniq -c | sort -n|") or die "can not select type from $db.gap table";
    while (my $line = <GL>) {
        chomp $line;
        $line =~ s/^\s+//;
        my ($count, $type) = split('\s+', $line);
        my $minSize = `hgsql -N -e 'select min(size) from gap where type="$type";' $db`;
        chomp $minSize;
        my $maxSize = `hgsql -N -e 'select max(size) from gap where type="$type";' $db`;
        chomp $maxSize;
        my $sizeMessage = "";
        if ($minSize == $maxSize) {
            $sizeMessage = sprintf ("all of size %s bases", commify($minSize));
        } else {
            $sizeMessage = sprintf ("size range: %s - %s bases",
                commify($minSize), commify($maxSize));
        }
        if (exists ($gapTypes{$type}) ) {
            printf $fh "<LI><B>%s</B> - %s (count: %s; %s)</LI>\n", $type,
                $gapTypes{$type}, commify($count), $sizeMessage;
        } else {
            die "makeLocalTrackDbRa: missing AGP gap type definition: $type";
        }
    }
    close (GL);
    if ($gapCount > 0) {
      print $fh "</UL></P>\n";
    }
  } else {
    print $fh <<_EOF_
<H2>Description</H2>
This track depicts gaps in the draft assembly ($assemblyDate, $assemblyLabel)
of the $em\$organism$noEm genome.

  *** Developer: remove this statement if no future assemblies are expected:

Many of these gaps &mdash; with the
exception of intractable heterochromatic gaps &mdash; may be closed during the
finishing process.
<P>
Gaps are represented as black boxes in this track.
If the relative order and orientation of the contigs on either side
of the gap is known, it is a <em>bridged</em> gap and a white line is drawn
through the black box representing the gap.
</P>
<P>
This assembly was sequenced with paired reads taken from

  *** Developer: check if this is accurate:

plasmids and fosmids of various sizes.

Overlapping reads were merged into contigs,
and pairing information was then used to join the contigs into scaffolds.
The gap sizes are estimated from the size of the

plasmids and fosmids,

with a minimum gap size of $fakeAgpMinContigGap.
</P>
<P></P>
<P>This assembly contains the following types of gaps:
<UL>
<LI><B>Contig</B> - gaps of size $fakeAgpMinContigGap to $fakeAgpMinScaffoldGap.
<LI><B>Scaffold</B> - gaps greater than $fakeAgpMinScaffoldGap in size.
</UL>
</P>
_EOF_
    ;
  }
  close($fh);

  $fh = &HgAutomate::mustOpen(">$topDir/html/gold.html");
  if ($gotAgp) {
    print $fh <<_EOF_
<H2>Description</H2>
<P>
This track shows the sequences used in the $assemblyDate $em\$organism$noEm genome assembly.
</P>
<P>
Genome assembly procedures are covered in the NCBI
<A HREF="https://www.ncbi.nlm.nih.gov/assembly/basics/"
TARGET=_blank>assembly documentation</A>.<BR>
NCBI also provides
<A HREF="https://www.ncbi.nlm.nih.gov/assembly/$ncbiAssemblyId"
TARGET="_blank">specific information about this assembly</A>.
</P>
<P>
The definition of this assembly is from the
<A HREF="ftp://hgdownload.soe.ucsc.edu/goldenPath/$db/bigZips/$db.agp.gz"
TARGET=_blank>AGP file</A> delivered with the sequence.  The NCBI document
<A HREF="https://www.ncbi.nlm.nih.gov/assembly/agp/AGP_Specification/"
TARGET=_blank>AGP Specification</A> describes the format of the AGP file.
</P>
<P>
In dense mode, this track depicts the contigs that make up the
currently viewed scaffold.
Contig boundaries are distinguished by the use of alternating gold and brown
coloration. Where gaps
exist between contigs, spaces are shown between the gold and brown
blocks.  The relative order and orientation of the contigs
within a scaffold is always known; therefore, a line is drawn in the graphical
display to bridge the blocks.</P>
<P>
Component types found in this track (with counts of that type in parentheses):
<UL>
_EOF_
    ;
    open (GL, "hgsql -N -e 'select type from gold;' $db | sort | uniq -c | sort -rn|") or die "can not select type from $db.gold table";
    while (my $line = <GL>) {
        chomp $line;
        $line =~ s/^\s+//;
        my ($count, $type) = split('\s+', $line);
        my $singleMessage = "";
        if ((1 == $count) && (("F" eq $type) || ("O" eq $type))) {
            my $chr = `hgsql -N -e 'select chrom from gold where type=\"$type\";' $db`;
            my $frag = `hgsql -N -e 'select frag from gold where type=\"$type\";' $db`;
            chomp $chr;
            chomp $frag;
            $singleMessage = sprintf("(%s/%s)", $chr, $frag);
        }
        if (exists ($goldTypes{$type}) ) {
            if (length($singleMessage)) {
                printf $fh "<LI>%s - one %s %s</LI>\n", $type, $goldTypes{$type}, $singleMessage;
            } else {
                printf $fh "<LI>%s - %s (%s)</LI>\n", $type, $goldTypes{$type}, commify($count);
            }
        } else {
            die "makeLocalTrackDbRa: missing AGP contig type definition: $type";
        }
    }
    close (GL);
    printf $fh "</UL></P>\n";
  } else {
    print $fh <<_EOF_
<H2>Description</H2>
<P>
This track shows the draft assembly ($assemblyDate, $assemblyLabel)
of the $em\$organism$noEm genome.

  *** Developer: check if this is accurate:

Whole-genome shotgun reads were assembled into contigs.  When possible,
contigs were grouped into scaffolds (also known as &quot;supercontigs&quot;).
The order, orientation and gap sizes between contigs within a scaffold are
based on paired-end read evidence. </P>
<P>
Locations of contigs and scaffolds were deduced from runs of Ns in the
assembled sequence.  A run of Ns of more than $fakeAgpMinScaffoldGap bases
was assumed to be a gap between scaffolds, and a run of Ns between
$fakeAgpMinContigGap and $fakeAgpMinScaffoldGap was assumed to be a gap
between contigs.</P>
<P>
In dense mode, this track depicts the contigs that make up the
currently viewed scaffold.
Contig boundaries are distinguished by the use of alternating gold and brown
coloration. Where gaps
exist between contigs, spaces are shown between the gold and brown
blocks.  The relative order and orientation of the contigs
within a scaffold is always known; therefore, a line is drawn in the graphical
display to bridge the blocks.</P>
<P>
All components within this track are of fragment type &quot;W&quot;:
whole genome shotgun contig. </P>
_EOF_
    ;
  }
  close($fh);

  my $localFiles = "$topDir/html/{" . join(',', @localFiles) . '}';
  return $localFiles;
} # makeLocalTrackDbRa

sub makeTrackDb {
# * step: trackDb [dbHost]
  my $runDir = "$topDir/TemporaryTrackDbCheckout";
  &HgAutomate::mustMkdir($runDir);
  &HgAutomate::mustMkdir("$topDir/html");
  &makeDescription();
  my $localFiles = &makeLocalTrackDbRa();
  my $whatItDoes =<<_EOF_
It makes a local checkout of kent/src/hg/makeDb/trackDb/
and makes *suggested* modifications.  Checking in modified files, and adding
new files, is left up to the user in case this script has made some wrong
guesses about proper names and locations.
_EOF_
  ;
  my $bossScript = new HgRemoteScript("$scriptDir/makeTrackDb.csh", $dbHost,
				      $runDir, $whatItDoes, $CONFIG);

  $bossScript->add(<<_EOF_
# These directories are necessary for running make in trackDb:
$HgAutomate::git archive --remote=git://genome-source.soe.ucsc.edu/kent.git \\
  --prefix=kent/ HEAD src/hg/makeDb/trackDb/loadTracks \\
src/hg/makeDb/trackDb/$dbDbSpeciesDir \\
src/hg/makeDb/trackDb/trackDb.transMap.ra \\
src/hg/makeDb/trackDb/trackDb.chainNet.ra \\
src/hg/makeDb/trackDb/crisprAll.ra \\
src/hg/makeDb/trackDb/trackDb.chainNet.primates.ra \\
src/hg/makeDb/trackDb/trackDb.chainNet.other.ra \\
src/hg/makeDb/trackDb/trackDb.chainNet.euarchontoglires.ra \\
src/hg/makeDb/trackDb/trackDb.chainNet.laurasiatheria.ra \\
src/hg/makeDb/trackDb/trackDb.chainNet.afrotheria.ra \\
src/hg/makeDb/trackDb/trackDb.chainNet.mammal.ra \\
src/hg/makeDb/trackDb/trackDb.chainNet.birds.ra \\
src/hg/makeDb/trackDb/trackDb.chainNet.sarcopterygii.ra \\
src/hg/makeDb/trackDb/trackDb.chainNet.fish.ra \\
src/hg/makeDb/trackDb/trackDb.chainNet.insects.ra \\
src/hg/makeDb/trackDb/trackDb.chainNet.nematode.ra \\
src/hg/makeDb/trackDb/chainNetPetMar1.ra \\
src/hg/makeDb/trackDb/chainNetPetMar2.ra \\
src/hg/makeDb/trackDb/trackDb.nt.ra \\
src/hg/makeDb/trackDb/joinedRmskComposite.ra \\
src/hg/makeDb/trackDb/joinedRmsk.ra \\
src/hg/makeDb/trackDb/trackDb.genbank.ra \\
src/hg/makeDb/trackDb/trackDb.genbank.new.ra \\
src/hg/makeDb/trackDb/trackDb.uniprot.ra \\
src/hg/makeDb/trackDb/crispr10K.ra \\
src/hg/makeDb/trackDb/crisprAll.ra \\
src/hg/makeDb/trackDb/tagTypes.tab \\
src/hg/lib/trackDb.sql \\
src/hg/lib/hgFindSpec.sql \\
src/hg/makeDb/trackDb/trackDb.ra | tar xf -

cd kent/src/hg/makeDb/trackDb

# Create the expected species-level directory for $db if necessary:
mkdir -p $dbDbSpeciesDir/$db

# Move local description files into place:
mv $localFiles $dbDbSpeciesDir/$db/

if (1 == $forceDescription) then
  rm -f $dbDbSpeciesDir/$db/description.html
endif
# Copy the template description.html for $db into place, and link to it
# from $HgAutomate::gbdb/$db/html/ .
if (! -e $dbDbSpeciesDir/$db/description.html) then
  cp -p $topDir/html/description.html $dbDbSpeciesDir/$db/description.html
endif
mkdir -p $HgAutomate::gbdb/$db/html
rm -f $HgAutomate::gbdb/$db/html/description.html
ln -s $topDir/html/description.html $HgAutomate::gbdb/$db/html/

# Do a test run with the generated files:
./loadTracks trackDb_\${USER} hgFindSpec_\${USER} $db
_EOF_
  );

  $bossScript->execute();

  $endNotes .=<<_EOF_

Template trackDb.ra and .html's have been created, but they all need editing!

cd $runDir/kent/src/hg/makeDb/trackDb/$dbDbSpeciesDir/$db

Search for '***' notes in each file in and make corrections (sometimes the
files used for a previous assembly might make a better template):
  description.html $localFiles

Then copy these files to your ~/kent/src/hg/makeDb/trackDb/$dbDbSpeciesDir/$db
 - cd ~/kent/src/hg/makeDb/trackDb
 - edit makefile to add $db to DBS.
 - git add $dbDbSpeciesDir/$db/*.{ra,html}
 - git commit -m "Added $db to DBS." makefile
 - git commit -m "Initial descriptions for $db." $dbDbSpeciesDir/$db/*.{ra,html}
 - git pull; git push
 - Run make update DBS=$db and make alpha when done.
 - (optional) Clean up $runDir

_EOF_
  ;
} # makeTrackDb


#########################################################################
# main

# Prevent "Suspended (tty input)" hanging:
&HgAutomate::closeStdin();

&checkOptions();
&usage(1) if (scalar(@ARGV) != 1);
($CONFIG) = @ARGV;

#*** Force -verbose until this is really ready to go:
$opt_verbose = 3 if (! $opt_verbose);

&parseConfig($CONFIG);

$endNotes = "";
$bedDir = "$topDir/$HgAutomate::trackBuild";
$scriptDir = "$topDir/jkStuff";
if (-e "$topDir/chrom.sizes") {
  $chromBased = (`wc -l < $topDir/chrom.sizes` < $HgAutomate::splitThreshold);
}

$stepper->execute();

my $stopStep = $stepper->getStopStep();
my $upThrough = ($stopStep eq 'trackDb') ? "" :
  "  (through the '$stopStep' step)";

HgAutomate::verbose(1,
	"\n *** All done!$upThrough\n");

if ($endNotes) {
  #*** Should mail this to $ENV{'USER'} so it's not so easy to ignore.
  #*** Does mail work on all of our machines??  Even if it works on one,
  #*** we can ssh it.  Should be in an HgAutomate routine.
  HgAutomate::verbose(0,
		      "\n\nNOTES -- STUFF THAT YOU WILL HAVE TO DO --\n\n" .
		      "$endNotes\n");
}

