#!/usr/bin/env perl

# DO NOT EDIT the /cluster/bin/scripts copy of this file -- 
# edit ~/kent/src/hg/utils/lft2BitToFa.pl instead.

# $Id: lft2BitToFa.pl,v 1.1 2006/05/04 16:18:58 hiram Exp $

use strict;
use warnings;
use File::Basename;
    
sub usage()
{
print "lft2BitToFa.pl - extract named contig fasta sequence from 2bit genome\n";
printf " and a contig lift file.\n";
printf "usage: %s <file.2bit> <file.lft> [more_files.lft]\n",  basename($0);
printf "\tfasta output is to stdout, therefore route stdout to result file\n";
printf "example: lft2BitToFa.pl mm8.2bit 1/lift/ctg_random.lft > chr1_random.ctg.fa\n";
exit 255;
}   
    
my $argc = scalar(@ARGV);

usage if ($argc < 2);

my $twoBitFile = shift;
    
while (my $liftFile = shift)
{   
open (FH,"<$liftFile") or die "Can not open $liftFile";
while (my $line=<FH>)
    {
    chomp $line;
    my ($start, $contig, $length, $chrom, $chrom_length) = split('\s',$line);
    my $cmd=sprintf("twoBitToFa $twoBitFile:%s:%d-%d stdout",
        $chrom, $start, $start+$length);
    print `$cmd | sed -e "s#^>.*#>$contig#"`;
    }
close (FH);
}
