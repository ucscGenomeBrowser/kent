#!/usr/bin/env perl

#--------------------------------------------------------------------------

use strict;
use warnings;


#--------------------------------------------------------------------------
#
# Documentation
#
#	./script1.pl --gene-map-file=genemap.txt
#


#--------------------------------------------------------------------------
#
# Required packages
#


#--------------------------------------------------------------------------
#
# Package Constants
#

BEGIN {

	# Set the locale to utf8
	$ENV{'LC_ALL'} = 'en_US.UTF-8';
	$ENV{'LANGUAGE'} = 'en_US.UTF-8';
	$ENV{'LANG'} = 'en_US.UTF-8';

}


#--------------------------------------------------------------------------
#
#	Function:	main()
#
#	Purpose:	main
#
#	Called by:	
#
#	Parameters:	
#
#	Global Variables:	
#
#	Returns:	void
#


# Gene map file path
my $geneMapFilePath = './genemap.txt';


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
	}
	else {
		printf("Error: Invalid action: '%s', type '%s  --help' for help.\n", $option, $0);
		exit (0);
	}
}


# Check the gene map file path
if ( !defined($geneMapFilePath) ) {
	die "Undefined gene map file path"
}



# Open the gene map file
open (GENE_MAP_FILE, $geneMapFilePath) || die "Failed to open the gene map file: '$geneMapFilePath'";


# Read the gene map file
while (<GENE_MAP_FILE>) {
 	
	# Get the line
 	my $line = $_;
 	chomp $line;
 
	# Split the fields
 	my @fields = split(/\|/, $line, 18);
 	
	# Get all the data
 	my $sort = $fields[0];
 
 	my $month = $fields[1];
 	my $day = $fields[2];
 	my $year = $fields[3];
 
 	my $cytoLocation = $fields[4];
 	my $geneSymbols = $fields[5];
 	my $confidence = $fields[6];
 
 	my $geneName0 = $fields[7];
 	my $geneName1 = $fields[8];
 
 	my $mimNumber = $fields[9];
 
 	my $mappingMethod = $fields[10];
 
 	my $comments0 = $fields[11];
 	my $comments1 = $fields[12];
 
 	my $phenotypes0 = $fields[13];
 	my $phenotypes1 = $fields[14];
 	my $phenotypes2 = $fields[15];
 
 	my $mouse = $fields[16];
 	my $references = $fields[17];
 
	# Re-assemble the hacked fields
 	my $geneName = $geneName0 . ' ' . $geneName1;
 	my $comments = $comments0 . ' ' . $comments1;
 	my $phenotypes = $phenotypes0 . ' ' . $phenotypes1 . ' ' . $phenotypes2;
 
 	# Clean the reassembled fields
 	$geneName = _cleanText($geneName);
 	$comments = _cleanText($comments);
 	$phenotypes = _cleanText($phenotypes);
 

 	# Split out the phenotypes
 	foreach my $phenotype ( split(/;\s*/, $phenotypes) ) {

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
	
		
		# Write out the data
		printf("%s\t%s\t%s\t%s\n", $mimNumber, $phenotype, $phenotypeMimNumber, $phenotypeMappingKey);

 	}
 
 }
 
 
 # Close the gene map file
 close (GENE_MAP_FILE);
 
 
 exit (0);

 

#--------------------------------------------------------------------------

 

sub _cleanText {

	my $text = shift;


	# Check the parameter
	return (undef) if !defined($text);
	

	# Replace new lines with spaces
	$text =~ s/[\n\r]/ /g;

	# Deduplicate spaces
	$text =~ s/\s+/ /g;

	# Trim leading and trailing spaces
	$text =~ s/^\s+//;
	$text =~ s/\s+$//;


	return ($text);

}


#--------------------------------------------------------------------------


