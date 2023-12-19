#!/usr/bin/env perl

use strict;
use warnings;
use File::Temp qw/ tempfile tempdir /;

my $argc = scalar(@ARGV);

if ($argc != 3) {
  printf STDERR "usage: ./aliasBedToCt.pl chromAlias.bed chromAlias.bb resultDir\n";
  printf STDERR "reads the chromAlias.bed file, writes several files\n";
  printf STDERR "into the resultDir one file for each name scheme\n";
  printf STDERR "uses the given chromAlias.bb for chromSizes to bedToBigBed\n";
  exit 255;
}

my $bedFile = shift;
my $aliasBb = shift;
my $resultDir = shift;
printf STDERR "# chromAlias input: %s\n", $bedFile;
printf STDERR "# chromAlias.bb input: %s\n", $aliasBb;
printf STDERR "# results to: %s/\n", $resultDir;

my @sourceNames;	# the name label
my @outFiles;	# reference to open file handle for each source name

open (FH, "<$bedFile") or die "can not read $bedFile";
my $headerLine = <FH>;
chomp $headerLine;
my @a = split('\t', $headerLine);
for (my $i = 3; $i < scalar(@a); ++$i) {
  my $outFile = sprintf("%s/%s.ct.txt", $resultDir, $a[$i]);
  open (my $fh, '>', $outFile) or die "can not write to $outFile";
  printf STDERR "# %s\t%s\n", $a[$i], $outFile;
  push @sourceNames, $a[$i];
  push @outFiles, $fh;
  printf $fh "track name='%s chrNames' description='chrom alias test with \"%s\" name scheme' type=bed visibility=pack\n", $a[$i], $a[$i];
}
chomp $headerLine;
while (my $line = <FH>) {
  chomp $line;
  my @a = split('\t', $line, -1);
  for (my $i = 0; $i < scalar(@sourceNames); ++$i) {
     my %nameDone;
     my $fh = $outFiles[$i];
     if (length($a[3+$i])) {
       $nameDone{$a[3+$i]} = 1;
       $nameDone{$a[0]} = 1;
       printf $fh "%s\t%d\t%d\t%s", $a[3+$i], $a[1], $a[2], $a[0];
       for (my $j = 3; $j < scalar(@a); ++$j) {
          next if (defined($nameDone{$a[$j]}));
          if ($j != 3+$i) {
             if (!defined($nameDone{$a[$j]})) {
               printf $fh ",%s", $a[$j] if (length($a[$j]));
               $nameDone{$a[$j]} = 1;
             }
          }
       }
       printf $fh "\n";
     }
  }
}
close (FH);

for (my $i = 0; $i < scalar(@outFiles); ++$i) {
  close ($outFiles[$i]);
}

my $tmpFile = "/dev/shm/chromAliasTest.$$.bed";
foreach my $source (@sourceNames) {
  my $ctFile = sprintf("%s/%s.ct.txt", $resultDir, $source);
  my $bbFile = sprintf("%s/%s.bb", $resultDir, $source);
  print `grep -v "^track" $ctFile | sort > $tmpFile`;
  print `bedToBigBed -sizesIsBb $tmpFile $aliasBb $bbFile`;
}
print `rm -f $tmpFile`;

__END__

==> GCF_000001405.39/GCF_000001405.39.chromAlias.bed <==
#chrom	chromStart	chromEnd	ucsc	assembly	genbank	ncbi	refseq
NW_011332701v1_alt	0	4998962	NW_011332701v1_alt	HG2139_PATCH	KN538374.1		NW_011332701.1
chr1	0	248956422	chr1	1	CM000663.2	1	NC_000001.11
chr10	0	133797422	chr10	10	CM000672.2	10	NC_000010.11
chr10_NT_187579v1_alt	0	181496	chr10_NT_187579v1_alt	HSCHR10_1_CTG3	KI270824.1		NT_187579.1
chr10_NT_187580v1_alt	0	188315	chr10_NT_187580v1_alt	HSCHR10_1_CTG4	KI270825.1		NT_187580.1
chr10_NW_003315934v1_alt	0	179254	chr10_NW_003315934v1_alt	HSCHR10_1_CTG1	GL383545.1		NW_003315934.1
chr10_NW_003315935v1_alt	0	309802	chr10_NW_003315935v1_alt	HSCHR10_1_CTG2	GL383546.1		NW_003315935.1
chr10_NW_009646202v1_alt	0	277797	chr10_NW_009646202v1_alt	HG2191_PATCH	KN196480.1		NW_009646202.1
chr10_NW_011332692v1_alt	0	14347	chr10_NW_011332692v1_alt	HG2241_PATCH	KN538365.1		NW_011332692.1

==> hg38/hg38.chromAlias.bed <==
#chrom	chromStart	chromEnd	ucsc	assembly	ensembl	genbank	refseq
chr1	0	248956422	chr1	1	1	CM000663.2	NC_000001.11
chr10	0	133797422	chr10	10	10	CM000672.2	NC_000010.11
chr10_GL383545v1_alt	0	179254	chr10_GL383545v1_alt	HSCHR10_1_CTG1		GL383545.1	NW_003315934.1
chr10_GL383546v1_alt	0	309802	chr10_GL383546v1_alt	HSCHR10_1_CTG2		GL383546.1	NW_003315935.1
chr10_KI270824v1_alt	0	181496	chr10_KI270824v1_alt	HSCHR10_1_CTG3		KI270824.1	NT_187579.1
chr10_KI270825v1_alt	0	188315	chr10_KI270825v1_alt	HSCHR10_1_CTG4		KI270825.1	NT_187580.1
chr10_KN196480v1_fix	0	277797	chr10_KN196480v1_fix	HG2191_PATCH		KN196480.1	NW_009646202.1
chr10_KN538365v1_fix	0	14347	chr10_KN538365v1_fix	HG2241_PATCH		KN538365.1	NW_011332692.1
chr10_KN538366v1_fix	0	85284	chr10_KN538366v1_fix	HG2242_HG2243_PATCH		KN538366.1	NW_011332693.1
