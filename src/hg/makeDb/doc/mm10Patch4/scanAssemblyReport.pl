#!/usr/bin/env perl

use strict;
use warnings;

sub usage() {
  printf STDERR "usage: scanAssemblyReport.pl <chrom.sizes> <faCount.GRCH38.p2.txt> <GCA_000001405.17_GRCh38.p2_assembly_report.txt>\n";
}

my $argc = scalar(@ARGV);
if ($argc != 3) {
  usage;
  exit 255;
}

my $chromSizes = shift;
my $faCount = shift;
my $asmReport = shift;

my %chrSize;

open (FH, "<$chromSizes") or die "can not read $chromSizes";
while (my $line = <FH>) {
  chomp $line;
  my ($chr, $size) = split('\t', $line);
  $chrSize{$chr} = $size;
}
close (FH);

my %patchSize;
open (FH, "<$faCount") or die "can not read $faCount";
while (my $line = <FH>) {
  next if ($line =~ m/^#seq|^total/);
  chomp $line;
  my ($chr, $size, $rest) = split('\s+', $line, 3);
  $patchSize{$chr} = $size;
}
close (FH);

# #seq	len	A	C	G	T	N	cpg
# CM000663.2	248956422	67070277	48055043	48111528	67244164	18475410	2375159
# KI270706.1	175055	50401	37708	35908	51038	0	1589
# KI270707.1	32032	10439	6781	6926	7886	0	311
# KI270932.1	215732	56283	52137	48354	58958	0	2720
# KI270933.1	170537	44612	40866	38012	47047	0	2095
# GL000209.2	177381	46045	42743	39757	48836	0	2193
# J01415.2	16569	5124	5181	2169	4094	1	435
# total	3221487035	901716923	626373718	628977988	904389966	160028440	31134771

open (FH, "<$asmReport") or die "can not read $asmReport";
while (my $line = <FH>) {
   next if ($line =~ m/^#/);
   chomp $line;
   my @a = split('\t', $line);
   my $chrN = $a[2];
   $chrN = "M" if ($chrN eq "MT");
   my $ucscName = "chr${chrN}";
   my $genbankName = $a[4];
   my $refseqName = $a[6];
   my $patchSize = $patchSize{$genbankName};
   $genbankName =~ s/\.1//;
   $genbankName =~ s/\./v/;
   if ($a[1] =~ m/alt-scaffold/) { $ucscName = "chr${chrN}_${genbankName}_alt"; }
   elsif ($a[1] =~ m/unlocalized-scaffold/) { $ucscName = "chr${chrN}_${genbankName}_random"; }
   elsif ($a[1] =~ m/assembled-molecule/) { $ucscName = "chr${chrN}"; }
   elsif ($a[1] =~ m/unplaced-scaffold/) { $ucscName = "chrUn_${genbankName}"; }
   elsif ($a[1] =~ m/fix-patch/) { $ucscName = "chr${chrN}_${genbankName}_fix"; }
   elsif ($a[1] =~ m/novel-patch/) { $ucscName = "chr${chrN}_${genbankName}_alt"; }
   else { die "do not recognize type: '$a[1]'";
   }
   my $warnings = "";
   if (length($ucscName) > 31) { $warnings .= "\t# warning size"; }
   if (exists($chrSize{$ucscName})) {
     if ($patchSize != $chrSize{$ucscName}) { $warnings .= "\t# bad size"; }
     printf "%s\t%d\t%s\t%s%s\n", $ucscName, $chrSize{$ucscName}, $a[4], $a[6], $warnings;
   } else {
     printf "%s\t%d\t%s\t%s\tnew%s\n", $ucscName, $patchSize, $a[4], $refseqName, $warnings;
   }
}
close (FH);

__END__
HG986_PATCH	fix-patch	1	Chromosome	KN196472.1	=	NW_009646194.1	PATCHES

HSCHR11_CTG1_UNLOCALIZED	unlocalized-scaffold	11	Chromosome	KI270721.1	=	NT_187376.1	Primary Assembly

     4 novel-patch
     25 assembled-molecule
     27 fix-patch
     42 unlocalized-scaffold
    127 unplaced-scaffold
    261 alt-scaffold


HSCHRUN_RANDOM_170       unplaced-scaffold       na      na      KI270335.1      =       NT_187462.1     Primary Assembly

LRC_KIR 19      54025634       55084318 alt-scaffold    GL949753.2      NW_003571061.2  ALT_REF_LOCI_8

Y	assembled-molecule	Y	Chromosome	CM000686.2	=	NC_000024.10	Primary Assembly
HSCHR1_CTG1_UNLOCALIZED	unlocalized-scaffold	1	Chromosome	KI270706.1	=	NT_187361.1	Primary Assembly
HSCHR1_CTG2_UNLOCALIZED	unlocalized-scaffold	1	Chromosome	KI270707.1	=	NT_187362.1	Primary Assembly
