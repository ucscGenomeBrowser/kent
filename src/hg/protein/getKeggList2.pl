#!/usr/local/bin/perl -w -I /usr/lib/perl5/site_perl/5.6.1

## Author: Chad Chen
## Last Modified: 10/1/2003
## Last Modified by Fan: 10/3/2003
## This program retrieves all the human gene entries from the 
## KEGG database using the SOAP protocol. The ouput file will be
## put under the KeggData subdirectory with timestamp attaced.
## Usage: kegg_geneIds.pl name_of_organism
##        name_of_organism -- the name of the organism we are interested in

use strict;
use SOAP::Lite;
use IO::File;

die "Usage: $0 organism_name (e.g., hsa, eco, mmu ...)\n" if @ARGV!=1;

my $org=$ARGV[0];

my @aaMap;
my @bbID;
my @lines;
my $map;
my $dd;

my $infilename;
$infilename=$org.".lis";

#my $fhId = IO::File->new(">hsa.dat") or die $!;
open INF, $infilename;

#select $fhId;
@lines = <INF>;
foreach (@lines)
    {
    $map=substr($_, 0, 13);
    my $desc = substr($_, 14);
    chomp($desc);
    my $url = 'http://soap.genome.ad.jp/KEGG.wsdl';
    my $response = SOAP::Lite
	-> service("$url") 
	-> get_genes_by_pathway($map);

$|=1;

foreach (@{$response})
    {
    @aaMap = split /:/, $map, 2;
    @bbID = split /:/,$_, 2;
    print $bbID[1],"\t", $aaMap[1], "\t", $desc, "\n";
    }

select STDOUT;
$|=1;
}
close INF;
#$fhId->close();
