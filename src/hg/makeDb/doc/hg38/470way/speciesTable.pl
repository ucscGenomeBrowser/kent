#!/usr/bin/env perl

use strict;
use warnings;

my %genArk;	# key is GCx, value is full asmId

open (FH, "grep -v '^#' master.run.list|") or die "can not read master.run.list";
while (my $line = <FH>) {
  my (undef, $accession, $asmId, $clade, $sciName) = split('\s+', $line);
  $genArk{$accession} = $asmId;
}
close (FH);

my %hillerData;	# key is asmName, value is "source|GCx"

open (FH, "grep -h ^HL listOf*.tsv|") or die "can not read liftOf*.tsv";
while (my $line= <FH>) {
  chomp $line;
  my ($asmName, $source, $GCx, $sciName) = split("\t", $line);
  if (defined($hillerData{$asmName})) {
    printf STDERR "# error duplicate name %s\n", $asmName;
    exit 255;
  }
  $hillerData{$asmName} = sprintf("%s;%s", $source, $GCx);
  printf STDERR "# hillerData{%s} = %s\n", $asmName, $hillerData{$asmName};
}
close (FH);

# listOfAssembliesMammals.tsv
# listOfAssembliesBirds.tsv

# HLaciJub2	NCBI	GCF_003709585.1	Acinonyx jubatus
# HLacaChl2	NCBI	GCA_016904835.1	Acanthisitta chloris

printf "<table border=1 class='stdTbl'>\n";
printf "<tr><th>count</th>
    <th>common<br>name</th>
    <th>clade</th>
    <th>scientific<br>name</th>
    <th>assembly</th>
    <th>taxon&nbsp;id</th>
</tr>
";

my $count = 0;
printf "<tr><th>%d</th>
    <th style='text-align:left;'>human</th>
    <th>primates</th>
    <th style='text-align:left;'>Homo sapiens</th>
    <th style='text-align:left;'><a href='/cgi-bin/hgTracks?db=hg38' target=_blank>Dec. 2013 (GRCh38/hg38)</a></th>
    <th style='text-align:right;'><a href='https://www.ncbi.nlm.nih.gov/Taxonomy/Browser/wwwtax.cgi?id=9606' target=_blank>9606</a></th>
</tr>\n", ++$count;


my @cladeOrder = qw( Primates Euarchontoglires Carnivora Laurasiatheria Cetartiodactyla Artiodactyla Xenarthra Chiroptera Glires Afrotheria Metatheria Monotremata );

foreach my $clade (@cladeOrder) {
open (FH, "grep -w $clade phyloOrder.db.distance.taxId.sciName.comName.tsv|") or die "can not read phyloOrder.db.distance.taxId.sciName.comName.tsv";
while (my $line = <FH>) {
  chomp $line;
  my ($asmName, $phyloDist, $taxId, $sciName, $comName, $clade) = split("\t", $line);
  $comName =~ s/'/&#39;/g;
# https://www.ncbi.nlm.nih.gov/assembly/GCA_009769605.1/
  printf "<tr><th>%d</th>
    <th style='text-align:left;'>%s</th>
    <th>%s</th>
    <th style='text-align:left;'>%s</th>\n", ++$count, $comName, $clade, $sciName;
  if ($asmName =~ m/^[a-z]/) {
    my $descr = `hgsql -N -e 'select description from dbDb where name="$asmName";' hgcentraltest`;
    chomp $descr;
    printf "    <th style='text-align:left;'><a href='/cgi-bin/hgTracks?db=%s' target=_blank>%s</a></th>\n", $asmName, $descr;
  } else {
    if (defined($hillerData{$asmName})) {
      my ($source, $GCx) = split(";", $hillerData{$asmName});
      if (defined($genArk{$GCx})) {
        printf "    <th style='text-align:left;'><a href='/h/%s' target=_blank>%s %s</a></th>\n", $GCx, $asmName, $genArk{$GCx};
      } else {
        if ($source eq "NCBI") {
          printf "    <th style='text-align:left;'><a href='https://www.ncbi.nlm.nih.gov/assembly/%s' target=_blank>%s %s</a></th>\n", $GCx, $asmName, $GCx;
        } elsif ($source =~ m/DNA Zoo/) {
          my $zooName = $sciName;
          $zooName =~ s/ /_/g;
          printf "    <th style='text-align:left;'><a href='https://www.dnazoo.org/assemblies/%s' target=_blank>DNA zoo %s</a></th>\n", $zooName, $sciName;
        } else {
          printf "    <th style='text-align:left;'>%s/%s/%s</th>\n", $asmName, $source, $GCx;
        }
      }
    } else {
      printf "    <th style='text-align:left;'>%s</th>\n", $asmName;
    }
  }
    printf "    <th style='text-align:right;'><a href='https://www.ncbi.nlm.nih.gov/Taxonomy/Browser/wwwtax.cgi?id=%d' target=_blank>%d</a></th>
</tr>\n", $taxId, $taxId;
}	#	while (my $line = <FH>)
close (FH);
}	#	foreach my $clade (@cladeOrder)
printf "</table><br>\n";

__END__

head phyloOrder.db.distance.taxId.sciName.comName.tsv
panTro6 0.011908        9598    Pan troglodytes chimpanzee      Primates
panPan3 0.011944        9597    Pan paniscus    pygmy chimpanzee        Primates
gorGor6 0.017625        9595    Gorilla gorilla gorilla western lowland gorillaPrimates

tupBel1 37347   Tupaia belangeri        northern tree shrew
tupChi1 246437  Tupaia chinensis        Chinese tree shrew
turTru2 9739    Tursiops truncatus      common bottlenose dolphin
ursMar1 29073   Ursus maritimus polar bear
vicPac2 30538   Vicugna pacos   alpaca
[hiram@hgwdev /hive/data/genomes/hg38/bed/multiz470way/html]   og
total 0
[hiram@hgwdev /hive/data/genomes/hg38/bed/multiz470way/html] cp -p ../nameLists/db.taxId.sciName.comName.tsv  .
[hiram@hgwdev /hive/data/genomes/hg38/bed/multiz470way/html] og
total 64
-rw-rw-r-- 1 24256 Aug 19 16:07 db.taxId.sciName.comName.tsv

