#!/usr/local/bin/perl 
# Extract WGS contig to accession mapping from Entrez Nucleotide summaries
# Outputs to STDOUT, one line per mapping, in the format:
#       <contig>\t<accession>

# To get the summary file, access the Genbank page for the project by searching:
#       genus[ORGN] AND WGS[KYWD]
# At the end of the page, click on the list of accessions for the contigs.
# Select summary format, and send to file.
# The format of the summaries and the contig naming convention
# differs for each project

# $Header: /projects/compbio/cvsroot/kent/src/hg/encode/regionAgp/contigAccession.pl,v 1.1 2004/10/06 17:07:17 kate Exp $

sub usage() {
    print "usage: contigAccession <summaryfile>\n";
    exit;
}

# process command-line args
$#ARGV ==  0 or &usage;

my $summaryfile = shift @ARGV;

# open summary file
open(SUMMARY, $summaryfile) or die "Couldn't open $summaryFile: $!\n";

my $contig;
my $acc;
my $line;

# print contig/accession pair for each entry
# accession is the 4th field in the gi line, which is pipe-delimited
while (<SUMMARY>) {
    # skip blank lines and initial accession # line
    if (/^$/ or /^\d/) {
        next;
    }
    if (/^Rattus/) {
        # contig name is RNOR*, at end of 2nd or 3rd line
        ($contig) = /(RNOR.*),/;
        if (!defined($contig)) {
            $_ = <SUMMARY>;
            ($contig) = /(RNOR.*),/;
        } else {
            $_ = <SUMMARY>;
        }
        $_ = <SUMMARY>;
        ($gi, $num, $gb, $acc) = split /\|/;
        print "$contig\t$acc\n";
    } elsif (/^Gallus/) {
        # contig name is Cont* in the 2nd line
        ($contig) = /(Cont.*),/;
        # change to convention in the AGP file we received with the assembly
        $contig =~ s/Cont/Contig/;
        $_ = <SUMMARY>;
        ($gi, $num, $gb, $acc) = split /\|/;
        print "$contig\t$acc\n";
    } elsif (/^Pan/) {
        # contig name is ctg_*, in the 2nd line of the Genbank summary
        # and contig_* in the AGP files from the assembly
        ($contig) = /(ctg_.*),/;
        $contig =~ s/ctg/contig/;
        $_ = <SUMMARY>;
        ($gi, $num, $gb, $acc) = split /\|/;
        print "$contig\t$acc\n";
    } elsif (/^Canis/) {
        # contig name is cont_*, in the 2nd line in the Genbank summary
        # and contig_* in the AGP files from the assembly
        ($contig) = /(cont_.*),/;
        $contig =~ s/cont/contig/;
        $_ = <SUMMARY>;
        ($gi, $num, $gb, $acc) = split /\|/;
        print "$contig\t$acc\n";
    } else {
        die "Unknown WGS project: only know rat, chicken, chimp, dog";
    }
}
