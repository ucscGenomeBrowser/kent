#!/usr/local/bin/perl -w
use strict;

# This script takes the liftAll.lft file, which is in NT
# notation and converts it into hs notation, writing
# to the liftHs.lft file as output

my $infile = "/cluster/store2/mm.2001.11/mm1/assembly/mouse.oo.03.agp.fasta";
my $outdir = "/cluster/store2/mm.2001.11/mm1";
my $ext = ".fa";
my $chromo;
my $line;

open(INFILE, "<$infile");

while ($line = <INFILE>) {
    chomp($line);
    $chromo = getChromoName($line);
    processDir($outdir, $chromo);
}

close(INFILE);

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
	$index = index($subLine, " ");

        print "DIRNAME is: " . $dirName . "\n";

	if (substr($subLine, 1, $index) ne $dirName) {
	    die("Invalid directory" . $dirName . "\n");
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
