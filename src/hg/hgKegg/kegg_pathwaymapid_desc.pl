#!/usr/bin/perl -w

## Author: Chad Chen
## Last modified: 10/1/2003
## Comments: This program reads in the gene id entries from the input file
##           and use the gene ids to search KEGG's website to further 
##           retrieve the pathway id(s) associated with each gene entry. 
##           The ouput file will be put under the KeggData subdirectory with 
##           timestamp attaced.

## Note: You can also retrieve the pathways associated with each gene entry
##       using the SOAP API provided by KEGG. However, the SOAP API is much
## 		 slower than the method used in this program.
## Usage: kegg_pathwaymapid_desc.pl geneIdFile organism
##        geneIdFile -- the input file containing all gene ids for a certain organism
##        organism -- the name of the organism we are interested in 

use strict;
use SOAP::Lite;
use IO::File;
use LWP::Simple;

die "Usage: $0 geneIdFile organism_name (e.g. hsa, eco, mmu ...)\n" if @ARGV!=2;

my $geneIdFile=$ARGV[0];
my $org=$ARGV[1];
my $timeStamp= `date +%Y_%m_%d_%H%M%S`;
my $fhId = IO::File->new("<$geneIdFile") or die $!;
my $fhIdAndPathway = IO::File->new(">KeggData/kegg-$org-pathways-desc-$timeStamp" . ".txt") or die $!; 
my $count=0;

select $fhIdAndPathway;
$|=1;

while(<$fhId>)
{
	chomp($_);
    $count++;
	my $tempGeneId=$_;
	my $tempUrl="http://www.genome.ad.jp/dbget-bin/www_bget?$org+" . $tempGeneId;	
	my $htmlContent=get($tempUrl);
	my $tempArrRef=[];
	&retrievePathwayArr($htmlContent,$tempArrRef,0);
	my $pathwayCount=0;
	foreach (@{$tempArrRef})
	{
		my $htmlTitle=&retrieveHtmlTitle($tempGeneId,$_);
		print $fhIdAndPathway "$tempGeneId" . "\t" . $htmlTitle . "\t" . $_ . "\n" ;
	}
}

$fhId->close();
$fhIdAndPathway->close();

sub retrievePathwayArr()
{
	my $urlContent=$_[0];
	my $pathwayArray=$_[1];
	my $arrLen=$_[2];
	if($urlContent =~ /(.*?)PATH:<a HREF=.*?>(.*?)<\/a>(.*)/is)
	{
		$pathwayArray->[$arrLen++]=$2;
		my $remainingContent=$3;
		&retrievePathwayArr($remainingContent,$pathwayArray,$arrLen);
	}
	else
	{
		return $pathwayArray;
	}	
}	

sub retrieveHtmlTitle()
{
	my $geneId=$_[0];
	my $pathId=$_[1];
    my $tempUrl="http://www.genome.ad.jp/dbget-bin/show_pathway?$pathId+$geneId";
    my $htmlContent=get($tempUrl);
    # m--to match multiple lines, s-- matching all chars including newlines
    if($htmlContent =~ /<title>(.*?)<\/title>/sim)
    {
        my $htmlTitle=$1;
        $htmlTitle =~ s/^\s*(.*?)( - .*)/$1/s;
		return $htmlTitle;
    }
}
1;
