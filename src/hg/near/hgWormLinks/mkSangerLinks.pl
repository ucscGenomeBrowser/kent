#!/usr/bin/env perl

#	the inputs are four files:
#	wormpep.table140 - 1st column == orf name, 5 == protein name
#	geneIDs.WS140.txt - WBGene number, Gene Class, orfName  (CSV !)
#	wbLongDescr.txt - WBGene number <tab> long description
#	wbGeneClass.txt - Gene Class <tab> concise description
#
#	By using some columns from these files, the different types
#	of gene names are going to be related to each other in the
#	output to create a sangerLinks.txt file with three fields:
#	orfName protName description

use strict;
use warnings;
use File::Basename;

my $argc = scalar(@ARGV);

sub usage() {
    printf "usage:\n./%s wormpep140/wormpep.table140 geneIDs.WS140.txt \\\n\twbLongDescr.txt wbGeneClass.txt > sangerLinks.txt 2> dbg.out\n", basename $0;
}

if ($argc != 4) { usage; exit 255; }

my $pepTable = shift;
my $geneIDs = shift;
my $longDescr = shift;
my $geneClassFile = shift;

my %orfToProt;		# key is orfName value is Swiss Prot name
my %orfToGeneClass;	# key is orfName value is GeneClass
my %orfToWBGene;	# key is orfName value is WBGeneNumber
my %orfToShortDescr;	# key is orfName value is short description
my %WBGeneToGeneClass;  # key is WBGeneNumber, value is Gene Class
my %WBGeneToLongDescription;  # key is WBGeneNumber, value is long description
my %GeneClassToWBGene;  # key is Gene Class value is WBGeneNumber
my %GeneClassToConcise;  # key is Gene Class value is short description

my $lineCount=0;

open (FH, "<$pepTable") or die "Can not open $pepTable";

printf STDERR "# ==== processing $pepTable\n";

#	parsing out columns 0, 2, 3 and 5 from the wormpep.table140 file
#	tab separated values
#	column 0 is a fully qualified orf Name, e.g.  >ZC416.8a
#	column 2, when it has a string, is GeneClass, e.g.: unc-17
#	column 3 is a short description, e.g. putative acetylcholine transporter
#	column 5, when it has a string, is a SwissProt ID, e.g.  SW:P34711
#			or TrEMBL ID, e.g.  TR:Q9XWB3
#	creating orfToProt and orfToGeneClass hashes

while (my $line=<FH>) {
    ++$lineCount;
    chomp $line;
    my ($orfName, $ceName, $geneClass, $shortDescr, $confStatus, $swissID, $dummy) =
    	split('\t', $line);
    if (defined($swissID)) {
	if ( ($swissID =~ m/^SW:/) || ($swissID =~ m/^TR:/)) {
	    $swissID =~ s/^SW://;
	    $swissID =~ s/^TR://;
	    if ($lineCount < 25) {
		printf STDERR "#\t%s|%s|%s|%s\n", $orfName, $swissID, $geneClass, $shortDescr;
	    }
	    $orfName =~ s/^>//;
	    if (length($orfName) > 0) {
		if (length($swissID) > 0) {
		    if (exists($orfToProt{$orfName})) {
printf STDERR "ERROR: orf: $orfName already assigned to a protein name: $orfToProt{$orfName}\n";
printf STDERR "ERROR at line: $lineCount in $pepTable\n";
			exit 255;
		    }
		    $orfToProt{$orfName} = $swissID;
		}
		if (length($geneClass) > 0) {
		    if (exists($orfToGeneClass{$orfName})) {
printf STDERR "ERROR: orf: $orfName already assigned to a gene class: $orfToGeneClass{$orfName}\n";
printf STDERR "ERROR at line: $lineCount in $pepTable\n";
			exit 255;
		    }
		    $orfToGeneClass{$orfName} = $geneClass;
		}
		if (length($shortDescr) > 0) {
		    if (exists($orfToShortDescr{$orfName})) {
printf STDERR "ERROR: orf: $orfName already assigned a short descr: $orfToShortDescr{$orfName}\n";
printf STDERR "ERROR at line: $lineCount in $pepTable\n";
			exit 255;
		    }
		    $orfToShortDescr{$orfName} = $shortDescr;
		}
	    }
	}
    }
}
close (FH);

printf STDERR "# ==== processed $lineCount lines from $pepTable\n";

open (FH, "<$geneIDs") or die "Can not open $geneIDs";

printf STDERR "# ==== processing $geneIDs\n";

#	parsing out worm base gene IDs from the file: geneIDs.WS140.txt
#	comma separated values
#	First column is a Worm basse gene number: WBGene00000481
#	Second column is a gene class: cha-1
#	Third column is a simple gene name without extensions: ZC416.8
#	Creating orfToWBGene, WBGeneToGeneClass and GeneClassToWBGene hashes

$lineCount = 0;
while (my $line=<FH>) {
    ++$lineCount;
    chomp $line;
    my ($WBGeneNumber, $fullQualGeneClass, $simpleOrfName) = 
	split('\s*,\s*', $line);
    if ($lineCount < 25) {
	printf STDERR "#\t%s\n", $line;
    }
    if (defined($fullQualGeneClass)) {
	if (length($fullQualGeneClass) > 0) {
	    if (exists($GeneClassToWBGene{$fullQualGeneClass})) {
printf STDERR "WARN: gene class: $fullQualGeneClass already assigned to a WBGeneNumber: $GeneClassToWBGene{$fullQualGeneClass}\n";
printf STDERR "WARN: at line: $lineCount in $geneIDs\n";
	    } else {
		$GeneClassToWBGene{$fullQualGeneClass} = $WBGeneNumber;
	    }
	}
    }
    if (defined($simpleOrfName)) {
	if (length($simpleOrfName) > 0) {
	    if (exists($orfToWBGene{$simpleOrfName})) {
printf STDERR "WARN: orf: $simpleOrfName already assigned to a WBGeneNumber: $orfToWBGene{$simpleOrfName}\n";
printf STDERR "WARN at line: $lineCount in $geneIDs\n";
	    } else {
		$orfToWBGene{$simpleOrfName} = $WBGeneNumber;
	    }
	    if (exists($WBGeneToGeneClass{$WBGeneNumber})) {
    printf STDERR "ERROR: WBGeneNumber: $WBGeneNumber already assigned to a GeneClass: $WBGeneToGeneClass{$WBGeneNumber}\n";
    printf STDERR "ERROR at line: $lineCount in $geneIDs\n";
		exit 255;
	    }
	    $WBGeneToGeneClass{$WBGeneNumber} = $simpleOrfName;
	}
    }
}

printf STDERR "# ==== processed $lineCount lines from $geneIDs\n";
close (FH);

############################# long descriptions

open (FH, "<$longDescr") or die "Can not open $longDescr";

printf STDERR "# ==== processing $longDescr\n";

#	wbLongDescr.txt - WBGene number <tab> long description
#	creating hash WBGeneToLongDescription
#	sometimes descriptions are continued with the same WBGene number
#	this will be recognized and the comments put together under a
#	single WBGene number key 
$lineCount = 0;
my $exampleLines = 0;
while (my $line=<FH>) {
    ++$lineCount;
    chomp $line;
    my ($WBGeneNumber, $longDescr) = split('\t', $line);
    if (defined($longDescr)) {
	if (length($longDescr) > 0) {
	    if (exists($WBGeneToLongDescription{$WBGeneNumber})) {
		my $existingDescription = $WBGeneToLongDescription{$WBGeneNumber};
    $WBGeneToLongDescription{$WBGeneNumber} = $existingDescription .  "\n" .  $longDescr;
	    }
	    $WBGeneToLongDescription{$WBGeneNumber} = $longDescr;
if ( ($exampleLines < 25) && ($longDescr !~ m/NULL/) ) {
    printf STDERR "#\t%s\n", $line;
    ++$exampleLines;
}
	}
    }
}

printf STDERR "# ==== processed $lineCount lines from $longDescr\n";
close (FH);

############################# concise descriptions

open (FH, "<$geneClassFile") or die "Can not open $geneClassFile";

printf STDERR "# ==== processing $geneClassFile\n";

#	wbGeneClass.txt  - two column tab separated
#	simple gene class, no -N extensions <tab> short description

$lineCount = 0;
while (my $line=<FH>) {
    ++$lineCount;
    chomp $line;
    my ($simpleGeneClass, $shortDescr) = split('\t', $line);
    if ($lineCount < 25) {
	printf STDERR "#\t%s\n", $line;
    }
    if (defined($shortDescr)) {
	if (length($shortDescr) > 0) {
	    if (exists($GeneClassToConcise{$simpleGeneClass})) {
printf STDERR "ERROR: GeneClass: $simpleGeneClass already assigned a concise description: $GeneClassToConcise{$simpleGeneClass}\n";
printf STDERR "ERROR at line: $lineCount in $geneClassFile\n";
			exit 255;
	    }
	    $GeneClassToConcise{$simpleGeneClass} = $shortDescr;
	}
    }
}

printf STDERR "# ==== processed $lineCount lines from $geneClassFile\n";
close (FH);

printf STDERR "# ==== now relating fully qualified orfName to protName to description\n";

#	to prevent multiple definitions for the same thing being emitted
my %WBGeneDone;

#	finding the best description in any of several locations.
#	1st and longest descr is in WBGeneToLongDescription{}
#	next best is in orfToShortDescr{}
#	third best is in GeneClassToConcise{}
#	in some cases, no description will be found

foreach my $key (sort(keys %orfToProt)) {
    my $geneClass = "";
    my $shortGeneClass = "";
    if (exists($orfToGeneClass{$key})) {
	$geneClass = $orfToGeneClass{$key};
	$shortGeneClass = $geneClass;
	$shortGeneClass =~ s/-.*//;
    }
    if (exists($orfToWBGene{$key})) {
	my $WBGeneNumber = $orfToWBGene{$key};
	if (exists($WBGeneDone{$WBGeneNumber})) {
printf STDERR "ERROR: already did WBGene: $WBGeneNumber\n";
printf STDERR "%s\t%s\t%s\n", $key, $orfToProt{$key}, $WBGeneNumber;
	    exit 255;
	}
	my $descr = "no descr found primary";
	my $descrOut = "";
	if (exists($WBGeneToLongDescription{$WBGeneNumber})) {
	    $descr = "have long descr primary";
	    $descrOut = $WBGeneToLongDescription{$WBGeneNumber};
	} elsif (exists($orfToShortDescr{$key})) {
	    $descrOut = $orfToShortDescr{$key};
	    $descr = "have short descr primary";
	} elsif (exists($GeneClassToConcise{$shortGeneClass})) {
	    $descr = "have concise descr primary $key";
	    $descrOut = $GeneClassToConcise{$shortGeneClass};
	}
	printf STDERR "%s\t%s\t%s\t%s\t%s\t%s\n", $key, $orfToProt{$key},
		$WBGeneNumber, $geneClass, $shortGeneClass, $descr;
	$descrOut =~ s/NULL//;
	printf "%s\t%s\t%s\n", $key, $orfToProt{$key}, $descrOut;
	$WBGeneDone{$WBGeneNumber} = $key;
    } else {
	my $shortOrf = $key;
	$shortOrf =~ s/[a-z]*$//;
	if (exists($orfToWBGene{$shortOrf})) {
	    my $descr = "no descr found shortOrf";
	    my $descrOut = "";
	    my $WBGeneNumber = $orfToWBGene{$shortOrf};
	    if (exists($WBGeneDone{$WBGeneNumber})) {
		if (exists($orfToShortDescr{$key})) {
		    $descr = "have short descr shortOrf 0";
		    $descrOut = $orfToShortDescr{$key};
		} elsif (exists($GeneClassToConcise{$shortGeneClass})) {
		    $descr = "have concise descr shortGeneClass 0";
		    $descrOut = $GeneClassToConcise{$shortGeneClass};
		} else {
printf STDERR "WARN: shortOrf already did WBGene: $WBGeneNumber: $WBGeneDone{$WBGeneNumber}\n";
printf STDERR "WARN: shortOrf %s\t%s\t%s\n", $key, $orfToProt{$key}, $WBGeneNumber;
		}
    printf STDERR "%s\t%s\t%s\t%s\n", $key, $orfToProt{$key}, $WBGeneNumber, $descr;
		$descrOut =~ s/NULL//;
    printf "%s\t%s\t%s\n", $key, $orfToProt{$key}, $descrOut;
	    } else {
		if (exists($WBGeneToLongDescription{$WBGeneNumber})) {
			$descr = "have long descr shortOrf";
			$descrOut = $WBGeneToLongDescription{$WBGeneNumber};
		} elsif (exists($orfToShortDescr{$key})) {
		    $descr = "have short descr shortOrf 2";
		    $descrOut = $orfToShortDescr{$key};
		} elsif (exists($GeneClassToConcise{$shortGeneClass})) {
		    $descr = "have concise descr shortGeneClass 1";
		    $descrOut = $GeneClassToConcise{$shortGeneClass};
		}
		printf STDERR "%s\t%s\t%s\t%s\n", $key, $orfToProt{$key}, $WBGeneNumber, $descr;
		$descrOut =~ s/NULL//;
		printf "%s\t%s\t%s\n", $key, $orfToProt{$key}, $descrOut;
		$WBGeneDone{$WBGeneNumber} = $key;
	    }
	} else {
printf STDERR "WARNING: orfName: $key not found in WBGene names\n";
	}
    }
}
