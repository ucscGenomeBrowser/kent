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

# $Header: /projects/compbio/cvsroot/kent/src/hg/encode/regionAgp/contigAccession.pl,v 1.5 2005/03/23 01:09:04 kate Exp $

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
    } elsif (/^Monodelphis/) {
        # contig name is cont*, in the 2nd line in the Genbank summary
        # and contig_* in the AGP files from the assembly
        ($contig) = /(cont.*),/;
        $contig =~ s/cont/contig_/;
        $_ = <SUMMARY>;
        ($gi, $num, $gb, $acc) = split /\|/;
        print "$contig\t$acc\n";
    } elsif (/^Bos/) {
        # scaffold name is BtUn_WGA*_1, in the 3nd line in the Genbank summary
        # and SCAFFOLD* in the AGP file
        # accession is also on the 3rd line
        # skip superscaffolds
        if (/super/) {
            # skip superscaffolds
            $_ = <SUMMARY>;
            $_ = <SUMMARY>;
            $_ = <SUMMARY>;
            $_ = <SUMMARY>;
        } else {
            $_ = <SUMMARY>;
            chomp;
            ($gi, $num, $gb, $acc, $contig) = split /\|/;
            $contig =~ s/BtUn_WGA/SCAFFOLD/;
            $contig =~ s/_1.*//;
            print "$contig\t$acc\n";
        }
    } elsif (/^Tetraodon/) {
        # contig name is SCAF*, in the 2nd line in the Genbank summary
        # and the same in the AGP files from the assembly
        # accession is on 3rd or 4th line
        ($contig) = /(SCAF.*),/;
        $_ = <SUMMARY>;
        ($gi, $num, $gb, $acc) = split /\|/;
        if ($gi ne "gi") {
            $_ = <SUMMARY>;
            ($gi, $num, $gb, $acc) = split /\|/;
        }
        print "$contig\t$acc\n";
    } elsif (/^Danio/) {
        # contig name is Zv4_scaffold* or Zv4_NA*, in the 2nd line in the Genbank summary
        # and the same in the AGP files from the assembly
        # accession is on 4th line
        ($contig) = /(Zv4_scaffold.*),/;
        if (!defined($contig)) {
            ($contig) = /(Zv4_NA.*),/;
        }
        $_ = <SUMMARY>;
        $_ = <SUMMARY>;
        ($gi, $num, $gb, $acc) = split /\|/;
        print "$contig\t$acc\n";
    } else {
        die "Unknown WGS project: only know rat, chicken, chimp, dog, opossum, tetra, zfish";
    }
}
