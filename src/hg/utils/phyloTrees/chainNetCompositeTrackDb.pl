#!/usr/bin/env perl

use strict;
use warnings;

my $argc = scalar(@ARGV);

if ($argc < 4) {
 printf STDERR "usage: compositeChainNet.pl [net|syn|rbest] <name> <db>  <clade1.list> \\\n\t[clade2.list ...etc...] > nameChainNet.ra
[net|syn|rbest] select one of these for lowest level chainNet\n\tor syntenic or reciprocal best chainNet
name is the name of the composite track, examples:
\tplacental, mammal, vertebrate
db is the name of the database to construct the composite for
The clade lists are lists of species dbs to put
together into a view.  Additional clade lists for more views.
The short and long lables will need attention in the result.
And the default on/off visibilities.\n";
   exit 255;
}

my $netType = shift;
my $trackName = shift;
my $thisDb = shift;
printf STDERR "# net type: '%s'\n", $netType;
printf STDERR "# track name: '%s'\n", $trackName;
printf STDERR "# thisDb '%s'\n", $thisDb;
my $dbPrefix = $thisDb;
$dbPrefix =~ s/[0-9]+$//;
my @cladeLists;
my @cladeNames;
my %dbList;	# key is cladeName, value is array of db names
my %commonNames;	# key is db, value is common name
my %speciesOrder;	# key is db, value is sNNN to get species order
my %cladeOrder;		# key is clade name, value is cNNN to get clade order
my %dbClade;		# key is db, value is clade
my %rrActive;		# key is db, value is 1 for active on RR
my %chainNetHg;		# chainNet tracks on hg38, hg19, key is db, value is 1

my @rrDbList;

open (FH, "HGDB_CONF=$ENV{'HOME'}/.hg.mysqlrr.conf hgsql -N -e 'show databases;' hg19|") or die "can not hgsql show databases hg19";
while (my $db = <FH>) {
  chomp $db;
  if ($db =~ m/^$dbPrefix/) {
    push @rrDbList, $db;
  }
}
close (FH);

my %existingTableList;	# key is table name, value is 1
my $thisTableCount = 0;

printf STDERR "# show tables on db '%s'\n", $thisDb;
open (FH, "HGDB_CONF=$ENV{'HOME'}/.hg.conf hgsql -N -e 'show tables;' $thisDb|") or die "can not hgsql show tables $thisDb";
while (my $table = <FH>) {
  chomp $table;
  $existingTableList{$table} = 1;
  ++$thisTableCount;
#  printf STDERR "# %s\n", $table;
}
close (FH);

printf STDERR "# database %s table count: %s\n", $thisDb, $thisTableCount;

foreach my $db (@rrDbList) {
  printf STDERR "# reading tables from %s\n", $db;
  open (FH, "HGDB_CONF=$ENV{'HOME'}/.hg.mysqlrr.conf hgsql -e 'show tables;' $db | grep -i chain | grep Link | egrep -i -v 'self|patch' | sed -e 's/chain//; s/Link//;'|") or die "can not hgsql show tables $db";
  while (my $line = <FH>) {
    chomp $line;
  #  printf STDERR "# %s\n", lcfirst($line);
    $chainNetHg{lcfirst($line)} = 1;
  }
  close (FH);
}

# foreach my $db (sort keys %chainNetHg) {
#   printf STDERR "# %s\n", $db;
# }
# exit 255;

open (FH, "hgsql -N -hgenome-centdb -e 'select name from dbDb where active=1' hgcentral | sort|") or die "can not hgsql select name from hgcentral.dbDb";
while (my $line = <FH>) {
  chomp $line;
  $rrActive{$line} = 1;
}
close (FH);

for (my $i = 0; $i < scalar(@ARGV); ++$i) {
    push @cladeLists, $ARGV[$i];
}

for (my $i = 0; $i < scalar(@cladeLists); ++$i) {
    my $cladeName = $cladeLists[$i];
    $cladeName =~ s/\.li.*//;
    push @cladeNames, $cladeName;
    if (!exists($dbList{$cladeName})) {
       my @a;
       $dbList{$cladeName} = \@a;
    }
    my $listPtr = $dbList{$cladeName};
    open (FH, "<$cladeLists[$i]") or die "can not read $cladeLists[$i]";
    while (my $db = <FH>) {
      chomp $db;
      push @$listPtr, $db;
    }
    close (FH);
}

my $subGroup2 = "species Species";
my $subGroup3 = "clade Clade";
my $speciesCount = 0;
my $cladeCount = 0;
foreach my $clade (@cladeNames) {
   printf STDERR "# %s\n", $clade;
   $subGroup3 .= sprintf(" c%02d=%s", $cladeCount, $clade);
   $cladeOrder{$clade} = sprintf("c%02d", $cladeCount);
   ++$cladeCount;
   my $listPtr = $dbList{$clade};
   foreach my $db (@$listPtr) {
     my $commonName = `hgsql -N -e 'select organism from dbDb where name=\"$db\";' hgcentraltest`;
     chomp $commonName;
     $commonName =~ s/ /_/g;
     $commonName = $db if (length($commonName) < 1);
     printf STDERR "# %s\t%s\ts%03d=%s\n", $clade, $db, $speciesCount, $commonName;
     $speciesOrder{$db} = sprintf("s%03d", $speciesCount);
     $subGroup2 .= sprintf(" s%03d=%s", $speciesCount, $commonName);
     $dbClade{$db} = $clade;
     ++$speciesCount;
   }
}

printf STDERR "# %s\n", $subGroup2;
printf STDERR "# %s\n", $subGroup3;

my $trackType = "";
$trackType = "Syn" if ($netType =~ m/syn/);
$trackType = "RBest" if ($netType =~ m/rbest/);
my $trackLabel = "";
$trackLabel = "Syntenic" if ($netType =~ m/syn/);
$trackLabel = "RecipBest" if ($netType =~ m/rbest/);
my $shortLabel = "";
$shortLabel = "sy" if ($netType =~ m/syn/);
$shortLabel = "rb" if ($netType =~ m/rbest/);

printf 'track %s%sChainNet
compositeTrack on
shortLabel %s %s Chain/Net
longLabel %s %s Chain and Net Alignments
subGroup1 view Views chain=Chains net=Nets
', $trackName, $trackType, ucfirst($trackName), $trackLabel, ucfirst($trackName), $trackLabel;

printf "subGroup2 %s\nsubGroup3 %s\n", $subGroup2, $subGroup3;

printf 'dragAndDrop subTracks
visibility hide
group compGeno
noInherit on
color 0,0,0
altColor 255,255,0
type bed 3
chainLinearGap loose
chainMinScore 5000
dimensions dimensionX=clade dimensionY=species
sortOrder species=+ view=+ clade=+
configurable on
html %s%sChainNet

', $trackName, $trackType;



printf '    track %s%sChainNetViewchain
    shortLabel %s Chains
    view chain
    visibility pack
    subTrack %s%sChainNet
    spectrum on

', $trackName, $trackType, $trackLabel, $trackName, $trackType;

for (my $i = 0; $i < scalar(@cladeNames); ++$i) {
  my $clade = $cladeNames[$i];
  my $dbs = $dbList{$clade};
  for (my $j = 0; $j < scalar(@$dbs); ++$j) {
      my $db = $dbs->[$j];
      my $Db = ucfirst($db);
      # for track on dev, use o_db
      my $organism = "\$o_db";
      # if track on RR, use o_Organism
      if (exists($chainNetHg{$db})) {
         $organism = "\$o_Organism";
      }
      my $tableName = sprintf("chain%s%s", $trackType, $Db);
      if (defined($existingTableList{$tableName})) {
        printf '        track chain%s%s
        subTrack %s%sChainNetViewchain off
        subGroups view=chain species=%s clade=%s
        shortLabel %s %sChain
        longLabel %s ($o_date) Chained Alignments
        type chain %s
        otherDb %s

', $trackType, $Db, $trackName, $trackType, $speciesOrder{$db}, $cladeOrder{$clade}, $organism, $shortLabel, $organism, $db, $db;
      }
  }
}

    printf '    track %s%sChainNetViewnet
    shortLabel %s Nets
    view net
    visibility dense
    subTrack %s%sChainNet

', $trackName, $trackType, $trackLabel, $trackName, $trackType;


for (my $i = 0; $i < scalar(@cladeNames); ++$i) {
  my $clade = $cladeNames[$i];
  my $dbs = $dbList{$clade};
  for (my $j = 0; $j < scalar(@$dbs); ++$j) {
      my $db = $dbs->[$j];
      my $Db = ucfirst($db);
      # for track on dev, use o_db
      my $organism = "\$o_db";
      # if track on RR, use o_Organism
      if (exists($chainNetHg{$db})) {
         $organism = "\$o_Organism";
      }
      my $tableName = sprintf("net%s%s", $trackType, $Db);
      if (defined($existingTableList{$tableName})) {
        printf '        track net%s%s
        subTrack %s%sChainNetViewnet off
        subGroups view=net species=%s clade=%s
        shortLabel %s %sNet
        longLabel %s ($o_date) Alignment Net
        type netAlign %s chain%s%s
        otherDb %s

', $trackType, $Db, $trackName, $trackType, $speciesOrder{$db}, $cladeOrder{$clade}, $organism, $shortLabel, $organism, $db, $trackType, $Db, $db;
      }
  }
}

__END__
        track netMacEug1
        subTrack vertebrateChainNetViewnet off
        subGroups view=net species=aWallaby clade=aMammal
        shortLabel $o_Organism Net
        longLabel $o_Organism ($o_date) Alignment Net
        type netAlign macEug1 chainMacEug1
        otherDb macEug1


