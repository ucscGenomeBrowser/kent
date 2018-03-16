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

my %tableCheckList = ( 'all_est' => 1,
'all_mrna' => 1,
'augustusGene' => 1,
'chainHg38' => 1,
'chainHg38Link' => 1,
'chainMm10' => 1,
'chainMm10Link' => 1,
'chainRBestHg38' => 1,
'chainRBestHg38Link' => 1,
'chainRBestMm10' => 1,
'chainRBestMm10Link' => 1,
'chainSynHg38' => 1,
'chainSynHg38Link' => 1,
'chainSynMm10' => 1,
'chainSynMm10Link' => 1,
'chromAlias' => 1,
'chromInfo' => 1,
'cpgIslandExt' => 1,
'cpgIslandExtUnmasked' => 1,
'cytoBandIdeo' => 1,
'estOrientInfo' => 1,
'gap' => 1,
'gapOverlap' => 1,
'gbStatus' => 1,
'gc5BaseBw' => 1,
'genscan' => 1,
'genscanSubopt' => 1,
'gold' => 1,
'grp' => 1,
'hgFindSpec' => 1,
'history' => 1,
'intronEst' => 1,
'microsat' => 1,
'mrnaOrientInfo' => 1,
'nestedRepeats' => 1,
'netHg38' => 1,
'netMm10' => 1,
'netRBestHg38' => 1,
'netRBestMm10' => 1,
'netSynHg38' => 1,
'netSynMm10' => 1,
'rmsk' => 1,
'simpleRepeat' => 1,
'tableDescriptions' => 1,
'tandemDups' => 1,
'trackDb' => 1,
'ucscToINSDC' => 1,
'ucscToRefSeq' => 1,
'windowmaskerSdust' => 1,
'xenoRefFlat' => 1,
'xenoRefGene' => 1,
'xenoRefSeqAli' => 1,
'gbLoaded' => 1,         ## genbank tables following
'all_mrna' => 1,   ## from /cluster/data/genbank/etc/gbPerAssemblyTables.txt
'xenoMrna' => 1,
'mrnaOrientInfo' => 1,
'all_est' => 1,
'intronEst' => 1,
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
);

my $db = shift;

my $tableCount = 0;
my %tableList;	# key is table name, value is 1
open (FH, "hgsql -N -e 'show tables;' $db|") or die "can not hgsql -N -e 'show tables $db'";
while (my $table = <FH>) {
   chomp $table;
   $tableList{$table} = 1;
   ++$tableCount;
}
close (FH);

printf STDERR "# %d tables in database %s\n", $tableCount, $db;

my %extraTables;
my $extraTableCount = 0;
my $tablesFound = 0;

foreach my $table (sort keys %tableList) {
  if (defined($tableCheckList{$table})) {
    ++$tablesFound;
  } else {
    if (! ($table =~ m/trackDb_/ || $table =~ m/hgFindSpec_/) ) {
      $extraTables{$table} = 1;
      ++$extraTableCount;
    }
  }
}

printf STDERR "# verified %d tables, %d extra tables\n", $tablesFound, $extraTableCount;

my $shownTables = 0;
foreach my $table (sort keys %extraTables) {
  ++$shownTables;
  if ($extraTableCount > 10) {
    if ( ($shownTables < 5) || ($shownTables > ($extraTableCount - 4)) ) {
       printf STDERR "# %d\t%s\n", $shownTables, $table;
    } elsif ($shownTables == 5) {
       printf STDERR "# . . . etc . . .\n";
    }
  } else {
    printf STDERR "# %d\t%s\n", $shownTables, $table;
  }
}

my %missingTables;
my $missingTableCount = 0;
$tablesFound = 0;

foreach my $table (sort keys %tableCheckList) {
  if (defined($tableList{$table})) {
    ++$tablesFound;
  } else {
    if (! ($table =~ m/^ccds|^mgc/) ) {
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

printf STDERR "# verified %d tables, %d missing tables\n", $tablesFound, $missingTableCount;

my $missedOut = 0;
foreach my $table (sort keys %missingTables) {
  ++$missedOut;
  printf STDERR "# %d\t%s\n", $missedOut, $table;
}
