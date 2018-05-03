#!/usr/bin/env perl

# DO NOT EDIT the /cluster/bin/scripts copy of this file --
# edit ~/kent/src/hg/utils/automation/doDbSnp.pl instead.

use Getopt::Long;
use LWP::UserAgent;
use warnings;
use strict;
use FindBin qw($Bin);
use lib "$Bin";
use HgAutomate;
use HgRemoteScript;
use HgStepManager;


my $stepper = new HgStepManager(
    [ { name => 'download',   func => \&download },
      { name => 'loadDbSnp',  func => \&loadDbSnp },
      { name => 'addToDbSnp', func => \&addToDbSnp },
      { name => 'bigJoin',    func => \&bigJoin },
      { name => 'catFasta',   func => \&catFasta },
      { name => 'translate',  func => \&translate },
      { name => 'load',       func => \&loadTables },
      { name => 'filter',     func => \&filterTables },
      { name => 'coding',     func => \&codingDbSnp },
      { name => 'bigBed',     func => \&bigBed },
    ]
			       );

my $base = $0;
$base =~ s/^(.*\/)?//;

# Hardcoded commands / paths:
my $wget = 'wget --timestamping --no-verbose';
my $ftpShared = 'ftp://ftp.ncbi.nih.gov/snp/database/shared_data';
my $ftpSnpDb = 'ftp://ftp.ncbi.nih.gov/snp/organisms/$orgDir/database/organism_data';
my $ftpOrgSchema = 'ftp://ftp.ncbi.nlm.nih.gov/snp/organisms/$orgDir/database/organism_schema';
my $ftpSharedSchema = 'ftp://ftp.ncbi.nlm.nih.gov/snp/database/shared_schema';
my $dbSnpRoot = '/hive/data/outside/dbSNP';
# Some ContigInfo columns (1-based) -- if there are any changes, garbage results
# should trigger errors.
my $ctgIdCol = 1;
my $contigAccCol = 3;
my $groupLabelCol = 12;
# This is for an hg19-specific mess: we unintentionally used a mitochondrion
# that is not the revised Cambridge Reference Sequence (rCRS) that is the
# standard.  dbSNP produces rCRS coords, which must be translated to our
# chrM.  Since there are some indels, liftOver is required.  See snp131
# section of makeDb/doc/hg19.txt for how this chain file was generated.
my $hg19MitoLiftOver = '/hive/data/outside/dbSNP/131/human/NC_012920ToChrM.over.chain';

# Option defaults:
my $dbHost = 'hgwdev';
my $workhorse = 'hgwdev';
my $fileServer = 'hgwdev';

sub usage {
  # Usage / help / self-documentation:
  my ($status, $detailed) = @_;
  # Basic help (for incorrect usage):
  print STDERR "
usage: $base config.ra
options:
    -buildDir dir         Use dir instead of default $dbSnpRoot/<build>/
";
  print STDERR $stepper->getOptionHelp();
  print STDERR &HgAutomate::getCommonOptionHelp('dbHost' => $dbHost,
						'workhorse' => '$workhorse',
					        'fileServer' => '$fileServer');
  print STDERR "
Automates our processing of dbSNP files into our snpNNN track:
    download:   FTP dbSNP database dump files and flanking sequence fasta
                from NCBI.
    loadDbSnp:  Create a local mysql database and load the dbSNP files into
                it, possibly excluding SNPs mapped to alternate assemblies
                or contigs that we don't know how to lift to the reference.
    addToDbSnp: Add information from fasta headers, and concatenated function
                codes, to the local dbSNP mysql database.
    bigJoin:    Run a large left-join query to extract the columns from
                various dbSNP tables into our snpNNN columns and lift from
                contigs up to chroms if necessary.
    catFasta:   Concatenate all flanking sequence files into one giant file.
    translate:  Run snpNcbiToUcsc on the join output to perform final checks
                and conversion of numeric codes into strings.
    load:       Load snpNNN* tables into our database.
    filter:     Extract Common, Mult and Flagged sets (may be empty).
    coding:     Process dbSNP's functional annotations into snpNNNCodingDbSnp.
    bigBed:     Create a bed4 bigBed for use by hgVai.
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

orgDir XXXXX_NNNNN
  - Subdirectory of ftp://ftp.ncbi.nlm.nih.gov/snp/database/organism_data/
    e.g. human_9606 or fruitfly_7227 (commonName_taxonomyId)

build NNN
  - The dbSNP build identifier (e.g. 132 for species updated in November 2010).

buildAssembly NN_N
  - The assembly identifier used in dbSNP's table/dumpfile names (e.g. 37_1
    for human 132).  From the directory listing of
    ftp://ftp.ncbi.nih.gov/snp/database/organism_data/ , click into orgDir
    and look for file names like b(1[0-9][0-9])_*_([0-9]_[0-9]).bcp.gz .
    The first number is build; the second number_number is buildAssembly.
    In dbSNP build 137 and later, they stopped including the buildAssembly
    in filenames so you should include this keyword but leave it blank.

-----------------------------------------------------------------------------
Conditionally required config.ra settings:

1. Required when there is more than one assembly label in dbSNP's ContigInfo
   table, ${groupLabelCol}th column (group_label).  If there is more than one, this script
   will halt at the end of the download step and display a list of labels.
   The developer needs to figure out which label(s) are the ones we need.
   For most non-human species, there is only one so this isn't needed.
   However, for human, as of build 132, there are four (CRA_TCAGchr7v2,
   Celera, GRCh37, HuRef) and the only one we want is GRCh37.  There can
   be multiple labels, e.g. when we include haplo-chroms that don't share
   the same label as the main assembly (see makeDb/doc/hg18.txt's snp130
   process).

refAssemblyLabel XXXXX
  - The assembly label that appears in the ContigInfo table dump,
    ${groupLabelCol}th column (group_label) and corresponds to the sequences included
    in UCSC's database.  If there is more than one label, this can be a
    comma-separated list.
    *** This script assumes that these labels are not numeric and distinct
    *** enough to never match inappropriate columns' values.
    *** If this is not the case you'll need to edit this script.


2. Required when we need to liftUp NCBI's contigs to chroms:
   NOTE: This script may be able to construct these files for you, see
   ncbiAssemblyReportFile below.

liftUp FFFFF
  - Absolute path to the liftUp file that maps NCBI's RefSeq assembly contig
    IDs to UCSC chroms.

3. Required when ContigInfo (after filtering by refAssemblyLabel if specified)
   contains sequences that are not in UCSC's assembly (liftUp if specified,
   or chrom.sizes.  These are typically alternate assembly sequences included
   in the assembly release that we chose not to include in our db, or patch
   sequences released after we created our db.
   Use either ignoreDbSnpContigsFile or ignoreDbSnpContigs:

ignoreDbSnpContigsFile FILENAME
  - File with one word per line, listing dbSNP contigs that cannot be liftUp'd
    to UCSC sequences.  Before using all contig IDs in script-generated
    file cantLiftUpSeqNames.txt, do some Entrez searches to verify that those
    contig IDs are generally for alt assemblies or patches.

ignoreDbSnpContigs REGEX
  - Deprecated -- this is a holdover from early days when alt assembly contigs
    had distinct ID ranges.  Nowadays the IDs are interspersed.
  - Regular expression of dbSNP contigs that cannot be liftUp'd to UCSC
    sequences, e.g. if we got one version of randoms from Baylor but NCBI
    got another.  The regular expression is interpreted by egrep -vw.
    If ContigInfo's ${groupLabelCol}th (group_label) column includes a label
    that covers all such contigs *and* does not cover any contigs that we need
    to keep, it is better to omit that label from refAssemblyLabel (above).
    *** This script assumes that these contig accessions are not numeric and
    *** distinct enough to never match other columns' values.
    *** If this is not the case, use ignoreDbSnpContigsFile

3. Required when UCSC doesn't have the mapping of RefSeq assembly contigs to
   GenBank assembly contigs:

ncbiAssemblyReportFile GCF_000*.assembly.txt
  - The NCBI Assembly file that contains mappings of GenBank assembly contigs
    (that we use) to RefSeq assembly contigs (that dbSNP uses).  Usually the
    sequences are identical, so the mapping is very straightforward.  However,
    if the RefSeq folks alter a chromosome or contig, the mapping will have to
    be deduced by you from NCBI Nucleotide sequence descriptions.
    HOW TO find this file for your assembly:
    1. Search in Entrez Assembly (https://www.ncbi.nlm.nih.gov/assembly/)
       for the refAssemblyLabel value (see above)
    2. Select the most recent matching assembly
    3. Find the link labeled 'Download the full sequence report' and copy
       its link address (URL)
    4. Download that file and use its local path.

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
use vars qw/
    $opt_buildDir
    /;
# Required config parameters:
my ($db, $build, $buildAssembly, $orgDir, $orgDirTrimmed);
# Conditionally required config parameters:
my ($refAssemblyLabel, $liftUp, $ignoreDbSnpContigsFile, $ignoreDbSnpContigs,
    $ncbiAssemblyReportFile);
# Optional config param:
my ($snpBase);
# Other globals:
my ($buildDir, $commonName, $assemblyDir, $assemblyLabelFile, $endNotes, $needSNPAlleleFreq_TGP);
# These dbSNP table/file names vary by build and assembly but
# table desc is stable:
my ($ContigInfo, $ContigLoc, $ContigLocusId, $MapInfo);

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
  $dbHost = $opt_dbHost if (defined $opt_dbHost);
  $fileServer = $opt_fileServer if ($opt_fileServer);
  $workhorse = $opt_workhorse if ($opt_workhorse);
} # checkOptions

#*** FIXME --- begin code lifted from makeGenomeDb.pl -- need to libify

sub parseConfig {
  # Parse config.ra file and return hashRef of var->val.
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
  return \%config;
} # parseConfig

sub requireVar {
  # Ensure that var is in %config and return its value.
  # Remove it from %config so we can check for unrecognized contents.
  my ($var, $config) = @_;
  my $val = $config->{$var};
  if (! defined $val) {
    die "Error: $CONFIG is missing required variable \"$var\".\n" .
        "For a detailed list of required variables, run \"$base -help\".\n";
  }
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

sub checkConfigEmpty {
  # If config hashRef has any vars not removed by requireVar() or
  # optionalVar(), die with error -- config might have misspelled var.
  my ($config, $base, $configFile) = (@_);
  my @stragglers = sort keys %{$config};
  if (scalar(@stragglers) > 0) {
    die "Error: config file $configFile has unrecognized variables:\n" .
      "    " . join(", ", @stragglers) . "\n" .
      "For a detailed list of supported variables, run \"$base -help\".\n";
  }
} # checkConfigEmpty

#*** FIXME --- end code lifted from makeGenomeDb.pl -- need to libify

sub checkConfig {
  # Make sure config hashRef contains the required variables.
  my ($config) = (@_);
  # Required variables.
  $db = &requireVar('db', $config);
  $build = &requireVar('build', $config);
  $buildAssembly = &requireVar('buildAssembly', $config);
  $orgDir = &requireVar('orgDir', $config);
  # Conditionally required variables -- optional here, but they might be
  # required later on in some cases.
  $refAssemblyLabel = &optionalVar('refAssemblyLabel', $config);
  $liftUp = &optionalVar('liftUp', $config);
  $ignoreDbSnpContigsFile = &optionalVar('ignoreDbSnpContigsFile', $config);
  $ignoreDbSnpContigs = &optionalVar('ignoreDbSnpContigs', $config);
  $ncbiAssemblyReportFile = &optionalVar('ncbiAssemblyReportFile', $config);
  if ($ignoreDbSnpContigs && $ignoreDbSnpContigsFile) {
    die "Error: Only one of {ignoreDbSnpContigs, ignoreDbSnpContigsFile} can be " .
        "passed in. Put all contig names to be ignored in ignoreDbSnpsContigsFile.";
  }
  # Optional var:
  $snpBase = &optionalVar('snpBase', $config);
  # Make sure no unrecognized variables were given.
  &checkConfigEmpty($config, $CONFIG, $base);
} # checkConfig


#########################################################################
# * step: download [fileServer]

sub download {
  # Fetch database dump files via anonymous FTP from NCBI
  # and translate dbSNP SQL to mySQL.
#*** It would be nice to kick off the ftp of rs_fasta files in parallel because
#*** we won't need them until after a significant amount of processing on db dumpfiles.
#*** I don't want to parallelize too much & hog NCBI FTP.
  my $runDir = $buildDir;
  &HgAutomate::mustMkdir("$buildDir/shared");
  &HgAutomate::mustMkdir("$assemblyDir");
  &HgAutomate::mustMkdir("$assemblyDir/data");
  &HgAutomate::mustMkdir("$assemblyDir/schema");
  &HgAutomate::mustMkdir("$assemblyDir/rs_fasta");
  my $getSNPAlleleFreqTGPToo =
    $needSNPAlleleFreq_TGP ? "$wget $ftpSnpDb/SNPAlleleFreq_TGP.bcp.gz" : "";
  my $whatItDoes =
    "It downloads a bunch of dbSNP database table dump files.";
  my $bossScript = new HgRemoteScript("$runDir/download_${commonName}_${db}_${build}.csh",
				      $fileServer, $runDir, $whatItDoes, $CONFIG);
  $bossScript->add(<<_EOF_
# Get field encodings -- if there are changes or additions to the
# encoding of the corresponding fields, you might need to update
# snpNcbiToUcsc, hgTracks, hgc and hgTrackUi (see also
# hg/lib/snp125Ui.c).
cd $buildDir/shared
$wget $ftpShared/LocTypeCode.bcp.gz
$wget $ftpShared/SnpClassCode.bcp.gz
$wget $ftpShared/SnpFunctionCode.bcp.gz
$wget $ftpShared/SnpValidationCode.bcp.gz
# Also fetch Allele, so we can translate allele IDs in SNPAlleleFreq
# into base values.
$wget $ftpShared/Allele.bcp.gz

# Get $commonName-specific data:
cd $assemblyDir
$wget ftp://ftp.ncbi.nih.gov/snp/00readme.txt
cd $assemblyDir/data
set orgDir = $orgDir
# $ContigLoc table has coords, orientation, loc_type, and refNCBI allele
$wget $ftpSnpDb/$ContigLoc.bcp.gz
# $ContigLocusId table has functional annotations
$wget $ftpSnpDb/$ContigLocusId.bcp.gz
$wget $ftpSnpDb/$ContigInfo.bcp.gz
# $MapInfo has alignment weights
$wget $ftpSnpDb/$MapInfo.bcp.gz
# SNP has univar_id, validation status and heterozygosity
$wget $ftpSnpDb/SNP.bcp.gz
# New info as of 132: allele freq, 'clinical' bit, SNP submitter handles
$wget $ftpSnpDb/SNPAlleleFreq.bcp.gz
$getSNPAlleleFreqTGPToo
$wget $ftpSnpDb/SNP_bitfield.bcp.gz
$wget $ftpSnpDb/Batch.bcp.gz
$wget $ftpSnpDb/SubSNP.bcp.gz
$wget $ftpSnpDb/SNPSubSNPLink.bcp.gz

# Get schema
cd $assemblyDir/schema
$wget $ftpOrgSchema/${orgDirTrimmed}_table.sql.gz
$wget $ftpSharedSchema/dbSNP_main_table.sql.gz

# Get fasta files
# using headers of fasta files for molType, class, observed
cd $assemblyDir/rs_fasta
$wget ftp://ftp.ncbi.nih.gov/snp/organisms/$orgDir/rs_fasta/\\*.gz

# Make all files group writeable so others can update them if necessary
find $buildDir -user \$USER -not -perm -660 | xargs --no-run-if-empty chmod ug+w

# Extract the set of assembly labels in case we need to exclude any.
zcat $assemblyDir/data/$ContigInfo.bcp.gz | cut -f $groupLabelCol | uniq | sort -u \\
  > $assemblyLabelFile
_EOF_
    );

  $bossScript->execute();
} # download


#########################################################################
# * step: loadDbSnp [workhorse]

sub translateSql {
  return if ($opt_debug);

#*** Note: we really need to compare these SQL definitions vs. our
#*** expectation of columns and column indices since we use numeric
#*** column offsets...

  # Translate dbSNP's flavor of SQL create statements into mySQL.
  # This is computationally trivial so it doesn't matter where it runs.
  my $schemaDir = "$assemblyDir/schema";
  # First the organism-specific tables from $ftpSnpDb:
  my @orgTables = ($ContigInfo, $ContigLoc, $ContigLocusId, $MapInfo);
  push @orgTables, qw( SNP SNPAlleleFreq SNP_bitfield Batch SubSNP SNPSubSNPLink );
  push @orgTables, 'SNPAlleleFreq_TGP' if ($needSNPAlleleFreq_TGP);
  my $tables = join('|', @orgTables);
  my $SQLIN = HgAutomate::mustOpen("zcat $schemaDir/${orgDirTrimmed}_table.sql.gz |" .
				   "sed -re 's/\r//g;' |");
  my $SQLOUT = HgAutomate::mustOpen("> $schemaDir/table.sql");
  my $sepBak = $/;
  $/ = "\ngo\n\n";
  my $tableCount = 0;
  while (<$SQLIN>) {
    next unless /^\n*CREATE TABLE \[($tables)\]/;
    s/[\[\]]//g;  s/\ngo\n/;/i;  s/smalldatetime/datetime/g;
    s/ON PRIMARY//g;  s/COLLATE//g;  s/Latin1_General_BIN//g;
    s/nvarchar/varchar/g;  s/set quoted/--set quoted/g;
    s/(image|varchar\s+\(\d+\))/BLOB/g;  s/tinyint/tinyint unsigned/g;
    s/ bit / tinyint unsigned /g;
    print $SQLOUT $_;
    $tableCount++;
  }
  close($SQLIN);
  # And shared table from $ftpShared, described in a separate sql file:
  my @sharedTables = qw( Allele );
  $tables = join('|', @sharedTables);
  $SQLIN = HgAutomate::mustOpen("zcat $schemaDir/dbSNP_main_table.sql.gz |" .
				   "sed -re 's/\r//g;' |");
  $/ = "\ngo\n\n";
  while (<$SQLIN>) {
    next unless /^CREATE TABLE \[$tables\]/;
    s/[\[\]]//g;  s/\nGO\n/;\n/i;  s/smalldatetime/datetime/g; s/IDENTITY\(1,1\) //g;
    print $SQLOUT $_;
    $tableCount++;
  }
  close($SQLIN);
  close($SQLOUT);
  $/ = $sepBak;
  my $expected = (@orgTables + @sharedTables);
  if ($tableCount != $expected) {
    die "Expected to process $expected CREATE statements but got $tableCount\n\t";
  }
} # translateSql


sub getDbSnpAssemblyLabels {
  # Return a list of assembly labels extracted from $ContigInfo.
  return ('ref') if ($opt_debug);
  my @labels;
  my $LAB = HgAutomate::mustOpen("$assemblyLabelFile");
  while (<$LAB>) {
    chomp;
    push @labels, $_;
  }
  close($LAB);
  if (@labels == 0) {
    die "$assemblyLabelFile is empty -- has $ContigInfo format changed?";
  }
  return @labels;
} # getDbSnpAssemblyLabels


sub demandAssemblyLabel {
  # Show developer the valid assembly labels, at least one of which must
  # appear in $CONFIG's refAssemblyLabel value.
  my @labels = @_;
  my $message =  <<_EOF_

 *** This release contains more than one assembly label.
 *** Please examine this list in case we need to exclude any of these:

_EOF_
    ;
  $message .= join("\n", @labels);
  my $refAssemblyLabelDef = "refAssemblyLabel " . join(',', @labels);
  $message .= <<_EOF_

 *** Add refAssemblyLabel to $CONFIG.  If keeping all labels, it will
 *** look like this:

$refAssemblyLabelDef

 *** Edit out any of those that are not included in $db (e.g. Celera).
 *** Then restart this script with -continue=loadDbSnp .

_EOF_
    ;
  die $message;
} # demandAssemblyLabel


sub checkAssemblySpec {
  # If $assemblyLabelFile contains more than one label, make sure that
  # refAssemblyLabel is in $CONFIG and includes only labels found in
  # $assemblyLabelFile.  Return labels that are omitted from the spec.
  my @labels = &getDbSnpAssemblyLabels();
  if (@labels > 1) {
    if ($refAssemblyLabel) {
      my %rejectLabels = map {$_ => 1} @labels;
      my %labelHash = map {$_ => 1} @labels;
      my @specLabels = split(',', $refAssemblyLabel);
      foreach my $l (@specLabels) {
	if (! exists $labelHash{$l}) {
	  die "\nConfig Error: refAssemblyLabel value '$l' is not recognized -- " .
	    "must be one of {" . join(", ", @labels) . "}\n" ;
	}
	delete $rejectLabels{$l};
      }
      return keys %rejectLabels;
    }
    &demandAssemblyLabel(@labels);
  } elsif (! $refAssemblyLabel) {
    $refAssemblyLabel = $labels[0];
  }
  return ();
} # checkAssemblySpec


sub tryToMakeLiftUpFromContigInfo {
  # Create $liftUpFile and add an entry whenever a line of $ContigInfo (possibly grepping
  # out lines we know we want to ignore) has sufficient info.  Make a list of lists to
  # describe lines of ContigInfo for which there was not enough info.
  # Return the count of entries written to $liftUpFile and the missing info list.
  my ($liftUpFile, $runDir, $chromSizes, $grepOutLabels, $grepOutContigs) = @_;
  my $ciPipe = "zcat $runDir/data/$ContigInfo.bcp.gz $grepOutLabels $grepOutContigs |";
  my $CI = &HgAutomate::mustOpen($ciPipe);
  my $LU = &HgAutomate::mustOpen(">$liftUpFile");
  my @missingInfo = ();
  my ($missingCount, $liftUpCount) = (0, 0);
  while (<$CI>) {
    chomp;  my @w = split("\t");
    my ($contig, $chr, $chromStart, $chromEnd, $orient, $groupTerm) = ($w[2], $w[5], $w[6], $w[7], $w[8], $w[10]);
    my $strand = $orient eq "1" ? "-" : "+";
    if ($chromStart ne "") {
      $chr = "chrM" if ($chr eq "MT");
      if (! exists $chromSizes->{$chr}) {
	if (exists $chromSizes->{"chr$chr"}) {
	  $chr = "chr$chr";
	} else {
	  $chr = "no chr" if (! $chr);
	  push @missingInfo, [ 'unrecognized newName', $contig, $chr ];
	  $missingCount++;
	  next;
	}
      }
      my $oldSize = $chromEnd+1 - $chromStart;
      my $newSize = $chromSizes->{$chr};
      print $LU join("\t", $chromStart, $contig, $oldSize, $chr, $newSize, $strand) . "\n";
      $liftUpCount++;
    } elsif ($groupTerm eq 'PATCHES') {
      $chr = "no chr" if (! $chr);
      push @missingInfo, [ 'patch contig', $contig, $chr ];
      $missingCount++;
    } elsif ($db eq 'hg38') {
      # If no chromStart given, transform chr and GB acc info into our local sequence names
      my ($acc, $accVersion, $size) = ($w[15], $w[16], $w[27]);
      my $ucscSeqName;
      if ($chr ne "") {
        my $suffix = ($groupTerm =~ /^ALT_REF/) ? '_alt' : '_random';
        $ucscSeqName = "chr$chr" . "_" . $acc . "v" . $accVersion . $suffix;
      } else {
        $ucscSeqName = "chrUn_" . $acc . "v" . $accVersion;
      }
      print $LU join("\t", 0, $contig, $size, $ucscSeqName, $size, $strand) . "\n";
      $liftUpCount++;
    } else {
      $chr = "no chr" if (! $chr);
      push @missingInfo, [ 'no coords', $contig, $chr ];
      $missingCount++;
    }
  }
  close($CI);
  close($LU);
  return $liftUpCount, \@missingInfo;
} # tryToMakeLiftUpFromContigInfo

sub eUtilQuery($$$) {
  # Send an eUtil query string to NCBI, try to match regex, and return regex's $1 if successful.
  # Otherwise warn and return undef.
  my ($ua, $query, $regex) = @_;
  my $req = HTTP::Request->new(GET => $query);
  my $res = $ua->request($req);
  my $match;
  if ($res->is_success()) {
    my $resStr = $res->as_string();
    if ($resStr =~ /$regex/) {
      $match = $1;
    } else {
      warn "Can't find match for regex '$regex' in results of NCBI EUtil query '$query'";
    }
  } else {
    warn "NCBI EUtil query '$query' failed";
  }
  return $match;
} # eUtilQuery

sub getNcbiAssemblyReportFile() {
  # Use a couple NCBI EUtils queries to get the GCF* accession for the assembly,
  # then ftp the assembly report to the local directory.
  # (see http://redmine.soe.ucsc.edu/issues/12490#note-3 )
  # Return the local filename if successful, otherwise undef.
  return undef unless ($refAssemblyLabel);
  my @assemblyLabels = split(',', $refAssemblyLabel);
  # If there are more than one, just fetch the first... if this doesn't work,
  # then make both this and tryToMakeLiftUpFromNcbiAssemblyReportFile() work
  # with multiple assembly report files.
  my $assemblyLabel = $assemblyLabels[0];
  my $ua = LWP::UserAgent->new;
  my $eUtilBase = 'https://eutils.ncbi.nlm.nih.gov/entrez/eutils';
  my $assemblyIdQuery = "$eUtilBase/esearch.fcgi?db=assembly&term=$assemblyLabel";
  my $assemblyId = eUtilQuery($ua, $assemblyIdQuery, '<Id>(\d+)<');
  if (! defined $assemblyId) {
    return undef;
  }
  my $summaryQuery = "$eUtilBase/esummary.fcgi?db=assembly&id=$assemblyId";
  my $gcfAcc = eUtilQuery($ua, $summaryQuery, '<AssemblyAccession>([\w.]+)<');
  my $assemblyReportFile = "${gcfAcc}_${refAssemblyLabel}_assembly_report.txt";
  my $assemblyReportDir = 'ftp://ftp.ncbi.nlm.nih.gov/genomes/all/' . substr($gcfAcc, 0, 3) . '/' .
    substr($gcfAcc, 4, 3) . '/' . substr($gcfAcc, 7, 3) . '/' . substr($gcfAcc, 10, 3) .
      "/${gcfAcc}_$refAssemblyLabel/";
  my $assemblyReportUrl = $assemblyReportDir . $assemblyReportFile;
  my $ftpCmd = "$wget $assemblyReportUrl";
  HgAutomate::verbose(1, "# $ftpCmd\n");
  if (system($ftpCmd) == 0) {
    return $assemblyReportFile;
  } else {
    warn "Command failed:\n$ftpCmd\n";
  }
  return undef;
} # getNcbiAssemblyReportFile

sub tryToMakeLiftUpFromNcbiAssemblyReportFile {
  # For missing contigs described in list-of-lists $missingInfo, try to find a mapping in
  # $ncbiAssemblyReports from RefSeq assembly contig to GenBank assembly contig, and check
  # $chromSizes to see if the GenBank assembly contig seems to be included.  For each
  # mapping that we find, append it to $liftUpFile.
  # Return count of new $liftUpFile entries and updated missing info list-of-lists.
  my ($liftUpFile, $chromSizes, $missingInfoIn) = @_;
  # Note we are appending to $liftUpFile, not overwriting:
  my $LU = &HgAutomate::mustOpen(">>$liftUpFile");
  my $liftUpCount = 0;
  # Make a hash of missingInfoIn's contigs to entries:
  my %missingContigs = ();
  foreach my $i (@{$missingInfoIn}) {
    my ($reason, $contig, $chr) = @{$i};
    $missingContigs{$contig} = $i;
  }
  my @missingInfo = ();
  my $NARF = &HgAutomate::mustOpen("$ncbiAssemblyReportFile");
  while (<$NARF>) {
    next if (/^#/);
    my (undef, $seqRole, $chr, undef, $gbAcc, $hopefullyEqual, $refSeqAcc) = split("\t");
    (my $rsaTrimmed = $refSeqAcc) =~ s/\.\d+$//;
    (my $gbaTrimmed = $gbAcc) =~ s/\.\d+$//;
    # contig names for more recent assemblies keep the version number, replacing the '.' with 'v':
    (my $altTrimmed = $gbAcc) =~ s/\./v/;
    if (exists $missingContigs{$rsaTrimmed}) {
      my $ucscName;
      if ($chr eq "na") {
        if ($db eq 'susScr3') {
          $ucscName = $gbAcc;
          $ucscName =~ s/\./-/;
        } else {
          $ucscName = "chrUn_$gbaTrimmed";
        }
      } else {
        $chr = "M" if ($chr eq "MT");
        $chr = "chr$chr" unless ($chr =~ /^chr/);
        my $suffix = 'random';
        if ($db eq 'hg38' && $seqRole eq 'alt-scaffold') {
          $suffix = 'alt';
        }
        $ucscName = join('_', $chr, $gbaTrimmed, $suffix);
      }
      # If a ucsc name without the version number isn't found, try it with.
      # (e.g., chrUn_GJ057137 vs. chrUn_GJ057137v1)
      my @namesToTry = ($ucscName);
      (my $altName = $ucscName) =~ s/$gbaTrimmed/$altTrimmed/;
      push @namesToTry, $altName;
      if ($altName =~ m/_random/) {
        $altName =~ s/_random/_alt/;
        push @namesToTry, $altName;
      }
      (my $fixName = $altName) =~ s/_alt/_fix/;
      push @namesToTry, $fixName;
      foreach my $name (@namesToTry) {
        if (exists $chromSizes->{$name}) {
          $ucscName = $name;
          last;
        }
      }
      if (exists $chromSizes->{$ucscName}) {
	if ($hopefullyEqual ne '=') {
	  # Aw crud.  The RefSeq folks saw fit to tweak the GenBank assembly sequence.
	  # Good luck, developer.
	  warn("
*** REFSEQ ASSEMBLY contig $refSeqAcc is not the same as GenBank assembly
*** contig $gbAcc.  Look at the Entrez Assembly description of the assembly
*** to see if it's possible to manually construct a liftUp entry.
");
	push @missingInfo, [ "REFSEQ TWEAKED ASSEMBLY", $rsaTrimmed, $chr ];
	} else {
	  # Yay!  We found a mapping.  Let this be the common case.
	  my $size = $chromSizes->{$ucscName};
          my $strand = "+";
	  print $LU join("\t", 0, $rsaTrimmed, $size, $ucscName, $size, $strand) . "\n";
	  $liftUpCount++;
	}
      } else {
	push @missingInfo, [ "$seqRole (no " . join("/", @namesToTry)  . " in chrom.sizes)",
                             $rsaTrimmed, $chr ];
      }
    } # else this contig is not in our missing list; ignore it.
  }
  close($LU);
  close($NARF);
  return $liftUpCount, \@missingInfo;
} # tryToMakeLiftUpFromNcbiAssemblyReportFile

sub tryToMakeLiftUp {
  # If it looks like we need a liftUp file, see if ContigInfo has enough info
  # to make at least a partial one.  Also, if config.ra includes ncbiAssemblyReportFile,
  # then look in there for anything we can't find in ContigInfo.
  my ($runDir, $chromSizesFile, $grepOutLabels, $grepOutContigs) = @_;
  my $CS = &HgAutomate::mustOpen($chromSizesFile);
  my %chromSizes;
  while (<$CS>) {
    chomp;  my ($chr, $size) = split("\t");
    $chromSizes{$chr} = $size;
  }
  close($CS);
  my $liftUpFile = "$assemblyDir/suggested.lft";
  # First see what we have in ContigInfo:
  my ($liftUpCount, $missingInfo) =
    &tryToMakeLiftUpFromContigInfo($liftUpFile, $runDir, \%chromSizes,
				   $grepOutLabels, $grepOutContigs);
  my $message = "";
  if ($liftUpCount > 0) {
    $message = "\n*** $ContigInfo has coords for $liftUpCount sequences; these have been written to
*** $liftUpFile .\n";
  }
  my $missingCount = scalar(@{$missingInfo});
  if ($missingCount > 0) {
    # If we didn't find liftUp info for some contigs in ContigInfo, look in ncbiAssemblyReportFile
    # if given:
    if (! $ncbiAssemblyReportFile) {
      $ncbiAssemblyReportFile = getNcbiAssemblyReportFile();
    }
    if ($ncbiAssemblyReportFile) {
      (my $thisLiftUpCount, $missingInfo) =
	&tryToMakeLiftUpFromNcbiAssemblyReportFile($liftUpFile, \%chromSizes, $missingInfo);
      if ($thisLiftUpCount > 0) {
	$message .= "
*** $ncbiAssemblyReportFile has mappings for $thisLiftUpCount sequences;
*** these have been written to
*** $liftUpFile .\n";
      }
    }
    $missingCount = scalar(@{$missingInfo});
    if ($missingCount > 0) {
      # Write missing info to file.
      my $noInfoSeqFile = "$assemblyDir/cantLiftUpSeqNames.txt";
      my $NI = &HgAutomate::mustOpen(">$noInfoSeqFile");
      foreach my $i (@{$missingInfo}) {
	print $NI join("\t", @{$i}) . "\n";
      }
      close($NI);
      $message .= "
*** $missingCount lines of $ContigInfo.bcp.gz contained contig names that
*** could not be mapped to chrom.size via their GenBank contig mappings; see
*** $noInfoSeqFile .\n";
    }
  }
  return $message;
} # tryToMakeLiftUp


sub checkSequenceNames {
  # Make sure that the reference sequence names from ContigInfo
  # (limited to refAssemblyLabel spec if applicable) match the
  # sequence names in liftUp file (if spec'd) or chrom.sizes.
  my ($runDir, $grepOutLabels, $grepOutContigs) = @_;
  my $chromSizes = "$HgAutomate::clusterData/$db/chrom.sizes";
  my $ucscSeqCommand;
  if ($liftUp) {
    $ucscSeqCommand = "cut -f 2 $liftUp | sort > $runDir/ucscSeqs.txt";
  } else {
    if (! -e $chromSizes) {
      die "*** Can't find file $chromSizes, so can't check sequence names";
    }
    $ucscSeqCommand = "cut -f 1 $chromSizes | sort > $runDir/ucscSeqs.txt";
  }
  &HgAutomate::run("$HgAutomate::runSSH $workhorse $ucscSeqCommand");
  my $hg19MitoTweak = ($db eq 'hg19') ? "| grep -vw ^NC_012920" : "";
#*** ssh and pipes don't really jive.  mini-script?
  &HgAutomate::run("$HgAutomate::runSSH $workhorse " .
		   "zcat $runDir/data/$ContigInfo.bcp.gz $grepOutLabels  $grepOutContigs" .
		   "| cut -f $contigAccCol $hg19MitoTweak | sort " .
		   "  > $runDir/dbSnpContigs.txt;");
  &HgAutomate::verbose(1, "FYI First 10 UCSC sequences not in dbSNP (if any):\n");
#*** Also, HgAutomate::run should probably use SafePipe internally when there is a |
  &HgAutomate::run("comm -23 $runDir/ucscSeqs.txt $runDir/dbSnpContigs.txt | head");
  &HgAutomate::run("comm -13 $runDir/ucscSeqs.txt $runDir/dbSnpContigs.txt " .
		   "  > $runDir/dbSnpContigsNotInUcsc.txt");
  my $badContigCount = $opt_debug ? 0 : `wc -l < $runDir/dbSnpContigsNotInUcsc.txt`;
  if ($badContigCount > 0) {
    chomp $badContigCount;
    my $grepDesc = $refAssemblyLabel ? " (limited to $refAssemblyLabel)" : "";
    my $ucscSource = $liftUp ? $liftUp : $chromSizes;
    my $ContigInfoLiftUp = &tryToMakeLiftUp($runDir, $chromSizes, $grepOutLabels, $grepOutContigs);
    my $msg = "
*** $ContigInfo$grepDesc has $badContigCount contig_acc values
*** that are not in $ucscSource .
*** They are listed in $runDir/dbSnpContigsNotInUcsc.txt
$ContigInfoLiftUp
*** You must account for all $badContigCount contig_acc values in $CONFIG,
*** using the liftUp and/or ignoreDbSnpContigsFile settings (see -help output).
";
    if ($ncbiAssemblyReportFile) {
      $msg .=
"*** Check the auto-generated suggested.lft to see if it covers all
*** $badContigCount contigs; if it does, add 'liftUp suggested.lft' to $CONFIG.
";
    }
    $msg .=
"*** Then run again with -continue=loadDbSnp .

";
    if (! $ncbiAssemblyReportFile) {
      $msg .= "
*** NOTE: If you add the ncbiAssemblyReportFile setting to $CONFIG and
***       run again with -continue=loadDbSnp, this script may be able to
***       construct those files for you.
";
    }
    die $msg;
  }
} # checkSequenceNames


sub loadDbSnp {
  # Do some consistency checks, create a local mysql database, and load
  # dbSNP's dump files into it.
  &translateSql();
  # Check for multiple reference assembly labels -- developer may need to exclude some.
  my @rejectLabels = &checkAssemblySpec();
  my $runDir = "$assemblyDir";
  # If liftUp is a relative path, prepend $runDir/
  if ($liftUp && $liftUp !~ /^\//) {
    $liftUp = "$runDir/$liftUp";
  }
  # Likewise for $ignoreDbSnpContigsFile:
  if ($ignoreDbSnpContigsFile && $ignoreDbSnpContigsFile !~ /^\//) {
    $ignoreDbSnpContigsFile = "$runDir/$ignoreDbSnpContigsFile";
  }
  # Prepare grep -v statements to exclude assembly labels or contigs if specified:
  my $grepOutLabels = @rejectLabels ? "| egrep -vw '(" . join('|', @rejectLabels) . ")' " : "";
  my $grepOutContigs = "";
  if ($ignoreDbSnpContigsFile) {
    $grepOutContigs = "| grep -vwFf '$ignoreDbSnpContigsFile'";
  } elsif ($ignoreDbSnpContigs) {
    $grepOutContigs = "| egrep -vw '$ignoreDbSnpContigs'";
  }

  &checkSequenceNames($runDir, $grepOutLabels, $grepOutContigs);

  my $tmpDb = $db . $snpBase;
  my $dataDir = "$runDir/data";
  # mysql warnings about datetime values that end with non-integer
  # seconds and missing values in numeric columns cause hgLoadSqlTab
  # to return nonzero, and then this script terminates.  Fix datetimes
  # and change missing values to \\N (mysql's file encoding of NULL):
  my $cleanDbSnpSql = 's/(\d\d:\d\d:\d\d)\.\d+/$1/g; ' .
    's/\t(\t|\n)/\t\\\\N$1/g; s/\t(\t|\n)/\t\\\\N$1/g;';

  my $whatItDoes = "It loads a subset of dbSNP tables into a local mysql database.";
  my $bossScript = new HgRemoteScript("$runDir/loadDbSnp.csh",
				      $workhorse, $runDir, $whatItDoes, $CONFIG);
  $bossScript->add(<<_EOF_
    # Work in local tmp disk to save some I/O time, and copy results back to
    # $runDir when done.

    #*** HgAutomate should probably have a sub that generates these lines:
    if (-d /data/tmp) then
      setenv TMPDIR /data/tmp
    else
      if (-d /tmp) then
        setenv TMPDIR /tmp
      else
        echo "Can't find TMPDIR"
        exit 1
      endif
    endif

    set tmpDir = `mktemp -d \$TMPDIR/$base.translate.XXXXXX`
    chmod 775 \$tmpDir
    cd \$tmpDir
    echo \$tmpDir > $runDir/workingDir

    # load dbSNP database tables into local mysql
    hgsql -e 'drop database if exists $tmpDb'
    hgsql -e 'create database $tmpDb'
    hgsql $tmpDb < $runDir/schema/table.sql

    foreach t ($ContigInfo $ContigLocusId $MapInfo)
      zcat $dataDir/\$t.bcp.gz $grepOutLabels $grepOutContigs\\
      | perl -wpe '$cleanDbSnpSql' \\
        > tmp.tab
      hgLoadSqlTab -oldTable $tmpDb \$t placeholder tmp.tab
      rm tmp.tab
    end
    hgsql $tmpDb -e \\
      'alter table $ContigInfo add index (ctg_id); \\
       alter table $ContigLocusId add index (ctg_id); \\
       alter table $MapInfo add index (snp_id);'

    # Make sure there are no orient != 0 contigs among those selected.
    set badCount = `hgsql $tmpDb -NBe \\
                      'select count(*) from $ContigInfo where orient != 0;'`
    if (\$badCount > 0) then
      echo "found \$badCount contigs in $ContigInfo with orient != 0"
      echo "Going to try continuing anyway, marking those as - strand in the lift spec"
    #  exit 1
    endif

    # $ContigLoc is huge, and we want only the reference contig mappings.
    # Keep lines only if they have a word match to some reference contig ID.
    # That allows some false positives from coord matches; clean those up afterward.
    zcat $dataDir/$ContigInfo.bcp.gz $grepOutLabels\\
    | cut -f $ctgIdCol | sort -n > $ContigInfo.ctg_id.txt
    zcat $dataDir/$ContigLoc.bcp.gz \\
    | grep -Fwf $ContigInfo.ctg_id.txt \\
    | perl -wpe '$cleanDbSnpSql' \\
    | hgLoadSqlTab -oldTable $tmpDb $ContigLoc placeholder stdin
    # Get rid of those false positives:
    hgsql $tmpDb -e 'alter table $ContigLoc add index (ctg_id);'
    hgsql $tmpDb -e 'create table ContigLocFix select cl.* from $ContigLoc as cl, $ContigInfo as ci where cl.ctg_id = ci.ctg_id;'
    hgsql $tmpDb -e 'alter table ContigLocFix add index (ctg_id);'
    hgsql $tmpDb -e 'drop table $ContigLoc; \\
                         rename table ContigLocFix to $ContigLoc;'
    hgsql $tmpDb -e 'alter table $ContigLoc add index (snp_id);'

    zcat $dataDir/SNP.bcp.gz \\
    | perl -wpe '$cleanDbSnpSql' \\
    | hgLoadSqlTab -oldTable $tmpDb SNP placeholder stdin
    # Add indices to tables for a big join:
    hgsql $tmpDb -e 'alter table SNP add index (snp_id);'

    # New additions to our pipeline as of 132:

    zcat $buildDir/shared/Allele.bcp.gz \\
    | perl -wpe '$cleanDbSnpSql' \\
    | hgLoadSqlTab -oldTable $tmpDb Allele placeholder stdin
    hgsql $tmpDb -e 'alter table Allele add index (allele_id);'

    foreach t (Batch SNPAlleleFreq SNPSubSNPLink SNP_bitfield SubSNP)
      zcat $dataDir/\$t.bcp.gz \\
      | perl -wpe '$cleanDbSnpSql' \\
      | hgLoadSqlTab -oldTable $tmpDb \$t placeholder stdin
    end
    # Watch out for NULL freq (sum of counts == 0) seen in b141:
    hgsql $tmpDb -e 'delete from SNPAlleleFreq where freq IS NULL'

    hgsql $tmpDb -e 'alter table Batch add index (batch_id); \\
                     alter table SNPAlleleFreq add index (snp_id); \\
                     alter table SNPSubSNPLink add index (subsnp_id); \\
                     alter table SNP_bitfield add index (snp_id); \\
                     alter table SubSNP add index (subsnp_id);'

    foreach t ($ContigInfo $ContigLoc $ContigLocusId $MapInfo SNP \\
               Allele Batch SNPAlleleFreq SNPSubSNPLink SNP_bitfield SubSNP)
      hgsql -N -B $tmpDb -e 'select count(*) from '\$t
    end
_EOF_
		  );
  if ($needSNPAlleleFreq_TGP) {
    $bossScript->add(<<_EOF_

    zcat $dataDir/SNPAlleleFreq_TGP.bcp.gz \\
    | perl -wpe '$cleanDbSnpSql' \\
    | hgLoadSqlTab -oldTable $tmpDb SNPAlleleFreq_TGP placeholder stdin
    hgsql $tmpDb -e 'alter table SNPAlleleFreq_TGP add index (snp_id);'
    hgsql -N -B $tmpDb -e 'select count(*) from SNPAlleleFreq_TGP'
_EOF_
		    );
  }

  $bossScript->execute();
} # loadDbSnp


#########################################################################
# * step: addToDbSnp [workhorse]

sub addToDbSnp {
  my $runDir = "$assemblyDir";
  my $tmpDb = $db . $snpBase;
  my $whatItDoes =
"It pre-processes functional annotations into a new table $tmpDb.ucscFunc,
pre-processes submitted snp submitter handles into a new table
$tmpDb.ucscHandles, and extracts info from fasta headers into a new table
$tmpDb.ucscGnl.";

  my $bossScript = new HgRemoteScript("$runDir/addToDbSnp.csh",
				      $workhorse, $runDir, $whatItDoes, $CONFIG);
  $bossScript->add(<<_EOF_
    set tmpDir = `cat $runDir/workingDir`
    cd \$tmpDir

    #######################################################################
    # Glom each SNP's function codes together and load up a new $tmpDb table.
    # Also extract NCBI's annotations of coding SNPs' effects on translation.
    # We extract $ContigLocusId info only for reference assembly mapping.
    # Some SNP's functional annotations are for an alternate assembly, so we will
    # have no NCBI functional annotations to display for those (but our own are
    # available).
    # Add indices to tables for a big join:
    hgsql $tmpDb -NBe 'select snp_id, ci.contig_acc, asn_from, asn_to, mrna_acc, \\
                           fxn_class, reading_frame, allele, residue, codon, cli.ctg_id \\
                           from $ContigLocusId as cli, $ContigInfo as ci \\
                           where cli.ctg_id = ci.ctg_id;' \\
      > ncbiFuncAnnotations.txt
    wc -l ncbiFuncAnnotations.txt
    # Ignore function code 8 (cds-reference, just means that some allele matches reference)
    # and glom functions for each SNP id:
    cut -f 1-4,6,11 ncbiFuncAnnotations.txt \\
    | sort -u -k1n,1n -k6n,6n -k3n,3n -k5n,5n \\
    | perl -we 'while (<>) { chomp; \\
                  (\$id, undef, \$s, \$e, \$f, \$c) = split; \\
                  if (defined \$prevId && \$id == \$prevId && \$c == \$prevC && \$s == \$prevS) { \\
                    \$prevFunc .= "\$f," unless (\$f == 8); \\
                  } else { \\
                    if (defined \$prevId) { \\
                      print "\$prevId\\t\$prevC\\t\$prevS\\t\$prevE\\t\$prevFunc\\n" if (\$prevFunc); \\
                    } \\
                    \$prevFunc = (\$f == 8) ? "" : "\$f,"; \\
                  } \\
                  (\$prevId, \$prevC, \$prevS, \$prevE) = (\$id, \$c, \$s, \$e); \\
                } \\
                print "\$prevId\\t\$prevC\\t\$prevS\\t\$prevE\\t\$prevFunc\\n" if (\$prevFunc);' \\
      > ucscFunc.txt

    wc -l ucscFunc.txt
    cat > ucscFunc.sql <<EOF
CREATE TABLE ucscFunc (
        snp_id int NOT NULL ,
        ctg_id int(10) NOT NULL ,
        asn_from int(10) NOT NULL ,
        asn_to int(10) NOT NULL ,
        fxn_class varchar(255) NOT NULL ,
        INDEX snp_id (snp_id),
        INDEX ctg_id (ctg_id)
);
EOF
    sleep 1
    hgLoadSqlTab $tmpDb ucscFunc{,.sql,.txt}
    # ucscFunc coords are NCBI's 0-based, fully-closed, 2-base-wide insertions.
    # We need to leave the coords alone here so ucscFunc can be joined below.
    # Make a list of SNPs with func anno's that are insertion SNPs, so we can use 
    # the list to determine what type of coord fix to apply to each annotation
    # when making snp130CodingDbSnp below.
    hgsql $tmpDb -NBe \\
      'select ci.contig_acc, cl.asn_from, cl.asn_to, uf.snp_id \\
       from ucscFunc as uf, $ContigLoc as cl, $ContigInfo as ci \\
       where uf.snp_id = cl.snp_id and \\
             uf.ctg_id = cl.ctg_id and uf.asn_from = cl.asn_from and uf.asn_to = cl.asn_to and \\
             cl.loc_type = 3 and \\
             cl.ctg_id = ci.ctg_id' \\
      > ncbiFuncInsertions.ctg.bed
    wc -l ncbiFuncInsertions.ctg.bed


    #######################################################################
    # Glom together the submitter handles (names) associated with each snp_id:
    hgsql $tmpDb -NBe 'select SNPSubSNPLink.snp_id, handle from SubSNP, SNPSubSNPLink, Batch \\
                       where SubSNP.subsnp_id = SNPSubSNPLink.subsnp_id and \\
                             SubSNP.batch_id = Batch.batch_id' \\
    | sort -k1n,1n -k2,2 -u \\
    | perl -we \\
       'while (<>) { \\
          chomp; my (\$id, \$handle) = split("\\t"); \\
          if (defined \$prevId && \$prevId != \$id) { \\
            print "\$prevId\\t\$handleCount\\t\$handleBlob\\n"; \\
            \$handleCount = 0;  \$handleBlob = ""; \\
          } \\
          \$handleCount++; \\
          \$handleBlob .= "\$handle,"; \\
          \$prevId = \$id; \\
        } \\
        print "\$prevId\\t\$handleCount\\t\$handleBlob\\n";' \\
      > ucscHandles.txt

    cat > ucscHandles.sql <<EOF
CREATE TABLE ucscHandles (
	snp_id int NOT NULL,
	handleCount int unsigned NOT NULL,
	handles longblob NOT NULL,
	INDEX snp_id (snp_id)
);
EOF
    hgLoadSqlTab $tmpDb ucscHandles{,.sql,.txt}

    #######################################################################
    # Glom together the allele frequencies for each snp_id:
_EOF_
		  );
  if ($needSNPAlleleFreq_TGP) {
    my $deDup = ($build >= 138) ? "-deDupTGP" : "";
    my $deDup2 = ($build >= 142) ? "-deDupTGP2" : "";
    $bossScript->add(<<_EOF_
    $Bin/snpAddTGPAlleleFreq.pl $tmpDb -contigLoc=$ContigLoc $deDup $deDup2 > ucscAlleleFreq.txt
_EOF_
		    );
  }
  else {
    $bossScript->add(<<_EOF_
    hgsql $tmpDb -NBe 'select snp_id, allele, chr_cnt, freq from SNPAlleleFreq, Allele \\
                       where SNPAlleleFreq.allele_id = Allele.allele_id' \\
    | perl -we \\
       'while (<>) { \\
          chomp; my (\$id, \$al, \$cnt, \$freq) = split("\\t"); \\
          if (defined \$prevId && \$prevId != \$id) { \\
            print "\$prevId\\t\$alleleCount\\t\$alBlob\\t\$cntBlob\\t\$freqBlob\\n"; \\
            \$alleleCount = 0;  \$alBlob = "";  \$cntBlob = "";  \$freqBlob = ""; \\
          } \\
          \$alleleCount++; \\
          \$alBlob .= "\$al,";  \$cntBlob .= "\$cnt,";  \$freqBlob .= "\$freq,"; \\
          \$prevId = \$id; \\
        } \\
        print "\$prevId\\t\$alleleCount\\t\$alBlob\\t\$cntBlob\\t\$freqBlob\\n";' \\
      > ucscAlleleFreq.txt
_EOF_
		    );
  }
  $bossScript->add(<<_EOF_
    cat > ucscAlleleFreq.sql <<EOF
CREATE TABLE ucscAlleleFreq (
        snp_id int NOT NULL,
        alleleCount int unsigned NOT NULL,
        alleles longblob NOT NULL,
        chr_cnts longblob NOT NULL,
        freqs longblob NOT NULL,
        INDEX snp_id (snp_id)
);
EOF
    hgLoadSqlTab $tmpDb ucscAlleleFreq{,.sql,.txt}


    #######################################################################
    # Extract observed alleles, molType and snp class from FASTA headers gnl

#*** This would be a good place to check for missing flanking sequences

    zcat $runDir/rs_fasta/rs_ch*.fas.gz \\
    | grep '^>gnl' \\
    | perl -wpe 's/""/"/g; s/^\\S+rs(\\d+) .*mol="(\\w+)"\\|class=(\\d+)\\|alleles="([^"]+)"\\|build.*/\$1\\t\$4\\t\$2\\t\$3/ || die "Parse error line \$.:\\n\$_\\n\\t";' \\
    | sort -nu \\
      > ucscGnl.txt
#*** compare output of following 2 commands:
    wc -l ucscGnl.txt
    cut -f 1 ucscGnl.txt | uniq | wc -l
    cat > ucscGnl.sql <<EOF
CREATE TABLE ucscGnl (
        snp_id int NOT NULL ,
        observed varchar(255) NOT NULL,
        molType varchar(255) NOT NULL,
        class varchar(255) NULL ,
        INDEX snp_id (snp_id)
);
EOF
    hgLoadSqlTab $tmpDb ucscGnl{,.sql,.txt}
_EOF_
		  );

  $bossScript->execute();
} # addToDbSnp


#########################################################################
# * step: bigJoin [workhorse]

sub bigJoin {
  my $runDir = "$assemblyDir";
  my $tmpDb = $db . $snpBase;
  my $catOrGrepOutMito = ($db eq 'hg19') ? "grep -vw ^NC_012920" : "cat";
  my $whatItDoes =
"It does a large left join to bring together all of the columns that we want
in $snpBase.";

  my $bossScript = new HgRemoteScript("$runDir/bigJoin.csh",
				      $workhorse, $runDir, $whatItDoes, $CONFIG);
  $bossScript->add(<<_EOF_
    set tmpDir = `cat $runDir/workingDir`
    cd \$tmpDir

    # Big leftie join to bring together all of the columns that we want in $snpBase,
    # using all of the available joining info.  asn_to is incremented in the select
    # output (for 0-based, half-open coords) so that liftUp works properly on reversed
    # contigs; the compensatory subtraction is executed after liftUp happens below.  Without
    # BED 6 formated input, liftUp is unable to adjust the strand correctly, so that is also
    # handled in the select:
     hgsql $tmpDb -NBe \\
     'SELECT ci.contig_acc, cl.asn_from, cl.asn_to+1, cl.snp_id, \\
             if (ci.orient = 1, 1 - cl.orientation, cl.orientation), cl.allele, \\
             ug.observed, ug.molType, ug.class, \\
             s.validation_status, s.avg_heterozygosity, s.het_se, \\
             uf.fxn_class, cl.loc_type, mi.weight, cl.phys_pos_from, \\
             uh.handleCount, uh.handles, \\
             ua.alleleCount, ua.alleles, ua.chr_cnts, ua.freqs, \\
             sb.link_prop_b2, sb.freq_prop, sb.pheno_prop, sb.quality_check \\
      FROM \\
      ((((((($ContigLoc as cl JOIN $ContigInfo as ci ON cl.ctg_id = ci.ctg_id) \\
             LEFT JOIN $MapInfo as mi ON mi.snp_id = cl.snp_id) \\
            LEFT JOIN SNP as s ON s.snp_id = cl.snp_id) \\
           LEFT JOIN ucscGnl as ug ON ug.snp_id = cl.snp_id) \\
          LEFT JOIN ucscFunc as uf ON uf.snp_id = cl.snp_id and uf.ctg_id = cl.ctg_id \\
                                      and uf.asn_from = cl.asn_from) \\
         LEFT JOIN ucscHandles as uh ON uh.snp_id = cl.snp_id) \\
        LEFT JOIN ucscAlleleFreq as ua ON ua.snp_id = cl.snp_id) \\
       LEFT JOIN SNP_bitfield as sb ON sb.snp_id = cl.snp_id;' \\
    | uniq \\
      > ucscNcbiSnp.ctg.bed
    wc -l ucscNcbiSnp.ctg.bed

    # There are some weird cases of length=1 but locType=range... in all the cases
    # that I checked, the length really seems to be 1 so I'm not sure where they got
    # the locType=range.  Tweak locType in those cases so we can keep those SNPs:
#*** probably send warning to file instead of stderr, since we want to add to endNotes:
    $catOrGrepOutMito ucscNcbiSnp.ctg.bed \\
    | awk -F"\\t" 'BEGIN{OFS="\\t";} \\
           \$2 == \$3 - 1 && \$14 == 1 {\$14=2; \\
                                 if (numTweaked < 10) {print \$4 > "/dev/stderr";} \\
                                 numTweaked++;}  {print;} \\
           END{print numTweaked, "single-base, locType=range, tweaked locType" > "/dev/stderr";}' \\
_EOF_
		  );

  if ($liftUp) {
    # If liftUp is a relative path, prepend $runDir/
    $liftUp = "$runDir/$liftUp" if ($liftUp !~ /^\//);
    $bossScript->add(<<_EOF_
    | liftUp -type=.bed3 stdout $liftUp warn stdin \\
        | tawk '{\$3 -= 1; print;}' > ucscNcbiSnp.bed 
_EOF_
		    );
  } else {
    $bossScript->add(<<_EOF_
    | tawk '{\$3 -= 1; print;}' > ucscNcbiSnp.bed
_EOF_
		    );
  }
#*** add stderr output of awk command to endnotes at this point..:
#TODO: examine these again, report to dbSNP:
#118203330
#118203339
#118203340
#118203367
#118203389
#118203401
#118203405
#118203425
#118203428
#118203433
#588     single-base, locType=range, tweaked locType
  if ($db eq 'hg19') {
    $bossScript->add(<<_EOF_
    # For liftOver, convert 0-base fully-closed to 0-based half-open because liftOver
    # doesn't deal with 0-base items.  Fake out phys_pos_from to -1 (missing) because
    # many coords will differ, oh well.
    grep -w NC_012920 ucscNcbiSnp.ctg.bed \\
    | awk -F"\\t" 'BEGIN{OFS="\\t";} {\$3 += 1; \$16 = -1; print;}' \\
    | liftOver -tab -bedPlus=3 stdin \\
        $hg19MitoLiftOver stdout chrM.unmapped \\
    | awk -F"\\t" 'BEGIN{OFS="\\t";} {\$3 -= 1; print;}' \\
    | sort -k2n,2n \\
      > chrMNcbiSnp.bed
    cat chrM.unmapped
#*** developer attention needed if there are more than a few.
    cat chrMNcbiSnp.bed >> ucscNcbiSnp.bed

_EOF_
		    );
  }
    $bossScript->add(<<_EOF_
    wc -l ucscNcbiSnp.bed
_EOF_
		  );

  $bossScript->execute();
} # bigJoin


#########################################################################
# * step: catFasta [fileServer]

sub catFasta {
  my $runDir = "$assemblyDir";
  my $tmpDb = $db . $snpBase;
  my $whatItDoes =
"It concatenates flanking sequence fasta files into one giant indexed file.";
  my $bossScript = new HgRemoteScript("$runDir/catFasta.csh",
				      $fileServer, $runDir, $whatItDoes, $CONFIG);
  $bossScript->add(<<_EOF_
    set tmpDir = `cat $runDir/workingDir`
    cd \$tmpDir

    # Make one big fasta file.
#*** It's a monster: 23G for hg19 snp132, 39G for hg38 snp149!  Can we split by hashing rsId?
#*** Can't use 2bit because it doesn't preserve the line structure that shows which base is
#*** the variant base between the two flanks.
    zcat $runDir/rs_fasta/rs_ch*.fas.gz \\
    | perl -wpe 's/^>gnl\\|dbSNP\\|(rs\\d+) .*/>\$1/ || ! /^>/ || die;' \\
      > $snpBase.fa
    # Use hgLoadSeq to generate .tab output for sequence file offsets,
    # and keep only the columns that we need: acc and file_offset.
    # Convert to snpSeq table format and remove duplicates.
    hgLoadSeq -test placeholder $snpBase.fa
    cut -f 2,6 seq.tab \\
    | sort -k1,1 -u \\
      > ${snpBase}Seq.tab
    rm seq.tab

    # Compress (where possible -- not .fa unfortunately) and copy results back to
    # $runDir
    gzip ${snpBase}Seq.tab
    cp -p ${snpBase}Seq.tab.gz $snpBase.fa $runDir/
    rm ${snpBase}Seq.tab.gz $snpBase.fa
_EOF_
    );

  $bossScript->execute();
} # catFasta


#########################################################################
# * step: translate [workhorse]

sub translate {
  my $runDir = "$assemblyDir";
  my $tmpDb = $db . $snpBase;
  my $whatItDoes =
"It runs snpNcbiToUcsc to make final $snpBase.* files for loading,
cleans up intermediate files and moves results from the temporary
working directory to $runDir.";

  my $parArg = "";
  my $parTable = `echo 'show tables like "par";' | $HgAutomate::runSSH $dbHost hgsql -N $db`;
  chomp $parTable;
  if ($parTable eq "par") {
    &HgAutomate::run("echo 'select chrom,chromStart,chromEnd,name from $parTable' " .
		     "| hgsql $db -NB > `cat workingDir`/par.bed");
    $parArg = "-par=par.bed";
  }
  my $bossScript = new HgRemoteScript("$runDir/translate.csh",
				      $workhorse, $runDir, $whatItDoes, $CONFIG);
  $bossScript->add(<<_EOF_
    set tmpDir = `cat $runDir/workingDir`
    cd \$tmpDir

    # Translate NCBI's encoding into UCSC's, and perform a bunch of checks.
#*** add output to endNotes:
    snpNcbiToUcsc -snp132Ext $parArg ucscNcbiSnp.bed $HgAutomate::clusterData/$db/$db.2bit $snpBase
#*** add output to endNotes:
    head ${snpBase}Errors.bed
#*** add output to endNotes:
    wc -l ${snpBase}*

# Compress and copy results back to
# $runDir
gzip *.txt *.bed *.tab
cp -p * $runDir/
rm \$tmpDir/*

# return to $runDir and clean up tmpDir
cd $runDir
rmdir \$tmpDir
_EOF_
    );

  $bossScript->execute();
} # translate


#########################################################################
# * step: load [dbHost]

sub loadTables {
  my $runDir = "$assemblyDir";
  my $whatItDoes = "It loads the $snpBase* tables into $db.";
  my $bossScript = new HgRemoteScript("$runDir/load.csh",
				      $dbHost, $runDir, $whatItDoes, $CONFIG);
  $bossScript->add(<<_EOF_
    #*** HgAutomate should probably have a sub that generates these lines:
    if (-d /data/tmp) then
      setenv TMPDIR /data/tmp
    else
      if (-d /tmp) then
        setenv TMPDIR /tmp
      else
        echo "Can't find TMPDIR"
        exit 1
      endif
    endif

    # Load up main track tables.
    hgLoadBed -tab -onServer -tmpDir=\$TMPDIR -allowStartEqualEnd -type=bed6+ \\
      $db $snpBase -sqlTable=$snpBase.sql $snpBase.bed.gz

    zcat ${snpBase}ExceptionDesc.tab.gz \\
    | hgLoadSqlTab $db ${snpBase}ExceptionDesc \$HOME/kent/src/hg/lib/snp125ExceptionDesc.sql stdin

    # Load up sequences.
    mkdir -p /gbdb/$db/snp
    if (-l /gbdb/$db/snp/$snpBase.fa) then
      rm /gbdb/$db/snp/$snpBase.fa
    endif
    ln -s $runDir/$snpBase.fa /gbdb/$db/snp/$snpBase.fa
    zcat ${snpBase}Seq.tab.gz \\
    | hgLoadSqlTab $db ${snpBase}Seq \$HOME/kent/src/hg/lib/snpSeq.sql stdin

    # Put in a link where one would expect to find the track build dir...
    if (-l $HgAutomate::clusterData/$db/bed/$snpBase) then
      rm $HgAutomate::clusterData/$db/bed/$snpBase
    endif
    ln -s $runDir $HgAutomate::clusterData/$db/bed/$snpBase

    # Look at the breakdown of exception categories:
    zcat ${snpBase}ExceptionDesc.tab.gz | sort -k 2n,2n
_EOF_
    );

  $bossScript->execute();

#*** Tell developer to ask cluster-admin to pack the $snpBase table (or whatever tables we'll push)
#*** Show summaries e.g. exception breakdown to developer, esp. ones that dbSNP really should
#*** be prodded about.
} # loadTables


#########################################################################
# * step: filter [dbHost]

sub filterTables {
  my $runDir = "$assemblyDir";
  my $whatItDoes = "It extracts subsets of All SNPs: ${snpBase}Common, ${snpBase}Mult etc.";
  my $bossScript = new HgRemoteScript("$runDir/filter.csh",
				      $dbHost, $runDir, $whatItDoes, $CONFIG);
  $bossScript->add(<<_EOF_
    zcat $snpBase.bed.gz \\
    | ~/kent/src/hg/utils/automation/categorizeSnps.pl
    foreach f ({Mult,Common,Flagged}.bed.gz)
      mv \$f $snpBase\$f
    end

    # Load each table (unless its file is empty)
    foreach subset (Mult Common Flagged)
      set table = $snpBase\$subset
      if (`zcat \$table.bed.gz | head | wc -l`) then
        hgLoadBed -tab -onServer -tmpDir=/data/tmp -allowStartEqualEnd -renameSqlTable \\
          $db \$table -sqlTable=$snpBase.sql \$table.bed.gz
      endif
    end
    rm -f bed.tab
_EOF_
    );
  $bossScript->execute();
} # filterTables


#########################################################################
# * step: coding [dbHost]

sub codingDbSnp {
  my $runDir = "$assemblyDir";
  my $whatItDoes = "It processes dbSNP's functional annotations into ${snpBase}CodingDbSnp.";
  my $bossScript = new HgRemoteScript("$runDir/coding.csh",
				      $dbHost, $runDir, $whatItDoes, $CONFIG);
  $liftUp = "$runDir/$liftUp" if ($liftUp !~ /^\//);
  $bossScript->add(<<_EOF_
    zcat ncbiFuncAnnotations.txt.gz \\
    | $Bin/fixNcbiFuncCoords.pl ncbiFuncInsertions.ctg.bed.gz \\
    | sort -k1n,1n -k2,2 -k3n,3n -k5,5 -k6n,6n \\
    | uniq \\
      > ncbiFuncAnnotationsFixed.txt
    wc -l ncbiFuncAnnotationsFixed.txt
    cut -f 6 ncbiFuncAnnotationsFixed.txt \\
    | sort -n | uniq -c
    $Bin/collectNcbiFuncAnnotations.pl ncbiFuncAnnotationsFixed.txt \\
    | liftUp -type=.bed3 ${snpBase}CodingDbSnp.bed $liftUp warn stdin
    hgLoadBed $db ${snpBase}CodingDbSnp -sqlTable=\$HOME/kent/src/hg/lib/snp125Coding.sql \\
      -renameSqlTable -tab -notItemRgb -allowStartEqualEnd \\
      ${snpBase}CodingDbSnp.bed
_EOF_
    );
  $bossScript->execute();
} # codingDbSnp


#########################################################################
# * step: bigBed [dbHost]

sub bigBed {
  my $runDir = "$assemblyDir";
  my $whatItDoes = "It creates a bed4 bigBed file of SNP positions for use by hgVai.";
  my $bossScript = new HgRemoteScript("$runDir/bigBed.csh",
				      $dbHost, $runDir, $whatItDoes, $CONFIG);
  my $chromSizes = "$HgAutomate::clusterData/$db/chrom.sizes";
  if (! -e $chromSizes) {
    die "*** Can't find file $chromSizes, so can't create bigBed file";
  }
  my $destDir = "$HgAutomate::gbdb/$db/vai";
  $bossScript->add(<<_EOF_
    #*** HgAutomate should probably have a sub that generates these lines:
    if (-d /data/tmp) then
      setenv TMPDIR /data/tmp
    else
      if (-d /tmp) then
        setenv TMPDIR /tmp
      else
        echo "Can't find TMPDIR"
        exit 1
      endif
    endif

    # Make bigBed with just the first 4 columns of the snp table
    zcat $snpBase.bed.gz \\
    | cut -f 1-4 \\
    | sort -k1,1 -k2,2n \\
      > \$TMPDIR/$db.$snpBase.bed4
    bedToBigBed \$TMPDIR/$db.$snpBase.bed4 $chromSizes $snpBase.bed4.bb
    # Install in location that hgVai will check:
    mkdir -p $destDir
    rm -f $destDir/$snpBase.bed4.bb
    ln -s `pwd`/$snpBase.bed4.bb $destDir/
    # Clean up
    rm \$TMPDIR/$db.$snpBase.bed4
_EOF_
  );
  $bossScript->execute();
} # bigBed

#########################################################################
# main

# Prevent "Suspended (tty input)" hanging:
&HgAutomate::closeStdin();

&checkOptions();
&usage(1) if (scalar(@ARGV) != 1);

($CONFIG) = @ARGV;
my $config = &parseConfig($CONFIG);
&checkConfig($config);

$buildDir = $opt_buildDir ? $opt_buildDir : "$dbSnpRoot/$build";
($commonName = $orgDir) =~ s/_\d+.*$//;
# orgDir used to be only human_9606 and the like, but not sometimes they append
# build ID and/or assembly ID to the end, so for example they can have separate
# directories for GRCh37 and GRCh38.  The file names don't have the new suffixes,
# so make a trimmed version
($orgDirTrimmed = $orgDir) =~ s/(_\d+)_.*/$1/;
$assemblyDir = "$buildDir/${commonName}_$db";
$assemblyLabelFile = "$assemblyDir/assemblyLabels.txt";
if ($buildAssembly ne "") {
  $ContigInfo = "b${build}_ContigInfo_$buildAssembly";
  $ContigLoc = "b${build}_SNPContigLoc_$buildAssembly";
  $ContigLocusId = "b${build}_SNPContigLocusId_$buildAssembly";
  $MapInfo = "b${build}_SNPMapInfo_$buildAssembly";
} else {
  $ContigInfo = "b${build}_ContigInfo";
  $ContigLoc = "b${build}_SNPContigLoc";
  $ContigLocusId = "b${build}_SNPContigLocusId";
  $MapInfo = "b${build}_SNPMapInfo";
}
$endNotes = "";
$needSNPAlleleFreq_TGP = ($commonName eq "human");
$snpBase = "snp$build" if (! $snpBase);

$stepper->execute();

my $stopStep = $stepper->getStopStep();
my $upThrough = ($stopStep eq 'coding') ? "" :
  "  (through the '$stopStep' step)";

HgAutomate::verbose(1,
	"\n *** All done!$upThrough\n");

if ($endNotes) {
  #*** Should mail this to $ENV{'USER'} so it's not so easy to ignore.
  #*** Does mail work on all of our machines??  Even if it works on one,
  #*** we can ssh it.  Should be in an HgAutomate routine.
  HgAutomate::verbose(0,
		      "\n\nNOTES -- STUFF THAT YOU WILL HAVE TO DO --\n\n"
		      . "$endNotes\n"
		      );
}
