#!/usr/bin/env perl

use Getopt::Long;
use strict;
use warnings;

sub usage($) {
  my ($msg) = @_;
  if (length($msg)) {
     printf STDERR "%s\n", $msg;
  }
  printf STDERR "usage: aliasTextToBed.pl -chromSizes=chrom.sizes \\\n\t-aliasText=chromAlias.txt -aliasBed=chromAlias.bed \\\n\t-aliasAs=chromAlias.as -aliasBigBed=chromAlias.bb\n";
  printf STDERR "converts the chromAlias.txt file into a bed file and\n\tcorresponding .as definition\n";
  exit 255;
}

my $argc = scalar(@ARGV);
usage("Note: must have five arguments.") if ($argc != 5);

use vars qw/
    $opt_chromSizes
    $opt_aliasText
    $opt_aliasBed
    $opt_aliasBigBed
    $opt_aliasAs
    /;

my $optsOk = GetOptions(
   "chromSizes=s",
   "aliasText=s",
   "aliasBed=s",
   "aliasBigBed=s",
   "aliasAs=s",
   );

usage("Note: Cannot recognize the arguments properly ?") if (!$optsOk);

printf STDERR "# chromSizes: %s\n", $opt_chromSizes;
printf STDERR "# aliasText %s\n", $opt_aliasText;
printf STDERR "# aliasBed %s\n", $opt_aliasBed;
printf STDERR "# aliasBigBed %s\n", $opt_aliasBigBed;
printf STDERR "# aliasAs %s\n", $opt_aliasAs;

open (FH, "<$opt_aliasText") or die "can not read $opt_aliasText";
my $titleLine = <FH>;
chomp $titleLine;
if ($titleLine !~ m/^#\s/) {
  printf STDERR "ERROR: unrecognized alias file title line:\n%s\n", $titleLine;
  exit 255;
}
if ($titleLine =~ m/# sequenceName/) {
  printf STDERR "ERROR: this is an older style alias file:\n%s\n", $titleLine;
  exit 255;
}

my %chromSizes;	# key is chrom name, value is size
open (SZ, "<$opt_chromSizes") or die "can not read the chromSizes: $opt_chromSizes\n";
while (my $line = <SZ>) {
  chomp $line;
  my ($chrom, $size) = split('\s+', $line);
  $chromSizes{$chrom} = $size;
}
close (SZ);

my %nameLabels = (
   "assembly" => "Assembly",
   "genbank" => "GenBank",
   "ncbi" => "NCBI",
   "refseq" => "RefSeq",
   "ucsc" => "UCSC",
   "ensembl" => "Ensembl",
   "xenbase" => "Xenbase",
   "chrNames" => "chrNames",
   "chrN" => "chrN",
   "VEuPathDB" => "VEuPathDB",
   "custom" => "custom"
);

open (AS, ">$opt_aliasAs") or die "can not write to $opt_aliasAs";

my $indexNames;

$titleLine =~ s/^#\s+//;
my @legendNames = split('\s+', $titleLine);
my $expectFieldCount = scalar(@legendNames);
my $i = 0;
# output the .as definition
printf AS "table chromAlias\n";
printf AS "   \"chromAlias bigBed index\"\n";
printf AS "    (\n";
printf AS "    string chrom;\t\"native sequence name\"\n";
printf AS "    uint chromStart;\t\"always 0\"\n";
printf AS "    uint chromEnd;\t\"chromosome size\"\n";
foreach my $title (@legendNames) {
  if (length($indexNames)) {
    $indexNames .= "," . $title;
  } else {
    $indexNames = $title;
  }
  die "not defined title" if (!defined($title));
  die "not defined nameLabels{$title}" if (!defined($nameLabels{$title}));
  printf AS "    string %s;\t\"%s name\"\n", $title, $nameLabels{$title};
}
printf AS "    )\n";
close (AS);

printf STDERR "# indexNames: '%s'\n", $indexNames;

open (BD, "|sort -k1,1 -k2,2n>$opt_aliasBed") or die "can not write to $opt_aliasBed";

while (my $line = <FH>) {
  chomp $line;
  my @a = split('\t', $line, -1); # the -1 keeps all empty fields too
  if (scalar(@a) != $expectFieldCount) {
     printf STDERR "ERROR: expected field count %d =! %d on line %d\n", $expectFieldCount, scalar(@a), $.;
     exit 255;
  }
  my $nameIndex = 0;
  if (! exists $chromSizes{$a[$nameIndex]}) {
      die("sequence '$a[$nameIndex]' not found in chromSizes")
  }
  printf BD "%s\t0\t%d", $a[$nameIndex], $chromSizes{$a[$nameIndex]};
  foreach my $name (@a) {
    printf BD "\t%s", $a[$nameIndex++];
  }
  printf BD "\n";
}
close (FH);
close (BD);

printf STDERR "bedToBigBed -tab -type=bed3+5 -as=$opt_aliasAs -extraIndex=$indexNames $opt_aliasBed $opt_chromSizes $opt_aliasBigBed\n";
print `bedToBigBed -tab -type=bed3+5 -as=$opt_aliasAs -extraIndex=$indexNames $opt_aliasBed $opt_chromSizes $opt_aliasBigBed`;

__END__

# ucsc	assembly	genbank	ncbi	refseq
NW_011332701v1_alt	HG2139_PATCH	KN538374.1		NW_011332701.1
chr1	1	CM000663.2	1	NC_000001.11
chr10	10	CM000672.2	10	NC_000010.11
chr10_NT_187579v1_alt	HSCHR10_1_CTG3	KI270824.1		NT_187579.1
chr10_NT_187580v1_alt	HSCHR10_1_CTG4	KI270825.1		NT_187580.1
chr10_NW_003315934v1_alt	HSCHR10_1_CTG1	GL383545.1		NW_003315934.1
chr10_NW_003315935v1_alt	HSCHR10_1_CTG2	GL383546.1		NW_003315935.1
chr10_NW_009646202v1_alt	HG2191_PATCH	KN196480.1		NW_009646202.1
chr10_NW_011332692v1_alt	HG2241_PATCH	KN538365.1		NW_011332692.1
