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

my %optionalCheckList = ( 'extNcbiRefSeq' => 1,
'ncbiRefSeq' => 1,
'ncbiRefSeqCds' => 1,
'ncbiRefSeqCurated' => 1,
'ncbiRefSeqLink' => 1,
'ncbiRefSeqOther' => 1,
'ncbiRefSeqPepTable' => 1,
'ncbiRefSeqPredicted' => 1,
'ncbiRefSeqPsl' => 1,
'seqNcbiRefSeq' => 1
);

my %tableCheckList = ( 'augustusGene' => 1,
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
'gap' => 1,
'gapOverlap' => 1,
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
  if ($lineCount > 3) {
    return 1;
  } else {
    return 0;
  }
}

#############################################################################
## main() starts here

my $db = shift;
my $Db = ucfirst($db);

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

foreach my $table (sort keys %tableList) {
  if (defined($tableCheckList{$table}) || defined($gbCheckList{$table}) || defined($optionalCheckList{$table}) ) {
    ++$tablesFound;
    $optionalCount += 1 if (defined($optionalCheckList{$table}));
  } else {
    $extraTables{$table} = 1;
    ++$extraTableCount;
  }
}

printf STDERR "# verified %d tables, %d extra tables, %d optional tables\n", $tablesFound, $extraTableCount, $optionalCount;

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

printf STDERR "# verified %d tables, %d missing tables\n", $tablesFound, $missingTableCount;

my $missedOut = 0;
foreach my $table (sort keys %missingTables) {
  ++$missedOut;
  printf STDERR "# %d\t%s\n", $missedOut, $table;
}

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
      printf STDERR "# missing $otherDbs[$j].$chainTable\n" if (! checkTableExists($otherDbs[$j], $chainTable));
      printf STDERR "# missing $otherDbs[$j].$chainLinkTable\n" if (! checkTableExists($otherDbs[$j], $chainLinkTable));
      printf STDERR "# missing $otherDbs[$j].$netTable\n" if (! checkTableExists($otherDbs[$j], $netTable));
   }
}

__END__
printf STDERR "# missing hg38.$chainTable\n" if (! checkTableExists("hg38", $chainTable));
printf STDERR "# missing hg38.$chainLinkTable\n" if (! checkTableExists("hg38", $chainLinkTable));
printf STDERR "# missing hg38.$netTable\n" if (! checkTableExists("hg38", $netTable));
printf STDERR "# missing mm10.$chainTable\n" if (! checkTableExists("mm10", $chainTable));
printf STDERR "# missing mm10.$chainLinkTable\n" if (! checkTableExists("mm10", $chainLinkTable));
printf STDERR "# missing mm10.$netTable\n" if (! checkTableExists("mm10", $netTable));

chainRBestThaSir1
chainRBestThaSir1Link
chainSynThaSir1
chainSynThaSir1Link
netRBestThaSir1
netSynThaSir1

