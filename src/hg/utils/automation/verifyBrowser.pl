#!/usr/bin/env perl

use strict;
use warnings;

my $argc = scalar(@ARGV);

if ($argc != 1) {
  printf STDERR "usage: verifyBrowser.pl <db>\n";
  printf STDERR "\twill check the <db> for the set of tables required\n";
  printf STDERR "\tfor a completed genome browser build.\n";
  exit 255;
}

my %optionalCheckList = ( 'ensGene' => "Ensembl genes",
'ensGtp' => "Ensembl genes",
'ensPep' => "Ensembl genes",
'ensemblSource' => "Ensembl genes",
'ensemblToGeneName' => "Ensembl genes",
'extNcbiRefSeq' => "NCBI RefSeq genes",
'ncbiRefSeq' => "NCBI RefSeq genes",
'ncbiRefSeqCds' => "NCBI RefSeq genes",
'ncbiRefSeqCurated' => "NCBI RefSeq genes",
'ncbiRefSeqLink' => "NCBI RefSeq genes",
'ncbiRefSeqOther' => "NCBI RefSeq genes",
'ncbiRefSeqPepTable' => "NCBI RefSeq genes",
'ncbiRefSeqPredicted' => "NCBI RefSeq genes",
'ncbiRefSeqPsl' => "NCBI RefSeq genes",
'seqNcbiRefSeq' => "NCBI RefSeq genes",
'chainRBestHg38' => "chainNetRBestHg38",
'chainRBestHg38Link' => "chainNetRBestHg38",
'chainRBestMm10' => "chainNetRBestMm10",
'chainRBestMm10Link' => "chainNetRBestMm10",
'chainSynHg38' => "chainNetSynHg38",
'chainSynHg38Link' => "chainNetSynHg38",
'chainSynMm10' => "chainNetSynMm10",
'chainSynMm10Link' => "chainNetSynMm10",
'netRBestHg38' => "chainNetRBestHg38",
'netRBestMm10' => "chainNetRBestMm10",
'netSynHg38' => "chainNetSynHg38",
'netSynMm10' => "chainNetSynMm10",
'tandemDups' => "tandemDups",
'gapOverlap' => "gapOverlap"
);

my %tableCheckList = ( 'augustusGene' => 1,
'chainHg38' => 1,
'chainHg38Link' => 1,
'chainMm10' => 1,
'chainMm10Link' => 1,
'chromAlias' => 1,
'chromInfo' => 1,
'cpgIslandExt' => 1,
'cpgIslandExtUnmasked' => 1,
'cytoBandIdeo' => 1,
'gap' => 1,
'gc5BaseBw' => 1,
'genscan' => 1,
'genscanSubopt' => 1,
'gold' => 1,
'grp' => 1,
'hgFindSpec' => 1,
'history' => 1,
'microsat' => 1,
'nestedRepeats' => 1,
'netHg38' => 1,
'netMm10' => 1,
'rmsk' => 1,
'simpleRepeat' => 1,
'tableDescriptions' => 1,
'trackDb' => 1,
'ucscToINSDC' => 1,
'ucscToRefSeq' => 1,
'windowmaskerSdust' => 1
);

## from /cluster/data/genbank/etc/gbPerAssemblyTables.txt
## genbank tables
## some of these should be present, do not need to be all
my %gbCheckList = ( 'gbLoaded' => 1,
'all_mrna' => 1,
'xenoMrna' => 1,
'mrnaOrientInfo' => 1,
'all_est' => 1,
'intronEst' => 1,
'gbStatus' => 1,
'xenoEst' => 1,
'estOrientInfo' => 1,
'refGene' => 1,
'refSeqAli' => 1,
'refFlat' => 1,
'xenoRefGene' => 1,
'xenoRefSeqAli' => 1,
'xenoRefFlat' => 1,
'mgcFullStatus' => 1,
'mgcStatus' => 1,
'mgcFullMrna' => 1,
'mgcGenes' => 1,
'mgcFailedEst' => 1,
'mgcIncompleteMrna' => 1,
'mgcPickedEst' => 1,
'mgcUnpickedEst' => 1,
'orfeomeMrna' => 1,
'orfeomeGenes' => 1,
'ccdsGene' => 1,
'ccdsInfo' => 1,
'ccdsNotes' => 1,
'ccdsKgMap' => 1
);	# my %gbCheckList

sub checkTableExists($$) {
  my ($db, $table) = @_;
  my $lineCount = `hgsql -N -e 'desc $table;' $db 2> /dev/null | wc -l`;
  chomp $lineCount;
  if ($lineCount > 0) {
    return 1;
  } else {
    return 0;
  }
}

#############################################################################
## main() starts here

my $db = shift;
my $Db = ucfirst($db);
my $dbVersion = $db;
$dbVersion =~ s/^[a-z]+//i;
my $dbPrefix = $db;
$dbPrefix =~ s/[0-9]+$//;

my $dbDbNames = `hgsql -N -e 'select organism,scientificName from dbDb where name="$db";' hgcentraltest`;
chomp $dbDbNames;
$dbDbNames =~ s/\t/, /;

my $tableCount = 0;
my %tableList;	# key is table name, value is 1
open (FH, "hgsql -N -e 'show tables;' $db|") or die "can not hgsql -N -e 'show tables $db'";
while (my $table = <FH>) {
   chomp $table;
   if ($table !~ m/trackDb_|hgFindSpec_/ ) {
     $tableList{$table} = 1;
     ++$tableCount;
   }
}
close (FH);

printf STDERR "# %d tables in database %s - %s\n", $tableCount, $db, $dbDbNames;

my %extraTables;
my $extraTableCount = 0;
my $tablesFound = 0;
my $optionalCount = 0;
my %optionsFound;	# key is category, value is count of tables

foreach my $table (sort keys %tableList) {
  if (defined($tableCheckList{$table}) || defined($gbCheckList{$table}) || defined($optionalCheckList{$table}) ) {
    ++$tablesFound;
    if (defined($optionalCheckList{$table})) {
       $optionalCount += 1;
       $optionsFound{$optionalCheckList{$table}} += 1;
    }
  } else {
    $extraTables{$table} = 1;
    ++$extraTableCount;
  }
}

printf STDERR "# verified %d tables in database $db, %d extra tables, %d optional tables\n", $tablesFound, $extraTableCount, $optionalCount;
if ($optionalCount > 0) {
   foreach my $category (sort keys %optionsFound) {
     printf "# %s\t%d optional tables\n", $category, $optionsFound{$category};
   }
}

my $shownTables = 0;
foreach my $table (sort keys %extraTables) {
  ++$shownTables;
  if ($extraTableCount > 10) {
    if ( ($shownTables < 5) || ($shownTables > ($extraTableCount - 4)) ) {
       printf STDERR "# %d\t%s\t- extra table\n", $shownTables, $table;
    } elsif ($shownTables == 5) {
       printf STDERR "# . . . etc . . .\n";
    }
  } else {
    printf STDERR "# %d\t%s\t- extra table\n", $shownTables, $table;
  }
}

my $gbTableCount = 0;
foreach my $table (sort keys %tableList) {
  $gbTableCount += 1 if (defined($gbCheckList{$table}));
}

if ($gbTableCount < 1) {
  printf STDERR "# ERROR: no genbank tables found\n";
} else {
  printf STDERR "# %d genbank tables found\n", $gbTableCount;
}

my %missingTables;
my $missingTableCount = 0;
$tablesFound = 0;

my $chainSelf = "chain.*" . $Db . "*";
my $netSelf = "net.*" . $Db . "*";

foreach my $table (sort keys %tableCheckList) {
  if (defined($tableList{$table})) {
    ++$tablesFound;
  } else {
    next if ($table =~ m/$chainSelf|$netSelf/);
    if ($table !~ m/^ccds|^mgc/) {
      $missingTables{$table} = 1;
      ++$missingTableCount;
    } elsif ( ($table =~ m/^ccds/) && ($db =~ m/^hg|^mm/) ) {
      $missingTables{$table} = 1;
      ++$missingTableCount;
    } elsif ( ($table =~ m/^mgc/) &&
                 ($db =~ m/^bosTau|^danRer|^hg|^mm|^rn|^xenTro/) ) {
      $missingTables{$table} = 1;
      ++$missingTableCount;
    }
  }
}

printf STDERR "# verified %d required tables, %d missing tables\n", $tablesFound, $missingTableCount;

my $missedOut = 0;
foreach my $table (sort keys %missingTables) {
  ++$missedOut;
  printf STDERR "# %d\t%s\t- missing table\n", $missedOut, $table;
}

my %optionalChainNet;
my %expectedChainNet;
my @chainTypes = ("", "RBest", "Syn");
my @otherDbs = ("hg38", "mm10");
for (my $i = 0; $i < scalar(@chainTypes); ++$i) {
   my $chainTable = "chain" . $chainTypes[$i] .  $Db;
   my $chainLinkTable = "chain" . $chainTypes[$i] .  $Db . "Link";
   my $netTable = "net" . $chainTypes[$i] . $Db;
   for (my $j = 0; $j < scalar(@otherDbs); ++$j) {
      next if ($db eq $otherDbs[$j]);
      # mm10 Syntenics do not exist (yet)
      next if ($otherDbs[$j] eq "mm10" && $chainTypes[$i] eq "Syn");
      if (length($chainTypes[$i]) > 0) { # RBest and Syn are optional
         $optionalChainNet{$otherDbs[$j]} += 1 if (checkTableExists($otherDbs[$j], $chainTable));
         $optionalChainNet{$otherDbs[$j]} += 1 if (checkTableExists($otherDbs[$j], $chainLinkTable));
         $optionalChainNet{$otherDbs[$j]} += 1 if (checkTableExists($otherDbs[$j], $netTable));
      } else {
      printf STDERR "# missing $otherDbs[$j].$chainTable\n" if (! checkTableExists($otherDbs[$j], $chainTable));
      printf STDERR "# missing $otherDbs[$j].$chainLinkTable\n" if (! checkTableExists($otherDbs[$j], $chainLinkTable));
      printf STDERR "# missing $otherDbs[$j].$netTable\n" if (! checkTableExists($otherDbs[$j], $netTable));
      $expectedChainNet{$otherDbs[$j]} += 1 if (checkTableExists($otherDbs[$j], $chainTable));
      $expectedChainNet{$otherDbs[$j]} += 1 if (checkTableExists($otherDbs[$j], $chainLinkTable));
      $expectedChainNet{$otherDbs[$j]} += 1 if (checkTableExists($otherDbs[$j], $netTable));
      }
   }
}

foreach my $expected (sort keys %expectedChainNet) {
  printf STDERR "# %s chainNet to %s found %d required tables\n", $expected, $db, $expectedChainNet{$expected};
}
foreach my $optional (sort keys %optionalChainNet) {
  printf STDERR "# %s chainNet RBest and syntenic to %s found %d optional tables\n", $optional, $db, $optionalChainNet{$optional};
}

if ($dbVersion > 1) {
  my $toOthers = `hgsql -N -e 'select fromDb,toDb from liftOverChain where fromDb = "$db" AND toDb like "${dbPrefix}%";' hgcentraltest | wc -l`;
  chomp $toOthers;
  my $fromOthers = `hgsql -N -e 'select fromDb,toDb from liftOverChain where fromDb like "${dbPrefix}%" AND toDb = "${db}";' hgcentraltest | wc -l`;
  chomp $fromOthers;
  if (($toOthers > 0) && ($fromOthers > 0)) {
    printf STDERR "# liftOver to previous versions: $toOthers, from previous versions: $fromOthers\n";
  } else {
    printf STDERR "# ERROR: liftOvers to/from previous versions not complete\n";
  }
}

my $blatServers=`hgsql -N -e 'select * from blatServers where db="$db";' hgcentraltest | wc -l`;
if ($blatServers != 2) {
  printf STDERR "# ERROR: blat server not found in hgcentraltest.blatServers ?\n";
}

my $chainNet = 0;
if ( $db ne "hg38" ) {
  $chainNet = `hgsql -e 'select * from trackDb;' hg38 | egrep "^chain$Db|^net$Db" | wc -l`;
  chomp $chainNet;
  if ($chainNet != 2) {
   printf STDERR "# ERROR: missing hg38.chainNet trackDb definitions for $db (found: $chainNet instead of 2)\n";
  }
}

if ( $db ne "mm10" ) {
  $chainNet = `hgsql -e 'select * from trackDb;' mm10 | egrep "^chain$Db|^net$Db" | wc -l`;
  chomp $chainNet;
  if ($chainNet != 2) {
   printf STDERR "# ERROR: missing mm10.chainNet trackDb definitions for $db (found: $chainNet instead of 2)\n";
  }
}

my $goldSearch = `hgsql -N -e 'select termRegex from hgFindSpec where searchTable="gold" AND searchName="gold";' $db | grep "abuz" | wc -l`;
chomp $goldSearch;

if ($goldSearch == 1) {
  printf STDERR "# ERROR: missing specific hgFindSpec rule for gold table\n";
}

my $allJoiner = `grep $db /usr/local/apache/cgi-bin/all.joiner | head -1 | wc -l`;
chomp $allJoiner;
if ($allJoiner != 1) {
  printf STDERR "# ERROR missing definitions in hg/makeDb/schema/all.joiner\n";
}
