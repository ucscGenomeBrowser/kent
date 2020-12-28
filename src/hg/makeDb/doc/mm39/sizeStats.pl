#!/usr/bin/env perl

use strict;
use warnings;

my $targetDb = "mm39";
my %dbToName;  # key is name from 35way.distances.txt, value is sciName

open (FH , "<sequenceName.sciName.tab") or die "can not read sequenceName.sciName.tab";
while (my $line = <FH>) {
  chomp $line;
  my ($db, $name) = split('\t',$line);
  $name =~ s/__/. /;
  $dbToName{$db} = $name;
}
close (FH);

open (FH, "<35way.distances.txt") or
        die "can not read 35way.distances.txt";

printf "# count phylo    chain      synNet    rBest  synNet v.  query\n";
printf "#       dist      link                         chain\n";

my $count = 0;
while (my $line = <FH>) {
    chomp $line;
    my ($D, $dist) = split('\s+', $line);
    my $Db = ucfirst($D);
    my $chain = "chain" . ucfirst($D);
    if ($D =~ m/^GC/) {
      $chain = "chain." . ucfirst($D);
    }
    my $B="/hive/data/genomes/${targetDb}/bed/lastz.$D/fb.${targetDb}." .
        $chain . "Link.txt";
#    my $accession=`grep -w $D taxId.GC.sequenceName.accAsmId.sciName.name.tab 2> /dev/null  | cut -f4 | awk -F'_' '{printf "%s_%s", \$1, \$2}'`;
#    if ( $D !~ m/^[a-z]/) {
#        $B=`ls /hive/data/genomes/${targetDb}/bed/awsMultiz/lastzRun/lastz_$accession/fb.${targetDb}.chain.${accession}* 2> /dev/null`;
#        chomp $B;
#    }
# printf STDERR "# chainLink: %s\n", $B;
    my $chainLinkMeasure =
        `awk '{print \$5}' ${B} 2> /dev/null | sed -e "s/(//; s/)//"`;
    chomp $chainLinkMeasure;
    $chainLinkMeasure = 0.0 if (length($chainLinkMeasure) < 1);
    $chainLinkMeasure =~ s/\%//;
    my $chainSynMeasure = "";
#    if ( $D !~ m/^[a-z]/) {
#        $B=`ls /hive/data/genomes/${targetDb}/bed/awsMultiz/lastzRun/lastz_$accession/fb.${targetDb}.chainSyn.${accession}* 2>/dev/null`;
#        chomp $B;
#    } else {
        $B="/hive/data/genomes/${targetDb}/bed/lastz.${D}/fb.${targetDb}.chainSyn${Db}Link.txt";
#    }
# printf STDERR "# chainSyn %s\n", $B;
    if ($D =~ m/^GC/) {
        $B="/hive/data/genomes/${targetDb}/bed/lastz.${D}/fb.${targetDb}.chainSyn.${Db}Link.txt";
    }
    if ( -s "${B}" ) {
      $chainSynMeasure =
        `awk '{print \$5}' ${B} 2> /dev/null | sed -e "s/(//; s/)//"`;
      chomp $chainSynMeasure;
    }
    $chainSynMeasure = 0.0 if (length($chainSynMeasure) < 1);
    $chainSynMeasure =~ s/\%//;
    my $chainRBestMeasure = "";
#    if ( $D !~ m/^[a-z]/) {
#        $B=`ls /hive/data/genomes/${targetDb}/bed/awsMultiz/lastzRun/lastz_$accession/fb.${targetDb}.chainRBest.${accession}*`;
#        chomp $B;
#    } else {
        $B="/hive/data/genomes/${targetDb}/bed/lastz.${D}/fb.${targetDb}.chainRBest.${Db}.txt";
#    }
# printf STDERR "# chainRBest %s\n", $B;
    $chainRBestMeasure =
      `awk '{print \$5}' ${B} 2> /dev/null | sed -e "s/(//; s/)//"`;
    chomp $chainRBestMeasure;
    $chainRBestMeasure = 0.0 if (length($chainRBestMeasure) < 1);
    $chainRBestMeasure =~ s/\%//;
    my $swapFile="/hive/data/genomes/${D}/bed/lastz.${targetDb}/fb.${D}.chainCe11Link.txt";
    my $swapMeasure = "0";
    if ( -s $swapFile ) {
	$swapMeasure =
	    `awk '{print \$5}' ${swapFile} 2> /dev/null | sed -e "s/(//; s/)//"`;
	chomp $swapMeasure;
	$swapMeasure = 0.0 if (length($swapMeasure) < 1);
	$swapMeasure =~ s/\%//;
    }
    my $orgName=
    `hgsql -N -e "select organism from dbDb where name='$D';" hgcentraltest`;
    chomp $orgName;
    if (length($orgName) < 1) {
        if (defined($dbToName{$D})) {
          $orgName = $dbToName{$D};
        } else {
          $orgName="N/A";
        }
    }
    ++$count;
#    my $percentAlike = 100.0 * ($chainLinkMeasure - $chainRBestMeasure) / $chainLinkMeasure;
    my $percentAlike = 100.0 * ($chainLinkMeasure - $chainSynMeasure) / $chainLinkMeasure;
    if ( $orgName eq $D ) {
    printf "# %03d %.4f (%% %06.3f) (%% %06.3f) (%% %06.3f) %5.2f - %s\n", $count, $dist,
        $chainLinkMeasure, $chainSynMeasure, $chainRBestMeasure, $percentAlike, $orgName;
    } else {
    printf "# %03d %.4f (%% %06.3f) (%% %06.3f) (%% %06.3f) %5.2f - %s %s\n", $count, $dist,
        $chainLinkMeasure, $chainSynMeasure, $chainRBestMeasure, $percentAlike, $orgName, $D;
    }
}
close (FH);

printf "# count phylo    chain      synNet    rBest  synNet v.  query\n";
printf "#       dist      link                         chain\n";

