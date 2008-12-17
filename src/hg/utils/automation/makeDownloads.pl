#!/usr/bin/env perl

# DO NOT EDIT the /cluster/bin/scripts copy of this file --
# edit ~/kent/src/hg/utils/automation/makeDownloads.pl instead.

# $Id: makeDownloads.pl,v 1.18 2008/12/02 17:36:44 angie Exp $

use Getopt::Long;
use warnings;
use strict;
use FindBin qw($Bin);
use lib "$Bin";
use HgAutomate;
use HgRemoteScript;
use HgStepManager;

# Option variable names:
use vars @HgAutomate::commonOptionVars;
use vars @HgStepManager::optionVars;

# Specify the steps supported with -continue / -stop:
my $stepper = new HgStepManager(
    [ { name => 'compress', func => \&doCompress },
      { name => 'install',  func => \&doInstall },
    ]
				);

# Option defaults:
my $defaultBigClusterHub = 'most available';
my $defaultSmallClusterHub = 'n/a';
my $defaultWorkhorse = 'least loaded';
my $dbHost = 'hgwdev';

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
  print STDERR &HgAutomate::getCommonOptionHelp('dbHost' => $dbHost,
					'workhorse' => $defaultWorkhorse);
  print STDERR "
Automates generation of assembly download files for genome database \$db:
    compress: Create compressed download files, md5sum.txt and README.txt in
              $HgAutomate::clusterData/\$db/goldenPath/*/
    install:  Create links to those files from
              $dbHost:$HgAutomate::goldenPath/\$db/*/
This will blow away any existing README.txt files and any files that are
already in bigZips etc.  So if you have added files specially for this
release (include README.txt sections), and then need to run this again,
be sure to back them up in a different directory first.
";
  # Detailed help (-help):
  print STDERR "
Assumptions:
1. $HgAutomate::clusterData/\$db/{\$db.2bit,chrom.sizes} are in place.
2. AGP, RepeatMasker .out and trfBig .bed files are in their usual places under
   $HgAutomate::clusterData/\$db/ .  (Will complain if not able to find.)
" if ($detailed);
  print "\n";
  exit $status;
}


# Globals:
# Command line args: db
my ($db);
# Other:
my ($topDir, $scriptDir, $trfRunDir, $trfRunDirRel);
my ($chromBased, @chroms, %chromRoots, $chromGz, $geneTable);

sub checkOptions {
  # Make sure command line options are valid/supported.
  my $ok = GetOptions(@HgStepManager::optionSpec,
		      @HgAutomate::commonOptionSpec,
		      );
  &usage(1) if (!$ok);
  &usage(0, 1) if ($opt_help);
  &HgAutomate::processCommonOptions();
  my $err = $stepper->processOptions();
  usage(1) if ($err);
  $dbHost = $opt_dbHost if ($opt_dbHost);
}


#*** libify?
sub dbHasTable {
  my ($dbHost, $db, $table) = @_;
  my $rows = `echo show tables like "'$table'" |
              ssh -x $dbHost hgsql -N $db | wc -l`;
  return ($rows > 0);
} # dbHasTable


sub getGeneTable {
  # If there is a suitable table for generating upstream genes, return it.
  foreach my $table ('refGene', 'mgcGenes') {
    if (&dbHasTable($dbHost, $db, $table)) {
      return $table;
    }
  }
  return undef;
} # getGeneTable


#########################################################################
# * step: template [workhorse]

sub compressChromFiles {
  # To be called only when assembly is chrom-based.
  # Expect to find per-chromosome .agp and RepeatMasker .out files in
  # directories with names distilled from chrom names.
  # Expect to find filtered TRF .bed files in
  # $topDir/bed/simpleRepeat/trfMaskChrom/ .
  # Get masked sequence directly from .2bit.
  # Add commands to $bossScript that will create .tar.gz compressed archive
  # files with per-chrom files from each of those categories.
  my ($runDir, $bossScript) = @_;
  my @chromAgpFiles = ();
  my @chromOutFiles = ();
  my @chromTrfFiles = ();
  my $problems = 0;
  my ($agpFudge, $rmFudge, $trfFudge) = (0, 0, 0);
  foreach my $chrRoot (sort keys %chromRoots) {
    foreach my $chr (@{$chromRoots{$chrRoot}}) {
      my $agpFile = "$chrRoot/$chr.agp";
      my $outFile = "$chrRoot/$chr.fa.out";
      my $trfFile = "trfMaskChrom/$chr.bed";
      if (-e "$topDir/$agpFile") {
	push @chromAgpFiles, $agpFile;
      } elsif ($chr eq 'chrM') {
	# It is OK to lack AGP for chrM, which we sometimes add to assemblies.
	$agpFudge++;
      } else {
	warn "Missing AGP $agpFile\n";
	$problems++;
      }
      if (-e "$topDir/$outFile") {
	push @chromOutFiles, $outFile;
      } elsif ($chr eq 'chrM') {
	# It is OK to lack RepeatMasker output for chrM too.
	$rmFudge++;
      } else {
	warn "Missing RepeatMasker $outFile\n";
	$problems++;
      }
      if (-e "$trfRunDir/$trfFile") {
	push @chromTrfFiles, $trfFile;
      } elsif ($trfFile =~ /chrM\.bed$/) {
	$trfFudge++;
      } else {
	warn "Missing TRF $trfFile\n";
	$problems++;
      }
    }
    if ($problems > 15) {
      warn "A bunch of missing files... stopping here.\n";
      last;
    }
  }
  if (((scalar(@chromAgpFiles) + $agpFudge) != scalar(@chroms)) ||
      ((scalar(@chromOutFiles) + $rmFudge) != scalar(@chroms)) ||
      ((scalar(@chromTrfFiles) + $trfFudge) != scalar(@chroms))) {
    die "Sorry, can't find the expected set of per-chromosome files.";
  }
  $bossScript->add(<<_EOF_
# For the time being, use $chromGz/ to temporarily store uncompressed
# 2bit-derived .fa and .fa.masked files:
rm -rf $chromGz
mkdir $chromGz
foreach chr ( @chroms )
  twoBitToFa $topDir/$db.2bit -seq=\$chr $chromGz/\$chr.fa
  maskOutFa $chromGz/\$chr.fa hard $chromGz/\$chr.fa.masked
end

# Make compressed archive files of per-chrom .agp, .out, TRF .bed,
# soft- and hard-masked .fa:
cd $topDir

tar cvzf $runDir/bigZips/chromAgp.tar.gz @chromAgpFiles

tar cvzf $runDir/bigZips/chromOut.tar.gz @chromOutFiles

cd $runDir/$chromGz
tar cvzf $runDir/bigZips/chromFa.tar.gz *.fa

tar cvzf $runDir/bigZips/chromFaMasked.tar.gz *.fa.masked

cd $trfRunDir
tar cvzf $runDir/bigZips/chromTrf.tar.gz @chromTrfFiles

# Now fix $chromGz/ up proper:
cd $runDir/$chromGz
rm *.fa.masked
gzip *.fa

_EOF_
  );
} # compressChromFiles


sub mustFindOne {
  # Return the first existing file under $topDir/ in the given list of
  # candidate files, or die if none exist.
  my @candidates = @_;
  my $firstFound;
  foreach my $f (@candidates) {
    if (-e "$topDir/$f") {
      $firstFound = "$topDir/$f";
      last;
    }
  }
  if (! defined $firstFound) {
    die "Sorry, can't find any of these: {" .
      join(", ", @candidates) . "}";
  }
  return $firstFound;
} # mustFindOne

sub compressScaffoldFiles {
  # To be called only when assembly is scaffold-based.
  # Expect to find monolithic files containing AGP, RepeatMasker .out, and
  # filtered TRF .bed.
  # Get masked sequence directly from .2bit.
  # Add commands to $bossScript that will create .gz compressed files
  # from each of those categories.
  my ($runDir, $bossScript) = @_;
  my $hgFakeAgpDir = "$HgAutomate::trackBuild/hgFakeAgp";
  my $agpFile = &mustFindOne("$db.agp", 'scaffolds.agp',
			     "$hgFakeAgpDir/$db.agp",
			     "$hgFakeAgpDir/scaffolds.agp");
  my $outFile = &mustFindOne("$db.fa.out", 'scaffolds.out');
  my $trfFile = &mustFindOne("$trfRunDirRel/trfMask.bed",
			     "$trfRunDirRel/scaffolds.bed");
  $bossScript->add(<<_EOF_
# Make compressed files of .agp, .out, TRF .bed, soft- and hard-masked .fa:
cd $runDir/bigZips

gzip -c $agpFile > $db.agp.gz
gzip -c $outFile > $db.fa.out.gz
gzip -c $trfFile > $db.trf.bed.gz

twoBitToFa $topDir/$db.2bit stdout \\
| gzip -c > $db.fa.gz

twoBitToFa $topDir/$db.2bit stdout \\
| maskOutFa stdin hard stdout \\
| gzip -c > $db.fa.masked.gz

_EOF_
  );
} # compressScaffoldFiles


sub isBaylor {
  # Return true if it looks like this assembly is from Baylor.
  my ($assemblyLabel) = @_;
  return ($assemblyLabel =~ /Baylor/);
}

sub isWustl {
  # Return true if it looks like this assembly is from WUSTL.
  my ($assemblyLabel) = @_;
  return ($assemblyLabel =~ /(WUSTL|WashU|Washington Univ|Chicken)/);
}

sub printAssemblyUsage {
  # Print out conditions of use for this assembly.
  my ($fh, $Organism, $assemblyLabel) = @_;
  if (&isBaylor($assemblyLabel)) {
    print $fh <<_EOF_
For conditions of use regarding the $Organism genome sequence data, see
http://www.hgsc.bcm.tmc.edu/projects/conditions_for_use.html .

_EOF_
    ;
  } elsif (&isWustl($assemblyLabel)) {
    print $fh <<_EOF_
The $Organism sequence is made freely available to the community by the
Genome Sequencing Center, Washington University School of Medicine, with
the following understanding:

1. The data may be freely downloaded, used in analyses, and repackaged in
   databases.

2. Users are free to use the data in scientific papers analyzing these data
   if the providers of these data are properly acknowledged.  See
   http://genome.ucsc.edu/goldenPath/credits.html for credit information.

*** IF GENOME HAS BEEN PUBLISHED -- ADD CITATION ***
*** IF GENOME HAS NOT YET BEEN PUBLISHED: ***
3. The centers producing the data reserve the right to publish the initial
   large-scale analyses of the data set, including large-scale identification
   of regions of evolutionary conservation and large-scale genomic assembly.
   Large-scale refers to regions with size on the order of a chromosome (that
   is, 30 Mb or more).

4. Any redistribution of the data should carry this notice.

_EOF_
    ;
  } elsif ($assemblyLabel =~ /JGI/) {
    print $fh <<_EOF_
1. The data may be freely downloaded, used in analyses, and repackaged
   in databases.

2. Users are free to use the data in scientific papers analyzing
   particular genes and regions if the provider of these data
   (DOE Joint Genome Institute) is properly acknowledged.  See
   http://genome.ucsc.edu/goldenPath/credits.html for credit information.

3. *** PLEASE ADD PUBLICATION PLANS, IF ANY ***

4. Any redistribution of the data should carry this notice.

_EOF_
    ;
} elsif ($assemblyLabel =~ /Broad/) {
    print $fh <<_EOF_
*** PLEASE CONFIRM THESE CONDITIONS AND/OR ALTER TO INCLUDE ANY PUBLICATION ***
The $Organism sequence is made freely available before scientific publication 
with the following understanding:

   1. The data may be freely downloaded, used in analyses, and repackaged in 
      databases.
   2. Users are free to use the data in scientific papers analyzing particular 
      genes and regions if the provider of these data (The Broad Institute) is 
      properly acknowledged.
   3. The center producing the data reserves the right to publish the initial 
      large-scale analyses of the data set, including large-scale identification 
      of regions of evolutionary conservation and large-scale genomic assembly. 
      Large-scale refers to regions with size on the order of a chromosome (that 
      is, 30 Mb or more).
   4. Any redistribution of the data should carry this notice. 1. The data may 
      be freely downloaded, used in analyses, and repackaged in databases.

_EOF_
    ;
  } else {
    print $fh <<_EOF_

*** PLEASE PASTE IN CONDITIONS OF USE FOR THIS ASSEMBLY IF THERE ARE ANY ***

_EOF_
    ;
  }
} # printAssemblyUsage

sub printSomeHaveConditions {
  # Print out a warning that some tables have conditions for use.
  my ($fh) = @_;
  print $fh <<_EOF_
All the files and tables in this directory are freely usable for any
purpose except for the following:

_EOF_
  ;
}

sub printAllAreFree {
  # State that all tables are freely available.
  my ($fh) = @_;
  print $fh <<_EOF_
All the files and tables in this directory are freely usable for any purpose.

_EOF_
  ;
}

sub printTableSpecificUsage {
  # If tables exist that have specific conditions for use, print out the
  # conditions.
  my ($fh) = @_;
  my $gotConditions = 0;

  if (&dbHasTable($dbHost, $db, 'softBerryGene')) {
    &printSomeHaveConditions() if (! $gotConditions);
    $gotConditions = 1;
    print $fh <<_EOF_
   softberryGene.txt and softberryPep.txt -  Free for academic 
        and nonprofit use. Commercial users should contact
        Softberry, Inc. at http://www.softberry.com.

_EOF_
    ;
  }

  if (&dbHasTable($dbHost, $db, 'knownGene')) {
    &printSomeHaveConditions() if (! $gotConditions);
    $gotConditions = 1;
    print $fh <<_EOF_
   Swiss-Prot/UniProt data in knownGene.txt - 
        UniProt copyright (c) 2002 - 2004 UniProt consortium

        For non-commercial use all databases and documents in the UniProt FTP
        directory may be copied and redistributed freely, without advance 
        permission, provided that this copyright statement is reproduced with 
        each copy. 

        For commercial use all databases and documents in the UniProt FTP 
        directory, except the files

        ftp://ftp.uniprot.org/pub/databases/uniprot/knowledgebase/uniprot_sprot.dat.gz

        and

        ftp://ftp.uniprot.org/pub/databases/uniprot/knowledgebase/uniprot_sprot.xml.gz

        may be copied and redistributed freely, without advance permission, 
        provided that this copyright statement is reproduced with each copy.

        More information for commercial users can be found in:
        http://www.expasy.org/announce/sp_98.html

        From January 1, 2005, all databases and documents in the UniProt FTP 
        directory may be copied and redistributed freely by all entities, 
        without advance permission, provided that this copyright statement is 
        reproduced with each copy. 

_EOF_
    ;
  }
  &printAllAreFree($fh) if (! $gotConditions);
} # printTableSpecificUsage

sub guessSequencingCenter {
  my ($assemblyLabel) = @_;
  my $sequencingCenter;
  my $unknown = "***PLEASE FILL IN SEQUENCING CENTER***";
  my $multiple = "***PLEASE FILL IN MULTIPLE SEQUENCING CENTERS***";
  if ($assemblyLabel =~ /Zv\d+/) {
    return 'a collaboration between the
Wellcome Trust Sanger Institute in Cambridge, UK, the Max Planck Institute
for Developmental Biology in Tuebingen, Germany, the Netherlands Institute
for Developmental Biology (Hubrecht Laboratory), Utrecht, The Netherlands
and Yi Zhou and Leonard Zon from the Children\'s Hospital in Boston,
Massachusetts.';
  }
  if (&isBaylor($assemblyLabel)) {
    $sequencingCenter =
      'the Baylor College of Medicine Human Genome Sequencing Center';
  }
  if (&isWustl($assemblyLabel)) {
    return $multiple if ($sequencingCenter);
    $sequencingCenter =
      'the Genome Sequencing Center at the Washington University School of Medicine in St. Louis';
  }
  if ($assemblyLabel =~ /Broad/) {
    return $multiple if ($sequencingCenter);
    $sequencingCenter =
      'the Broad Institute at MIT and Harvard';
  }
  if ($assemblyLabel =~ /Sanger/) {
    return $multiple if ($sequencingCenter);
    $sequencingCenter =
      'the Wellcome Trust Sanger Institute';
  }
  if ($assemblyLabel =~ /NCBI/) {
    return $multiple if ($sequencingCenter);
    $sequencingCenter =
      'the National Center for Biotechnology Information (NCBI)';
  }
  if ($assemblyLabel =~ /JGI/) {
    return $multiple if ($sequencingCenter);
    $sequencingCenter =
      'the US DOE Joint Genome Institute (JGI)';
  }
  $sequencingCenter = $unknown if (! $sequencingCenter);
  return $sequencingCenter;
} # guessSequencingCenter

sub guessConsortium {
  my ($Organism) = @_;
  my $consortium = "";
  if (scalar(grep /^$Organism/i, qw( Mouse Rat Chimp Chicken ))) {
    $consortium = "\nfrom the $Organism Genome Sequencing Consortium";
  } elsif ($Organism eq 'X. tropicalis') {
    $consortium = "\nfrom the $Organism Genome Consortium";
  }
} # guessConsortium

sub getDescriptives {
  # Return a slew of variables used to describe the assembly.
  my ($Organism, $assemblyDate, $assemblyLabel) =
    &HgAutomate::getAssemblyInfo($dbHost, $db);
  my $organism = $Organism;
  if ($organism !~ /^[A-Z]\. [a-z]+/) {
    $organism = lc($Organism);
  }
  my $consortium = &guessConsortium($Organism);
  my $sequencingCenter = &guessSequencingCenter($assemblyLabel);
  my $projectUrl = "***PLEASE INSERT PROJECT URL OR REMOVE THIS STATEMENT***";
  # WUSTL project page example: http://genome.wustl.edu/genome.cgi?GENOME=Gallus%20gallus
  # Baylor project page example: http://www.hgsc.bcm.tmc.edu/projects/honeybee/
  # Broad Institute project page example: http://www.broad.mit.edu/mammals/horse/ 
  return ($Organism, $assemblyDate, $assemblyLabel,
	  $organism, $consortium, $sequencingCenter, $projectUrl);
}

sub makeDatabaseReadme {
  # Dump out a README.txt for the database/ dir, where autodumped .sql
  # and .txt for each table will be generated on the RR (not hgwdev).
  my ($runDir) = @_;
  my ($Organism, $assemblyDate, $assemblyLabel,
      $organism, $consortium, $sequencingCenter, $projectUrl) =
	&getDescriptives();
  my $fh = &HgAutomate::mustOpen(">$runDir/README.database.txt");
  print $fh <<_EOF_
This directory contains a dump of the UCSC genome annotation database for
the $assemblyDate assembly of the $organism genome ($db, $assemblyLabel)$consortium.
The annotations were generated by UCSC and collaborators worldwide.

This assembly was produced by $sequencingCenter.
For more information on the $organism genome, see the project website:
  $projectUrl

Files included in this directory (updated nightly):

  - *.sql files:  the MySQL commands used to create the tables

  - *.txt.gz files: the database tables in a tab-delimited format
    compressed with gzip.

To see descriptions of the tables underlying Genome Browser annotation
tracks, select the table in the Table Browser:
  http://genome.ucsc.edu/cgi-bin/hgTables?db=$db
and click the "describe table schema" button.  There is also a "view
table schema" link on the configuration page for each track.

---------------------------------------------------------------
If you plan to download a large file or multiple files from this 
directory, we recommend you use ftp rather than downloading the files 
via our website. To do so, ftp to hgdownload.cse.ucsc.edu, then go to 
the directory goldenPath/$db/database/. To download multiple 
files, use the "mget" command:

    mget <filename1> <filename2> ...
    - or -
    mget -a (to download all the files in the directory) 

Alternate methods to ftp access.

Using an rsync command to download the entire directory:
    rsync -avzP rsync://hgdownload.cse.ucsc.edu/goldenPath/$db/database/ .
For a single file, e.g. gc5Base.txt.gz
    rsync -avzP \
        rsync://hgdownload.cse.ucsc.edu/goldenPath/$db/database/gc5Base.txt.gz .

Or with wget, all files:
    wget --timestamping \
        'ftp://hgdownload.cse.ucsc.edu/goldenPath/$db/database/*'
With wget, a single file: 
    wget --timestamping \
        'ftp://hgdownload.cse.ucsc.edu/goldenPath/$db/database/gc5Base.txt.gz' \
        -O gc5Base.txt.gz

To uncompress the *.txt.gz files:
    gunzip <table>.txt.gz
The tables can be loaded directly from the .txt.gz compressed file.
It is not necessary to uncompress them to load into a database,
as shown in the example below.

To load one of the tables directly into your local mirror database,
for example the table chromInfo:
## create table from the sql definition
\$ hgsql $db < chromInfo.sql
## load data from the txt.gz file
\$ zcat chromInfo.txt.gz | hgsql $db --local-infile=1 \
        -e 'LOAD DATA LOCAL INFILE "/dev/stdin" INTO TABLE chromInfo;'

_EOF_
  ;
  &printAssemblyUsage($fh, $Organism, $assemblyLabel);
  &printTableSpecificUsage($fh);
  close($fh);
} # makeDatabaseReadme

sub makeBigZipsReadme {
  # Dump out a README.txt for bigZips/ .
  my ($runDir) = @_;
  my ($Organism, $assemblyDate, $assemblyLabel,
      $organism, $consortium, $sequencingCenter, $projectUrl) =
	&getDescriptives();
  my $fh = &HgAutomate::mustOpen(">$runDir/README.bigZips.txt");
  print $fh <<_EOF_
This directory contains the $assemblyDate assembly of the $organism genome
($db, $assemblyLabel), as well as repeat annotations and GenBank sequences.

This assembly was produced by $sequencingCenter.
For more information on the $organism genome, see the project website:
  $projectUrl

Files included in this directory:

_EOF_
  ;
  if ($chromBased) {
    print $fh <<_EOF_
chromAgp.tar.gz - Description of how the assembly was generated from
    fragments, unpacking to one file per chromosome.

chromFa.tar.gz - The assembly sequence in one file per chromosome.
    Repeats from RepeatMasker and Tandem Repeats Finder (with period
    of 12 or less) are shown in lower case; non-repeating sequence is
    shown in upper case.

chromFaMasked.tar.gz - The assembly sequence in one file per chromosome.
    Repeats are masked by capital Ns; non-repeating sequence is shown in
    upper case.

chromOut.tar.gz - RepeatMasker .out files (one file per chromosome).
    RepeatMasker was run with the -s (sensitive) setting.
    *** PLEASE ADD REPEATMASKER VERSION AND LIB VERSION FROM THE DATE THAT REPEATMASKER WAS RUN (MAY BE IN MAKE.DOC)

chromTrf.tar.gz - Tandem Repeats Finder locations, filtered to keep repeats
    with period less than or equal to 12, and translated into UCSC's BED
    format (one file per chromosome).

_EOF_
    ;
  } else {
    print $fh <<_EOF_
$db.agp.gz - Description of how the assembly was generated from
    fragments.

$db.fa.gz - "Soft-masked" assembly sequence in one file.
    Repeats from RepeatMasker and Tandem Repeats Finder (with period
    of 12 or less) are shown in lower case; non-repeating sequence is
    shown in upper case.

$db.fa.masked.gz - "Hard-masked" assembly sequence in one file.
    Repeats are masked by capital Ns; non-repeating sequence is shown in
    upper case.

$db.fa.out.gz - RepeatMasker .out file.  RepeatMasker was run with the
    -s (sensitive) setting.  *** PLEASE ADD REPEATMASKER VERSION AND LIB VERSION FROM THE DATE THAT REPEATMASKER WAS RUN (MAY BE IN MAKE.DOC)

$db.trf.bed.gz - Tandem Repeats Finder locations, filtered to keep repeats
    with period less than or equal to 12, and translated into UCSC's BED
    format.

_EOF_
    ;
  }
  if (&dbHasTable($dbHost, $db, 'all_est')) {
    print $fh <<_EOF_
est.fa.gz - $Organism ESTs in GenBank. This sequence data is updated once a
    week via automatic GenBank updates.

_EOF_
    ;
  }
  print $fh <<_EOF_
md5sum.txt - checksums of files in this directory

_EOF_
    ;
  if (&dbHasTable($dbHost, $db, 'all_mrna')) {
    print $fh <<_EOF_
mrna.fa.gz - $Organism mRNA from GenBank. This sequence data is updated
    once a week via automatic GenBank updates.

_EOF_
    ;
  }
  if (&dbHasTable($dbHost, $db, 'refGene')) {
    print $fh <<_EOF_
refMrna.fa.gz - RefSeq mRNA from the same species as the genome.
    This sequence data is updated once a week via automatic GenBank
    updates.

_EOF_
    ;
  }
  my $dunno = '*** ??? ***';
  if ($geneTable) {
    my $geneDesc;
    if ($geneTable eq 'refGene') {
      $geneDesc = 'RefSeq';
    } elsif ($geneTable eq 'mgcGenes') {
      $geneDesc = 'MGC';
    } elsif ($geneTable eq 'xenoRefGene') {
      $geneDesc = 'non-$Organism RefSeq';
    } else {
      $geneDesc = $dunno;
    }
    print $fh <<_EOF_
upstream1000.fa.gz - Sequences 1000 bases upstream of annotated
    transcription starts of $geneDesc genes with annotated 5' UTRs.
_EOF_
    ;
    if ($geneDesc ne $dunno) {
      print $fh <<_EOF_
    This file is updated weekly so it might be slightly out of sync with
    the $geneDesc data which is updated daily for most assemblies.
_EOF_
      ;
    } else {
      print $fh <<_EOF_
    Note that upstream files are generated only when an assembly is
    released. Therefore, the data may be slightly out of synch with
    the RefSeq data in assemblies that are incrementally updated
    nightly.
_EOF_
      ;
    }
    print $fh <<_EOF_

upstream2000.fa.gz - Same as upstream1000, but 2000 bases.

upstream5000.fa.gz - Same as upstream1000, but 5000 bases.

_EOF_
    ;
  }
  if (&dbHasTable($dbHost, $db, 'xenoMrna')) {
    print $fh <<_EOF_
xenoMrna.fa.gz - GenBank mRNAs from species other than that of 
    the genome. This sequence data is updated once a week via automatic 
    GenBank updates.
_EOF_
    ;
  }
  print $fh <<_EOF_
------------------------------------------------------------------
If you plan to download a large file or multiple files from this
directory, we recommend that you use ftp rather than downloading the
files via our website. To do so, ftp to hgdownload.cse.ucsc.edu
[username: anonymous, password: your email address], then cd to the
directory goldenPath/$db/bigZips. To download multiple files, use
the "mget" command:

    mget <filename1> <filename2> ...
    - or -
    mget -a (to download all the files in the directory)

Alternate methods to ftp access.

Using an rsync command to download the entire directory:
    rsync -avzP rsync://hgdownload.cse.ucsc.edu/goldenPath/$db/bigZips/ .
For a single file, e.g. chromFa.tar.gz
    rsync -avzP \
        rsync://hgdownload.cse.ucsc.edu/goldenPath/$db/bigZips/chromFa.tar.gz .

Or with wget, all files:
    wget --timestamping \
        'ftp://hgdownload.cse.ucsc.edu/goldenPath/$db/bigZips/*'
With wget, a single file:
    wget --timestamping \
        'ftp://hgdownload.cse.ucsc.edu/goldenPath/$db/bigZips/chromFa.tar.gz' \
        -O chromFa.tar.gz

To unpack the *.tar.gz files:
    tar xvzf <file>.tar.gz
To uncompress the fa.gz files:
    gunzip <file>.fa.gz

_EOF_
  ;
  &printAssemblyUsage($fh, $Organism, $assemblyLabel);
  close($fh);
} # makeBigZipsReadme


sub makeChromosomesReadme {
  # Dump out a README.txt for chromsomes/ .
  my ($runDir) = @_;
  my ($Organism, $assemblyDate, $assemblyLabel,
      $organism, $consortium, $sequencingCenter, $projectUrl) =
	&getDescriptives();
  my $fh = &HgAutomate::mustOpen(">$runDir/README.chromosomes.txt");
  print $fh <<_EOF_
This directory contains the $assemblyDate assembly of the $organism genome
($db, $assemblyLabel) in one gzip-compressed FASTA file per chromosome.

This assembly was produced by $sequencingCenter.
For more information on the $organism genome, see the project website:
  $projectUrl

Files included in this directory:

  - chr*.fa.gz: compressed FASTA sequence of each chromosome.

_EOF_
  ;
  print $fh <<_EOF_
------------------------------------------------------------------
If you plan to download a large file or multiple files from this 
directory, we recommend that you use ftp rather than downloading the 
files via our website. To do so, ftp to hgdownload.cse.ucsc.edu, then 
go to the directory goldenPath/$db/chromosomes. To download multiple 
files, use the "mget" command:

    mget <filename1> <filename2> ...
    - or -
    mget -a (to download all the files in the directory)

Alternate methods to ftp access.
    
Using an rsync command to download the entire directory:
    rsync -avzP rsync://hgdownload.cse.ucsc.edu/goldenPath/$db/chromosomes/ .
For a single file, e.g. chrM.fa.gz
    rsync -avzP \
        rsync://hgdownload.cse.ucsc.edu/goldenPath/$db/chromosomes/chrM.fa.gz .
    
Or with wget, all files:
    wget --timestamping \
        'ftp://hgdownload.cse.ucsc.edu/goldenPath/$db/chromosomes/*'
With wget, a single file:
    wget --timestamping \
        'ftp://hgdownload.cse.ucsc.edu/goldenPath/$db/chromosomes/chrM.fa.gz' \
        -O chrM.fa.gz
    
To uncompress the fa.gz files:
    gunzip <file>.fa.gz

_EOF_
  ;
  &printAssemblyUsage($fh, $Organism, $assemblyLabel);
  close($fh);
} # makeChromosomesReadme


sub makeLiftOverReadme {
  # Dump out a README.txt for the liftOver/ dir, where doBlastzChainNet.pl
  # runs will deposit the .over.chain.gz files.
  my ($runDir) = @_;
  my $fh = &HgAutomate::mustOpen(">$runDir/README.liftOver.txt");
  print $fh <<_EOF_
This directory contains the data files required as input to the
liftOver utility. This tool -- which requires a Linux platform --
allows the mass conversion of coordinates from one assembly to
another. The executable file for the utility can be downloaded from
http://hgdownload.cse.ucsc.edu/admin/exe/liftOver.gz .

The file names reflect the assembly conversion data contained within
in the format <db1>To<Db2>.over.chain.gz. For example, a file named
hg15ToHg16.over.chain.gz file contains the liftOver data needed to
convert hg15 (Human Build 33) coordinates to hg16 (Human Build 34).
If no file is available for the assembly in which you're interested,
please send a request to the genome mailing list
(genome\@soe.ucsc.edu) and we will attempt to provide you with one.

To download a large file or multiple files from this directory,
we recommend that you use ftp rather than downloading the files via our
website. To do so, ftp to hgdownload.cse.ucsc.edu (user: anonymous),
then cd to goldenPath/$db/liftOver.  To download multiple files,
use the "mget" command:

    mget <filename1> <filename2> ...
    - or -
    mget -a (to download all the files in the directory)

-------------------------------------------------------
Please refer to the credits page
(http://genome.ucsc.edu/goldenPath/credits.html) for guidelines and
restrictions regarding data use for these assemblies.
-------------------------------------------------------
Alternate methods to ftp access.

Using an rsync command to download the entire directory:
    rsync -avzP rsync://hgdownload.cse.ucsc.edu/goldenPath/$db/liftOver/ .
For a single file, e.g. ${db}ToHg18.over.chain.gz
    rsync -avzP \
        rsync://hgdownload.cse.ucsc.edu/goldenPath/$db/liftOver/${db}ToHg18.over.chain.gz .
    (Hg18 is merely an example here, not necessarily existing.)

Or with wget, all files:
    wget --timestamping \
        'ftp://hgdownload.cse.ucsc.edu/goldenPath/$db/liftOver/*'
With wget, a single file:
    wget --timestamping \
        'ftp://hgdownload.cse.ucsc.edu/goldenPath/$db/liftOver/${db}ToHg18.over.chain.gz' \ 
        -O ${db}ToHg18.over.chain.gz

To uncompress the *.chain.gz files:
    gunzip <file>.chain.gz 
The liftOver utility can read the files in their .gz format,
it is not necessary to uncompress them to use with the liftOver command.

_EOF_
  ;
  close($fh);
} # makeLiftOverReadme

sub doCompress {
  # step: compress [workhorse]
  my $runDir = "$topDir/goldenPath";
  &HgAutomate::mustMkdir($runDir);

  my $whatItDoes =
"It creates compressed sequence and repeat-annotation files for download.";
  my $workhorse = &HgAutomate::chooseWorkhorse();
  my $bossScript = new HgRemoteScript("$scriptDir/doCompress.csh", $workhorse,
				      $runDir, $whatItDoes);

  $bossScript->add(<<_EOF_
rm -rf bigZips database
mkdir bigZips database
mkdir -p liftOver

_EOF_
  );
  if ($chromBased) {
    &compressChromFiles($runDir, $bossScript);
  } else {
    &compressScaffoldFiles($runDir, $bossScript);
  }
  $bossScript->add(<<_EOF_
# Add md5sum.txt and README.txt to each dir:
foreach d (bigZips $chromGz database liftOver)
  cd $runDir/\$d
  if (\$d != "database" && \$d != "liftOver") then
    md5sum *.gz > md5sum.txt
  endif
  mv $runDir/README.\$d.txt README.txt
end

_EOF_
  );

  # Create README.*.txt files which will be moved into subdirs by the script.
  &makeDatabaseReadme($runDir);
  &makeBigZipsReadme($runDir);
  &makeChromosomesReadme($runDir) if ($chromBased);
  &makeLiftOverReadme($runDir);

  $bossScript->execute();
} # doCompress


#########################################################################
# * step: install [dbHost]

sub doInstall {
  my $runDir = "$topDir/goldenPath";
  my $whatItDoes =
"It creates links from the web server's goldenPath download area to the
actual compressed files.";
  my $bossScript = new HgRemoteScript("$scriptDir/doInstall.csh", $dbHost,
				      $runDir, $whatItDoes);
  my $gp = "$HgAutomate::goldenPath/$db";
  $bossScript->add(<<_EOF_
mkdir -p $gp
foreach d (bigZips $chromGz database)
  rm -rf $gp/\$d
  mkdir $gp/\$d
  ln -s $runDir/\$d/*.{gz,txt} $gp/\$d/
end
# Don't blow away all of liftOver, just the README -- there may be
# pre-existing links that are not regenerated above.
mkdir -p $gp/liftOver
rm -f $gp/liftOver/README.txt
ln -s $runDir/liftOver/README.txt $gp/liftOver/README.txt
_EOF_
  );
  if ($geneTable) {
    $bossScript->add(<<_EOF_
cd $runDir/bigZips
foreach size (1000 2000 5000)
  echo \$size
  featureBits $db $geneTable:upstream:\$size -fa=stdout \\
  | gzip -c > upstream\$size.fa.gz
end
md5sum up*.gz >> md5sum.txt
ln -s $runDir/bigZips/up*.gz $gp/bigZips/
_EOF_
    );
  }
  $bossScript->execute();
} # doInstall


#########################################################################
# main

# Prevent "Suspended (tty input)" hanging:
&HgAutomate::closeStdin();

# Make sure we have valid options and exactly 1 argument:
&checkOptions();
&usage(1) if (scalar(@ARGV) != 1);
($db) = @ARGV;

$topDir = "$HgAutomate::clusterData/$db";
$scriptDir = "$topDir/jkStuff";
$trfRunDirRel = "$HgAutomate::trackBuild/simpleRepeat";
$trfRunDir = "$topDir/$trfRunDirRel";
$geneTable = &getGeneTable();

if (! -e "$topDir/$db.2bit") {
  die "Sorry, this script requires $topDir/$db.2bit.\n";
}
if (! -e "$topDir/chrom.sizes") {
  die "Sorry, this script requires $topDir/chrom.sizes.\n";
}
@chroms = split("\n", `awk '{print \$1;}' $topDir/chrom.sizes`);
$chromBased = (scalar(@chroms) <= $HgAutomate::splitThreshold);
if ($chromBased) {
  foreach my $chr (@chroms) {
    my $chrRoot = $chr;
    $chrRoot =~ s/^chr//;
    $chrRoot =~ s/_random$//;
    $chrRoot =~ s/_\w+_hap\d+//;
    push @{$chromRoots{$chrRoot}}, $chr;
  }
  $chromGz = "chromosomes";
} else {
  $chromGz = "";
}

# Do everything.
$stepper->execute();

# Tell the user anything they should know.
my $stopStep = $stepper->getStopStep();
my $upThrough = ($stopStep eq 'install') ? "" :
  "  (through the '$stopStep' step)";

&HgAutomate::verbose(1, <<_EOF_

 *** All done!$upThrough
_EOF_
);
if ($stopStep eq 'install') {
&HgAutomate::verbose(1, <<_EOF_
 *** Please take a look at the downloads for $db using a web browser.
 *** The downloads url is: http://hgwdev.cse.ucsc.edu/goldenPath/$db. 
 *** Edit each README.txt to resolve any notes marked with "***":
     $topDir/goldenPath/database/README.txt
     $topDir/goldenPath/bigZips/README.txt
_EOF_
  );
  if ($chromBased) {
    &HgAutomate::verbose(1, <<_EOF_
     $topDir/goldenPath/$chromGz/README.txt
_EOF_
    );
  }
  # liftOver/README.txt doesn't require any editing.
  &HgAutomate::verbose(1, <<_EOF_
     (The htdocs/goldenPath/$db/*/README.txt "files" are just links to those.)
 *** If you have to make any edits that would always apply to future
     assemblies from the same sequencing center, please edit them into
     ~/kent/src/hg/utils/automation/$base (or ask Angie for help).

_EOF_
  );
}

