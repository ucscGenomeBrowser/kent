#!/usr/bin/env perl

#	$Id: mkContigAgp.pl,v 1.2 2007/08/03 16:39:51 hiram Exp $

use strict;
use warnings;

sub usage() {
print STDERR "usage: mkContigAgp.pl allContigs.agp\n";
print STDERR "\treads seq_contig.md.gz to produce the named file\n";
}

my $argc = scalar(@ARGV);

if ($argc != 1) { usage; exit 255; }
my $contigAgp = shift;

open (CA,">$contigAgp") or die "can not write to $contigAgp";
# select only the sequences for the reference assembly
#open (FH,'zcat seq_contig.md.gz|egrep -v "Celera|129/S|129/O|129S7/S|A/J|unknown|NOD"|') or die "can not read seq_contig.md.gz";
open (FH,'zcat seq_contig.md.gz|grep "C57BL/6J"|') or die "can not read seq_contig.md.gz";

my $chrStart = 1;
my $chrEnd = 1;
my $prevChrEnd = 1;
my $prevChr = "";
my $scafPartCount = 0;
while (my $line=<FH>) {
    next if ($line =~ m/^#/);
    chomp $line;
    my ($tax_id, $chr, $start, $stop, $strand, $name, $id, $type,
	$label, $weight) = split('\s+',$line);
    next if ($name =~ m/start|end/);
    if ($chr !~ m/\|/) {
	my $contigLen = $stop - $start + 1;
	$chr = "M" if ($chr =~ m/MT/);
	if ($prevChr ne $chr) {
	    # first contig does not start at 1, chrom begins with a gap
	    if ($start != 1) {
		my $gap = $start - 1;
		printf CA "chr%s\t1\t%d\t%d\t%s\t%d\t%s\t%s\n",
		    $chr, $start-1, ++$scafPartCount, "N",
			    $gap, "contig", "no";
	    }
	    printf CA "chr%s\t%d\t%d\t%d\t%s\t%s\t%d\t%d\t%s\n",
		$chr, $start, $stop, ++$scafPartCount, $type,
		    $name, 1, $contigLen, $strand;
	    $prevChrEnd = $stop;
	    $prevChr = $chr;
	} else {
	    #	when contigs have gaps between them, indicate such
	    if (($prevChrEnd + 1) != $start) {
		my $gap = $start - $prevChrEnd - 1;
		printf CA "chr%s\t%d\t%d\t%d\t%s\t%d\t%s\t%s\n",
		    $chr, $prevChrEnd+1, $start-1, ++$scafPartCount, "N",
			    $gap, "contig", "no";
	    }
	    printf CA "chr%s\t%d\t%d\t%d\t%s\t%s\t%d\t%d\t%s\n",
		$chr, $start, $stop, ++$scafPartCount, $type,
		    $name, 1, $contigLen, $strand;
	    $prevChrEnd = $stop;
	}
    }
}

close (FH);
close (CA);

__END__
#tax_id chromosome      chr_start       chr_stop        orientation
feature_name    feature_id      feature_type    group_label     weight

have 17 fragments for acc NT_166323.1
1       2750    1       CAAA01120500.1  1       2750    +
2933    44033   2       CAAA01120505.1  1       41101   +
44134   52344   3       CAAA01120513.1  1       8211    +
52451   61934   4       CAAA01120516.1  1       9484    +
62035   83937   5       CAAA01120521.1  1       21903   +
84038   125782  6       CAAA01125558.1  1       41745   +
125981  128964  7       CAAA01120526.1  1       2984    +
130120  132004  8       CAAA01178455.1  1       1885    +
132105  134075  9       CAAA01183294.1  1       1971    +
135593  137948  10      CAAA01159944.1  1       2356    +
144436  154484  11      CAAA01120530.1  1       10049   +
154585  170634  12      CAAA01120534.1  1       16050   +
172416  174379  13      CAAA01120538.1  1       1964    +
175058  177748  14      CAAA01120542.1  1       2691    +
178315  179992  15      CAAA01120546.1  1       1678    +
180093  181389  16      CAAA01097765.1  1       1297    +
182566  186869  17      CAAA01167939.1  1       4304    +


