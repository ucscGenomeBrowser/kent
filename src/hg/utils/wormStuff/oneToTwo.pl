#!/usr/bin/env perl

#	$Id: oneToTwo.pl,v 1.2 2007/08/03 16:41:12 hiram Exp $

use strict;
use warnings;

sub usage() {
    print STDERR "usage: ./oneToTwo.pl newPep.tab\n";
    print STDERR "\treads from sangerPep.names.original.txt, sangerCanonical.transcript.txt,\n";
    print STDERR "\tand ../sangerGene/sangerPep.tab\n";
    print STDERR "\twrites new peptide file to given file name newPep.tab\n";
}

my $argc = scalar(@ARGV);

if ($argc != 1) { usage; exit 255; }
my $newPepFile = shift;

my %pepNames;	# key is pepName - value is unimportant
open (FH, "<sangerPep.names.original.txt") or
	die "can not read sangerPep.names.original.txt";
while (my $line = <FH>) {
    chomp $line;
    die "duplicate pepName $line" if (exists($pepNames{$line}));
    $pepNames{$line} = 1;
}
close (FH);

my %canonicalNames;	# key is existing canonical name
my %oneDotLess;		# key is canonical name without 2nd dot suffix
my %newCanonicalNames;  # key is new two dot name

my $threeDotCount = 0;
my $canonicalCount = 0;
open (FH, "<sangerCanonical.transcript.txt") or
	die "can not read sangerCanonical.transcript.txt";
while (my $line = <FH>) {
    chomp $line;
    my ($one, $two, $three, $four, $five, $rest) = split('\.',$line,6);
    die "found four dot name $line" if (defined($four));
    die "duplicate canonical name" if (exists($canonicalNames{$line}));
    $canonicalNames{$line} = 1;
    my $twoDot = "";
    if (defined($two)) {
	$twoDot = sprintf "%s.%s", $one, $two;
    } else {
	$twoDot = sprintf "%s", $one;
    }
    if (defined($three)) {
	++$threeDotCount;
    }
    die "duplicate new two dot name $line" if (exists($newCanonicalNames{$twoDot}));
    $newCanonicalNames{$twoDot} = $line;
    die "duplicate two dot name $line" if (exists($oneDotLess{$twoDot}));
    $oneDotLess{$twoDot} = $line;
    ++$canonicalCount;
}
close (FH);

printf STDERR "found $threeDotCount three dot names\n";
printf STDERR "found $canonicalCount names\n";

die "stop here";

foreach my $key (keys %oneDotLess) {
    my $fullName = $oneDotLess{$key};
    if ($fullName ne $key) {
	print STDERR "$key <-\t$fullName\n";
    }
    if (!exists($pepNames{$key})) {
	print STDERR "warning no peptide for canonical name $key\t$fullName\n";
    }
}

foreach my $key (keys %pepNames) {
    print STDERR "warning peptide name $key not in canonical\n"
	if (!exists($oneDotLess{$key}));
}

my %peptides;	#	key is peptide name, value is protein sequence
open (FH,"<../sangerGene/sangerPep.tab") or
	die "can not read ../sangerGene/sangerPep.tab";

my $addedNewNames = 0;

while (my $line = <FH>) {
    chop $line;
    my ($name, $pep) = split('\s+',$line);
    die "duplicate name in sangerPep.tab" if (exists($peptides{$name}));
    my $newName = $name;
    if (exists($oneDotLess{$name})) {
	$newName = $oneDotLess{$name};
    }
    die "duplicate peptide newName $newName" if (exists($peptides{$newName}));
    die "duplicate peptide name $name" if (exists($peptides{$name}));
    $peptides{$name} = $pep;
    if ($name ne $newName) {
	$peptides{$newName} = $pep;
	++$addedNewNames;
    }
}
close (FH);

print STDERR "added $addedNewNames new peptide names\n";

open (FH,">$newPepFile") or die "can not write to $newPepFile";
foreach my $key (keys %peptides) {
    printf FH "%s\t%s\n", $key, $peptides{$key};
}
close (FH);
