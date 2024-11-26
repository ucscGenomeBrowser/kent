#!/usr/bin/env perl

use strict;
use warnings;
use taxId;

my $commonNames = "allAssemblies.commonNames.tsv";
my %commonNames;	# key is taxId, value is 'shortest' common name
my %existingImages;	# key is scientific name from jpg/png/gif image name
                        # value is 1
my %firstNames;		# key is first name of image name, value is count of
                        # how often seen

my $wikiText = 0;

my $argc = scalar(@ARGV);
if ($argc > 0) {
  while (my $arg = shift) {
     $wikiText = 1 if ($arg eq "-wiki");
  }
}

# a few species have photo files not exactly matching the scientific name
my %photoFile;	# key is taxId, value is photo file name
open (FH, "<taxId.photoFile.tab") or die "can not read taxId.photoFile.tab";
while (my $line = <FH>) {
  chomp $line;
  my ($taxId, $photoURL) = split('\t', $line);
  $photoFile{$taxId} = $photoURL;
}
close (FH);

## prepare the commonNames hash
&taxId::commonNames("taxId.commonNames.2017-02-08.tab", $commonNames, \%commonNames);

## set up the image list(s) from /usr/local/apache/htdocs/images
&taxId::imageList(\%existingImages, \%firstNames);

&taxId::header($wikiText, "RefSeq primate assemblies");

my $speciesCount = 0;
my $devCount = 0;
my $rrCount = 0;

my $tsvFile = "allAssemblies.taxonomy.tsv";

open (FH, "grep -w Primates $tsvFile | cut -f1-4|") or die "can not grep $tsvFile";
while (my $line = <FH>) {
  chomp $line;
  my ($taxId, $asmAcc, $asmName, $sciName) = split('\t', $line);
  my @a = split('\s+', $sciName);
  my $dbName = lc(substr($a[0], 0, 3));
  for (my $i = 1; $i < scalar(@a); ++$i) {
     $dbName .= ucfirst(lc(substr($a[$i], 0, 3)));
  }
  $dbName =~ s/homSap/hg/;
  $dbName =~ s/macMul/rheMac/;
  $dbName =~ s/carSyr/tarSyr/;
  $dbName =~ s/saiBolBol/saiBol/;
  $dbName =~ s/gorGorGor/gorGor/;
  $dbName =~ s/colAngPal/colAng/;
  $dbName =~ s/cebCapImi/cebCap/;
  my $dbDbDev = `/cluster/bin/x86_64/hgsql -N -e 'select name from dbDb where name like "$dbName%";' hgcentraltest | egrep -v "Patch|Haps|LggInv|mmtv|Strains" | wc -l`;
  chomp $dbDbDev;
  my $trackDbDev = 0;
  my $sampleDev = "n/a";
  if ($dbDbDev > 0) {
     $sampleDev = `/cluster/bin/x86_64/hgsql -N -e 'select name from dbDb where name like "$dbName%";' hgcentraltest | egrep -v "Patch|Haps|LggInv|mmtv|Strains" | sort -r | head -1`;
     chomp $sampleDev;
     $trackDbDev = `/cluster/bin/x86_64/hgsql -N -e 'show table status like "trackDb";' $sampleDev | grep 2017 | wc -l`;
     chomp $trackDbDev;
  }
  my $dbDbCent = `/cluster/bin/x86_64/hgsql -hgenome-centdb -N -e 'select name from dbDb where name like "$dbName%";' hgcentral | egrep -v "Patch|Haps|LggInv|mmtv|Strains" | wc -l`;
  chomp $dbDbCent;
  my $commonName = "n/a";
  my $exampleCentDb = &taxId::mostRecent($dbName, "-hgenome-centdb", "hgcentral");
  ++$rrCount if ($exampleCentDb ne $dbName);
  my $exampleDev = &taxId::mostRecent($dbName, "", "hgcentraltest");
  ++$devCount if ($exampleDev ne $dbName);
  $commonName = $commonNames{$taxId} if (exists($commonNames{$taxId}));;
  my $imageFound = 0;
  my $firstName = $sciName;
  $firstName =~ s/ .*//;
  my $nameBar = $sciName;
  $nameBar =~ s/ /_/g;
  my $imageFile = "n/a";
  if (exists($existingImages{$nameBar})) {
     $imageFound = 1;
     $imageFile = $existingImages{$nameBar};
  }
  if ($exampleCentDb ne $dbName) {
     if ($imageFound != 1) {
        if (exists($firstNames{$firstName}) ) {
           if (1 == $firstNames{$firstName}) {
              $imageFile = "should find";
              $imageFile = $photoFile{$taxId} if (exists($photoFile{$taxId}));
              $imageFound = 1;
           }
        }
     }
  }
  ++$speciesCount;
  &taxId::rowOutput($wikiText, $taxId, $exampleDev, $dbDbDev, $exampleCentDb, $dbDbCent, $sciName, $commonName, $imageFile, $speciesCount);
}
close (FH);

&taxId::tableClose($wikiText);

if (! $wikiText) {
  my $missing = $speciesCount - $devCount;
  printf "#\t%d\tprimates\t%d on hgwdev\t%d on RR\t%d not found\n", $speciesCount, $devCount, $rrCount, $missing;
  printf STDERR "#\t%d\tprimates\t%d on hgwdev\t%d on RR\t%d not found\n", $speciesCount, $devCount, $rrCount, $missing;
}
