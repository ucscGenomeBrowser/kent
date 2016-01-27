#!/usr/bin/env perl

use strict;
use warnings;

sub usage() {
  printf STDERR "usage: ./compositeAgp.pl [ucsc|ncbi] <pathTo>/*_assembly_structure/Primary_Assembly\n";
  printf STDERR "output AGP is to stdout, redirect to > chr.agp\n";
  printf STDERR "reroute stderr output thru a 'sort -u' to write a file\n";
  printf STDERR "    of the UCSC to NCBI name correspondence, e.g.:\n";
  printf STDERR "       2> >(sort -u > xxx.ucsc.to.ncbi.chr.names)\n";
  printf STDERR "expecting to find assembled_chromosomes hierarchy at Primary_Assembly/\n";
  printf STDERR "the ucsc|ncbi selection determines the naming scheme\n";
}

my $argc = scalar(@ARGV);

if ($argc != 2) {
  usage;
  exit 255;
}

my $ncbiUcsc = shift(@ARGV);
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
    my ($chrN, $acc) = split('\t', $line);
    $chrN =~ s/ /_/g;
    $accToChr{$acc} = $chrN;
}
close (FH);

foreach my $acc (keys %accToChr) {
    my $chrN =  $accToChr{$acc};
    my $theirChr = "chr${chrN}";
    open (FH, "zcat $primary/assembled_chromosomes/AGP/${theirChr}.comp.agp.gz|") or die "can not read chr${chrN}.comp.agp.gz";
    while (my $line = <FH>) {
        if ($line =~ m/^#/) {
            print $line;
        } else {
            $theirChr = "chrM" if ($theirChr eq "chrMT");
            printf STDERR "%s\t%s\n", $theirChr, $acc if ($ncbiUcsc ne "ncbi");
            $line =~ s/^$acc/$theirChr/ if ($ncbiUcsc ne "ncbi");
            print $line;
        }
    }
    close (FH);
}
