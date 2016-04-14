#!/usr/bin/env perl

use strict;
use warnings;

sub usage() {
  printf STDERR "usage: ./ucscCompositeAgp.pl ../refseq/*_genomic.fna.gz ../refseq/*_assembly_structure/Primary_Assembly\n";
}

my $argc = scalar(@ARGV);

if ($argc != 2) {
  usage;
  exit 255;
}

my $refseqFasta = shift(@ARGV);
my $primary = shift(@ARGV);

my %accToChr;
# #Chromosome     Accession.version
# 1       CM002885.1
# 2       CM002886.1
# 3       CM002887.1

open (FH, "<$primary/assembled_chromosomes/chr2acc") or
        die "can not read Primary_Assembly/assembled_chromosomes/chr2acc";
while (my $line = <FH>) {
    next if ($line =~ m/^#/);
    chomp $line;
    my ($chrN, $acc) = split('\s+', $line);
    $accToChr{$acc} = $chrN;
}
close (FH);

# check if refseq.2bit needs to be created or
#      refseqFasta is newer than refseq.2bit
if ( ! -e "refseq.2bit" || (stat "$refseqFasta")[9] > (stat "refseq.2bit")[9]) {
  printf STDERR "# constructing refseq.2bit from $refseqFasta\n";
  print STDERR `faToTwoBit -noMask $refseqFasta "refseq.2bit"`;
  print STDERR `touch -r $refseqFasta "refseq.2bit"`;
}

foreach my $acc (sort keys %accToChr) {
    my $chrN =  $accToChr{$acc};
    printf STDERR "$acc chr$chrN\n";
    my $theirChr = "chr${chrN}";
    open (FH, "zcat $primary/assembled_chromosomes/AGP/${theirChr}.comp.agp.gz|") or die "can not read chr${chrN}.comp.agp.gz";
    open (UC, ">chr${chrN}.agp") or die "can not write to ${theirChr}.agp";
    while (my $line = <FH>) {
        if ($line =~ m/^#/) {
            print UC $line;
        } else {
            $line =~ s/^$acc/chr${chrN}/;
            print UC $line;
        }
    }
    close (FH);
    close (UC);

    open (UC, "|gzip -c >chr${chrN}.fa.gz") or die "can not write to chr${chrN}.fa.gz";
    printf UC ">chr${chrN}\n";
    print UC `twoBitToFa refseq.2bit:$acc stdout | grep -v "^>"`;
    close (UC);
}
