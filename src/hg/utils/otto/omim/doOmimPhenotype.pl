#!/usr/bin/env perl
# Parse out phenotype information into a format compatible with the UCSC Genome Browser table

use strict;
use warnings;

# Gene map file path
my $geneMapFilePath = './genemap2.txt';

# Check for command parameters
for ( my $argc = 0; $argc <= $#ARGV; $argc++ ) {

	my $option = $ARGV[$argc];

	if ( $option =~ /^--gene-map-file=(.*)$/i ) {
		$geneMapFilePath = $1;
	}
	
	elsif ( $option =~ /^(--help|\-?)$/ ) {
		printf("Usage:\n");
		printf("\t[--help|-?] print out this message.\n");
		printf("\n");
		printf("\t[--gene-map-file=path] gene map file path, defaults to: '%s'.\n", $geneMapFilePath);
		printf("\n");
        exit (0);
	}
	else {
		printf("Error: Invalid action: '%s', type '%s  --help' for help.\n", $option, $0);
		exit (0);
	}
}

open (GENE_MAP_FILE, $geneMapFilePath) || die "Failed to open the gene map file: '$geneMapFilePath'";

while (my $line = <GENE_MAP_FILE>)
{
 	chomp $line;
  next if $line =~ /^\s*#/;

    my @fields = split(/\t/, $line, 14);

  my $mimNumber = $fields[5];
    my $phenotypes = _cleanText($fields[12]);

 	foreach my $phenotype ( split(/;\s*/, $phenotypes) )
  {
		my $phenotypeMimNumber = '';
		my $phenotypeMappingKey = '';

		# Extract the phenotype mim number and remove it
		if ( $phenotype =~ /(\d{6})/ ) {
			$phenotypeMimNumber = $1;
			$phenotype =~ s/\d{6}//;
		}

		# Extract the phenotype mapping key and remove it
		if ( $phenotype =~ /(.*)\((\d)\)/ ) {
			$phenotypeMappingKey = $2;
			$phenotype = $1
		}
	
		# Strip trailing comma from the phenotype
		if ( $phenotype =~ /(.*?),\s*$/ ) {
			$phenotype = $1
		}
 	
		# Final cleaning
 		$phenotype = _cleanText($phenotype);
 		
 		
 		# Copy the mim number to the phenotype mim number for phenotype mapping key == 2
#  		if ( $phenotypeMappingKey == 2 ) {
#  			$phenotypeMimNumber = $mimNumber;
#  		}
		
		printf("%s\t%s\t%s\t%s\n", $mimNumber, $phenotype, $phenotypeMimNumber, $phenotypeMappingKey);
 	}
}
 
close (GENE_MAP_FILE);
exit (0);
 

# Strip out extraneous spaces
sub _cleanText
{
  my $text = shift;
	return (undef) if !defined($text);

	$text =~ s/[\n\r]/ /g;
	$text =~ s/\s+/ /g;
	$text =~ s/^\s+//;
	$text =~ s/\s+$//;

	return ($text);
}
