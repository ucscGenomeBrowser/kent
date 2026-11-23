#!/usr/bin/env perl

#
# asmHubList.pl - uses the clade listings to find the ./GCF and ./GCA
#               - hubs that have a hub.txt and thus are functional hubs
#  works on hgwdev and on hgdownload

use strict;
use warnings;
use File::Basename;
use JSON::PP;

my $argc = scalar(@ARGV);
if ($argc != 1) {
  printf STDERR "usage: asmHubList.pl goForIt | sort > assembly.hub.list.today\n";
  printf STDERR "will use the clade listings to find ready to\n";
  printf STDERR "go assembly hubs on hgwdev or on hgdownload.\n";
  exit 255;
}

my $goForIt = shift;
die "ERROR: need to have argument: goForIt in order to operate." if ($goForIt ne "goForIt");

my $thisMachine = `hostname -s`;
chomp $thisMachine;

if ($thisMachine eq "hgwdev") {
  chdir ("/hive/data/genomes/asmHubs");
} elsif ($thisMachine eq "hgdownload1") {
  chdir ("/mirrordata/hubs");
} elsif ($thisMachine eq "hgdownload2") {
  chdir ("/mirrordata/hubs");
} elsif ($thisMachine eq "hgdownload3") {
  chdir ("/mirrordata/hubs");
} else {
  die "ERROR: not running on hgdownload ?  Good for only hgwdev and hgdownload[123]";
}

my %cladeListings;	# key is assemblyName GCA/GCF identifier, value is clade
my %asmInfo;	# key is GCA/GCF identifier, value is hash of the output columns

# JSON strings: undef becomes "", characters beyond Latin-1 (from \u
# escapes) are utf8 encoded so they print without a 'Wide character' warning
sub jsonStr($) {
  my ($s) = @_;
  return "" if (!defined($s));
  utf8::encode($s) if ($s =~ m/[^\x00-\xff]/);
  return $s;
}

# the clade assemblyList.json has everything needed from each hub.txt,
# reading the one json file avoids opening tens of thousands of hub.txt
# files, which is slow when their data blocks are not in the page cache
sub readAssemblyList($) {
  my ($jsonFile) = @_;
  if ( ! -s "$jsonFile" ) {
    printf STDERR "# warning: missing %s, will read hub.txt files instead\n", $jsonFile;
    return;
  }
  open (my $jh, "<", $jsonFile) or die "can not read $jsonFile";
  my $text = do { local $/; <$jh> };
  close ($jh);
  # not utf8 mode, non-ASCII bytes pass through to the output unchanged
  my $json = JSON::PP->new->decode($text);
  foreach my $row (@{$json->{'data'}}) {
    next if (!defined($row->{'asmId'}));
    my ($gcX, $num, $asmName) = split('_', $row->{'asmId'}, 3);
    next if (!defined($num));
    my $acc = sprintf("%s_%s", $gcX, $num);
    next if (defined($asmInfo{$acc}));
    $asmInfo{$acc} = {
      asmName => defined($asmName) ? $asmName : "",
      sciName => jsonStr($row->{'sciName'}),
      descr => jsonStr($row->{'comName'}),
      taxId => jsonStr($row->{'taxId'}),
    };
  }
}

# fallback for an accession not found in any assemblyList.json
sub readHubTxt($) {
  my ($hubTxt) = @_;
  open (GE, "<", $hubTxt) or die "can not read $hubTxt";
  my $descr = "";
  my $sciName = "";
  my $asmId = "";
  my $taxId = "";
  my $asmName = "";
  while (my $line = <GE>) {
    chomp $line;
    $line =~ s/\r//g;
    my ($tag, $value) = split('\s+', $line, 2);
    next if (!defined($value));
    if ($tag eq "description") {
      $descr = $value;
    } elsif ($tag eq "scientificName") {
      $sciName = $value;
    } elsif ($tag eq "taxId") {
      $taxId = $value;
    } elsif ($tag eq "htmlPath") {
      $asmId = $value;
      $asmId=~ s/.description.html//;
      $asmId=~ s#html/##;
      (undef, undef, $asmName) = split('_', $asmId, 3);
    }
  }
  close (GE);
  $asmName = "" if (!defined($asmName));
  return { asmName => $asmName, sciName => $sciName, descr => $descr,
    taxId => $taxId };
}

open (FH, "ls /dev/shm/clade.*.list|grep -v legacy|") or die "can not read /dev/shm/clade.*.list";
while (my $cladeList = <FH>) {
  chomp $cladeList;
  my $clade = basename($cladeList);
  $clade =~ s/clade.//;
  $clade =~ s/.list//;
  readAssemblyList("./$clade/assemblyList.json");
  my $genomeCount = 0;
  open (CL, "<$cladeList") or die "can not read $cladeList\n";
  while (my $genome = <CL>) {
    chomp $genome;
    if (defined($cladeListings{$genome})) {
      printf STDERR "# warning: already defined clade for %s '%s' again '%s'\n", $genome, $cladeListings{$genome}, $clade;
    }
    $cladeListings{$genome} = $clade;
    ++$genomeCount;
  }
  printf STDERR "# clade %s has %d genomes\n", $clade, $genomeCount;
  close (CL);
}
close (FH);

readAssemblyList("./legacy/assemblyList.json");

open (FH, "</home/qateam/cronJobs/legacy.clade.txt") or die "can not read legacy.clade.txt";
while (my $line = <FH>) {
  chomp $line;
  my ($genome, $clade) = split('\t', $line);
  my @a = split("_", $genome);
  $genome = sprintf("%s_%s", $a[0], $a[1]);
  if (defined($cladeListings{$genome})) {
    printf STDERR "# warning: already defined clade for %s '%s' again '%s' legacy\n", $genome, $cladeListings{$genome}, $clade;
  } else {
    $cladeListings{$genome} = $clade . "(L)";
  }
}
close (FH);

my $missCount = 0;
my $hubTxtReads = 0;

# the hub directory path is computed directly from the accession:
#   GCF_000001405.40 -> ./GCF/000/001/405/GCF_000001405.40
# much faster than a 'find' through the GCF/GCA hierarchy.  Hubs on
# the filesystem not in any clade listing are not seen here, the weekly
# hubTxtAudit.sh script reports those.
foreach my $acc (sort keys %cladeListings) {
  my ($gcX, $num) = $acc =~ m/^(GC[AF])_(\d{9})\./;
  if (!defined($num)) {
    printf STDERR "# warning: can not parse accession '%s'\n", $acc;
    next;
  }
  my $path = sprintf("./%s/%s/%s/%s/%s", $gcX, substr($num,0,3),
     substr($num,3,3), substr($num,6,3), $acc);
  my $hubTxt = "$path/hub.txt";
  # the stat is fast, it keeps the listing to hubs present on this machine
  if ( -s "$hubTxt" ) {
    my $info = $asmInfo{$acc};
    if (!defined($info)) {
      $info = readHubTxt($hubTxt);
      ++$hubTxtReads;
    }
    my $clade = $cladeListings{$acc};
    printf "%s\t%s\t%s\t%s\t%s\t%s\n", $acc, $info->{'asmName'}, $info->{'sciName'}, $info->{'descr'}, $info->{'taxId'}, $clade;
  } else {
    ++$missCount;
    printf STDERR "# missing '%s'\n", $hubTxt;
  }
}

printf STDERR "# %d hub.txt files read for accessions not in assemblyList.json\n", $hubTxtReads;

if ($missCount > 0) {
  printf STDERR "# warning: failed to find hub.txt for %d genomes in clade listings\n", $missCount;
}
