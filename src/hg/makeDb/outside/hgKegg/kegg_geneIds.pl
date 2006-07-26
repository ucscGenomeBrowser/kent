#!/usr/local/bin/perl -w

## Author: Chad Chen
## Last Modified: 10/1/2003
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

my $timeStamp= `date +%Y_%m_%d_%H%M%S`;
print $timeStamp . "\n";
my $fhId = IO::File->new(">KeggData/kegg-$org-allgenesid-$timeStamp".".txt") or die $!;
my $count=0;

my $url = 'http://soap.genome.ad.jp/KEGG.wsdl';

my $response = SOAP::Lite
			   -> service("$url") 
			   -> get_all_genes_by_organism($org);

select $fhId;
$|=1;

foreach (@{$response})
{
    $count++;
	if($_ =~ /$org:(\d+)/)
	{
    	print $fhId "$1\n";
	}
}
$fhId->close();

select STDOUT;
$|=1;
print "number of gene ids retrieved=" . $count . "\n";
