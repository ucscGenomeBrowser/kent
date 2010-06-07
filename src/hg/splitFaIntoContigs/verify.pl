#!/usr/local/bin/perl -w
use strict;

# This script takes the liftAll.lft file, which is in NT
# notation and converts it into hs notation, writing
# to the liftHs.lft file as output

if (1 != $#ARGV) {
    print "Usage: verify.pl genome-file.fa contigDir"
         . "verify.pl - Takes a genome fasta file and a set of contig"
         . " directories and compares them to make sure they contain identical sequence\n";
    exit -1;
}

my $infile = $ARGV[0];
my $outdir = $ARGV[1];
my $ext = ".fa";
my $chromo;
my $line;

print "Verifying genome at $infile against contigs in $outdir\n";

open(INFILE, "<$infile");
while ($line = <INFILE>) {
    chomp($line);
    $chromo = getChromoName($line);
    processDir($outdir, $chromo);
}
close(INFILE);

############### End of main routine

############### Subroutines

sub getChromoName {
    my ($line) = @_;
    my $chromo = substr($line, 4);
    return $chromo;
}

sub processDir {
    my ($chromDir, $chrom) = @_;
    $chromDir = $chromDir . "/" . $chrom;
    print("Processing chromosome " . $chrom . "\n");
    opendir(DIR, $chromDir);

    my @dirList = grep(/^chr$chrom\_[0-9]+$/, readdir(DIR));
    my $dirName;
    my @subFileList;
    my $subLine;
    my $filename;
    my @line;
    my @subLine;
    my $index;
    my @mainChars;
    my $charIndex = 0;

    for $dirName (@dirList) {
	$filename = $chromDir . "/" . $dirName . "/" . $dirName . $ext;
	print("OPENING SUBFILE: " . $filename . "\n");
	open(SUBFILE, $filename);
	$subLine = <SUBFILE>;
	$index = index($subLine, " ") - 1;

        print "DIRNAME is: " . $dirName . "\n";

	if (substr($subLine, 1, $index) ne $dirName) {
	    die("Invalid directory: " . $dirName . "\n");
	}

        my @subChars;
        my $subChar;
        my $mainChar;
           

        # Read file and compare to main file in order 
        while ($subLine = <SUBFILE>) {
	    chomp($subLine);
            #print "SUBLINE: \"" . $subLine . "\"\n";
            @subChars = split(//, $subLine);

            foreach $subChar (@subChars) {
                if (!(@mainChars) || $charIndex > $#mainChars) {
                    chomp($line = <INFILE>);
                    #print "LINE: \"" . $line . "\"\n";
                    @mainChars = split(//, $line);
                    $charIndex = 0;
                }

                $mainChar = $mainChars[$charIndex];

                if (uc($mainChar) ne uc($subChar)) {
                    print "LINE: \"" . $line . "\"\n";
                    print "SUBLINE: \"" . $subLine . "\"\n";
                    print "\"" . $subChar . "\"=\"" . $mainChar . "\" ";
                    print("\nFILES DON'T MATCH\n");
                    exit;
                }    

                $charIndex++;
            }   
	}        
    }
}
