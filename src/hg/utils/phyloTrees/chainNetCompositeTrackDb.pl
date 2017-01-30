#!/usr/bin/env perl

use strict;
use warnings;

my $argc = scalar(@ARGV);

if ($argc < 2) {
 printf STDERR "usage: compositeChainNet.pl <name> <clade1.list> \\\n\t[clade2.list ...etc...] > nameChainNet.ra\n";
 printf STDERR "name is the name of the composite track, examples:\n";
 printf STDERR "\tplacental, mammal, vertebrate\n";
 printf STDERR "The clade lists are lists of species dbs to put\n";
 printf STDERR "together into a view.  Additional clade lists for more views.\n";
 printf STDERR "The short and long lables will need attention in the result.\n";
 printf STDERR "And the default on/off visibilities.\n";
   exit 255;
}

my $trackName = shift;
my @cladeLists;
my @cladeNames;
my %dbList;	# key is cladeName, value is array of db names
my %commonNames;	# key is db, value is common name
my %speciesOrder;	# key is db, value is sNNN to get species order
my %cladeOrder;		# key is clade name, value is cNNN to get clade order
my %dbClade;		# key is db, value is clade

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
     printf STDERR "# %s\t%s\ts%03d=%s\n", $clade, $db, $speciesCount, $commonName;
     $speciesOrder{$db} = sprintf("s%03d", $speciesCount);
     $subGroup2 .= sprintf(" s%03d=%s", $speciesCount, $commonName);
     $dbClade{$db} = $clade;
     ++$speciesCount;
   }
}

printf STDERR "# %s\n", $subGroup2;
printf STDERR "# %s\n", $subGroup3;

printf 'track %sChainNet
compositeTrack on
shortLabel %s Chain/NetChainNet
longLabel %sChainNet, Chain and Net Alignments
subGroup1 view Views chain=Chains net=Nets
', $trackName, $trackName, $trackName;

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
sortOrder  species=+ view=+ clade=+
configurable on
html %sChainNet

', $trackName;



printf '    track %sChainNetViewchain
    shortLabel Chains
    view chain
    visibility pack
    subTrack %sChainNet
    spectrum on

', $trackName, $trackName;

for (my $i = 0; $i < scalar(@cladeNames); ++$i) {
  my $clade = $cladeNames[$i];
  my $dbs = $dbList{$clade};
  for (my $j = 0; $j < scalar(@$dbs); ++$j) {
      my $db = $dbs->[$j];
      my $Db = ucfirst($db);
      printf '        track chain%s
        subTrack %sChainNetViewchain off
        subGroups view=chain species=%s clade=%s
        shortLabel $o_Organism Chain
        longLabel $o_Organism ($o_date) Chained Alignments
        type chain %s
        otherDb %s

', $Db, $trackName, $speciesOrder{$db}, $cladeOrder{$clade}, $db, $db;
  }
}

    printf '    track %sChainNetViewnet
    shortLabel Nets
    view net
    visibility dense
    subTrack %sChainNet

', $trackName, $trackName;


for (my $i = 0; $i < scalar(@cladeNames); ++$i) {
  my $clade = $cladeNames[$i];
  my $dbs = $dbList{$clade};
  for (my $j = 0; $j < scalar(@$dbs); ++$j) {
      my $db = $dbs->[$j];
      my $Db = ucfirst($db);
      printf '        track net%s
        subTrack %sChainNetViewnet off
        subGroups view=net species=%s clade=%s
        shortLabel $o_Organism Net
        longLabel $o_Organism ($o_date) Alignment Net
        type netAlign %s chain%s
        otherDb %s

', $Db, $trackName, $speciesOrder{$db}, $cladeOrder{$clade}, $db, $Db, $db;
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


