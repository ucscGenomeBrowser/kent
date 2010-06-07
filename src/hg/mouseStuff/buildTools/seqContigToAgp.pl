#!/usr/bin/env perl

#	$Id: seqContigToAgp.pl,v 1.3 2007/08/03 16:39:51 hiram Exp $

use strict;
use warnings;

sub usage() {
print STDERR "usage: seqContigToAgp.pl randomFragments.agp randomContigs.agp\n";
print STDERR "\treads allcontig.agp.gz and seq_contig.md.gz to produce\n";
print STDERR "\tthe two named files.\n";
}

my $argc = scalar(@ARGV);

if ($argc != 2) { usage; exit 255; }
my $ranFragsAgp = shift;
my $ranContigsAgp = shift;

my %ctgToAcc;    # key is contig name, value is Accession number
my $ctgCount = 0;
my %ctgFrags;	# key is accession number, value is an array of contig fragments

### Read in the allcontig.agp.gz to get contig name correspondence
### with Accesion number
open (FH,"zcat allcontig.agp.gz|") or die "can not zcat allcontig.agp.gz";
while (my $line = <FH>) {
    next if ($line =~ m/fragment/);
    next if ($line =~ m/^#/);
    chomp $line;
    my ($acc, $start, $end, $item, $type, $ctgName, $ctgStart,
        $ctgEnd, $strand) = split('\s+',$line);
    if (exists($ctgToAcc{$ctgName})) {
        if ($ctgToAcc{$ctgName} ne $acc) {
            print STDERR "ERROR: duplicate ctgName: $ctgName, $acc\n";
            print STDERR "previously: $ctgName, $ctgToAcc{$ctgName}\n";
            exit 255;
        }
    } else {
        $ctgToAcc{$ctgName} = $acc;
	++$ctgCount;
	if (exists($ctgFrags{$acc})) {
	    my $frags = $ctgFrags{$acc};
	    my $fragCount = scalar(@$frags);
	    $frags->[$fragCount] = sprintf("%d\t%d\t%d\t%s\t%s\t%d\t%d\t%s",
		$start, $end, $fragCount+1, $type, $ctgName, $ctgStart, $ctgEnd,
			$strand);
	} else {
	    my @frags;
	    $frags[0] = sprintf("%d\t%d\t1\t%s\t%s\t%d\t%d\t%s",
		$start, $end, $type, $ctgName, $ctgStart, $ctgEnd, $strand);
	    $ctgFrags{$acc} = \@frags;
	}
printf STDERR "# %s\t%s\t%d\t%d\t%d\t%d\t%d\t%s\n", $ctgName, $acc, $ctgCount,
$start, $end, $ctgStart, $ctgEnd, $strand if ($acc =~ m/NT_110857.1|NT_166323.1/);
    }
}
close (FH);

print STDERR "# read in $ctgCount contig names from allcontig.agp.gz\n";

my $frags = $ctgFrags{"NT_166323.1"};
my $fragCount = scalar(@$frags);
printf STDERR  "have $fragCount fragments for acc NT_166323.1\n";
for (my $i = 0; $i < $fragCount; ++$i) {
    printf STDERR "%s\n", $frags->[$i];
}

open (RF,">$ranFragsAgp") or die "can not write to $ranFragsAgp";
open (RC,">$ranContigsAgp") or die "can not write to $ranContigsAgp";
# open (FH,'zcat seq_contig.md.gz|egrep -v "Celera|129/S|129/O|129S7/S|A/J|unknown|NOD"|') or die "can not read seq_contig.md.gz";
open (FH,'zcat seq_contig.md.gz|grep "C57BL/6J"|') or die "can not read seq_contig.md.gz";

my $randomContigCount = 0;
my $seqId = 0;
my $chrStart = 1;
my $chrEnd = 1;
my $chrName = "";
my $partCount = 0;
my $prevChrEnd = 1;
my $prevChrName = "";
my $contigGap = 50000;
my $scafEnd = 1;
my $scafPartCount = 0;
while (my $line=<FH>) {
#    print $line;
    next if ($line =~ m/^#/);
    chomp $line;
    my ($tax_id, $chr, $start, $stop, $strand, $name, $id, $type,
	$label, $weight) = split('\s+',$line);
    next if ($name =~ m/start|end/);
    if ($chr =~ m/\|/) {
	my ($chrN, $acc) = split ('\|', $chr);
	die "accession '$acc' ne name from seq_contig.md '$name'"
		if ($acc ne $name);
	if (!exists($ctgFrags{$name})) {
	    die "can not find accession $name in ctgFrags hash\n";
	}
	my $frags = $ctgFrags{$name}; # reference to array of frags
	my $fragCount = scalar(@$frags);
	if ($chrN ne $prevChrName) {
	    $partCount = 1;
	    $chrStart = $start;
	    $scafEnd = $chrStart + ($stop - $start);
	    ++$randomContigCount;
	    printf RC "chr%s_random\t%d\t%d\t%d\t%s\t%s\t%d\t%d\t%s\n",
		$chrN, $chrStart, $scafEnd, ++$scafPartCount, $type,
			$name, $start, $stop, "+";
	    for (my $i = 0; $i < $fragCount; ++$i) {
		my ($partStart, $partEnd, $dummy, $ctgType, $contigName,
		    $ctgStart, $ctgEnd, $ctgStrand) = split('\t',$frags->[$i]);
		my $ctgLen = $ctgEnd - $ctgStart + 1;
		my $chrLen = $partEnd - $partStart + 1;
		die "contig length $ctgLen != chr len $chrLen"
		    if ($ctgLen != $chrLen);
		if ($i > 0) {
		    my $gapLen = $partStart - $prevChrEnd - 1;
		    if ($gapLen > 0) {
			$chrEnd = $chrStart + $gapLen - 1;
			printf RF "chr%s_random\t%d\t%d\t%d\t%s\t%d\t%s\t%s\n",
			    $chrN, $chrStart, $chrEnd, $partCount++, "N",
				$gapLen, "fragment", "yes";
			$chrStart += $gapLen;
		    } else {
			printf STDERR "# WARNING: zero gap chr%s_random:%d-%d\n",
			    $chrN, $chrStart, $chrEnd;
		    }
		}
		$chrEnd = $chrStart + $chrLen - 1;
		printf RF "chr%s_random\t%d\t%d\t%d\t%s\t%s\t%d\t%d\t%s\n",
		    $chrN, $chrStart, $chrEnd, $partCount++, $ctgType,
			$contigName, $ctgStart, $ctgEnd, $ctgStrand;
		$chrStart += $chrLen;
		$prevChrEnd = $partEnd
	    }
	    $prevChrName = $chrN;
	} else {
	    for (my $i = 0; $i < $fragCount; ++$i) {
		my ($partStart, $partEnd, $dummy, $ctgType, $contigName,
		    $ctgStart, $ctgEnd, $ctgStrand) = split('\t',$frags->[$i]);
		my $ctgLen = $ctgEnd - $ctgStart + 1;
		my $chrLen = $partEnd - $partStart + 1;
		die "contig length $ctgLen != chr len $chrLen"
		    if ($ctgLen != $chrLen);
		if ($i == 0) {
		    $chrEnd = $chrStart + $contigGap - 1;
		    printf RF "chr%s_random\t%d\t%d\t%d\t%s\t%d\t%s\t%s\n",
			$chrN, $chrStart, $chrEnd, $partCount++, "N",
			    $contigGap, "contig", "no";
		    printf RC "chr%s_random\t%d\t%d\t%d\t%s\t%d\t%s\t%s\n",
			$chrN, $chrStart, $chrEnd, ++$scafPartCount,
			    "N", $contigGap, "contig", "no";
		    $chrStart += $contigGap;
		    $scafEnd = $chrStart + ($stop - $start);
		    ++$randomContigCount;
		    printf RC "chr%s_random\t%d\t%d\t%d\t%s\t%s\t%d\t%d\t%s\n",
			$chrN, $chrStart, $scafEnd, ++$scafPartCount,
			    $type, $name, $start, $stop, "+";
		} else {
		    my $gapLen = $partStart - $prevChrEnd - 1;
		    if ($gapLen > 0) {
			$chrEnd = $chrStart + $gapLen - 1;
			printf RF "chr%s_random\t%d\t%d\t%d\t%s\t%d\t%s\t%s\n",
			    $chrN, $chrStart, $chrEnd, $partCount++, "N",
				$gapLen, "fragment", "yes";
			$chrStart += $gapLen;
		    } else {
			printf STDERR "# WARNING: zero gap chr%s_random:%d-%d\n",
			    $chrN, $chrStart, $chrEnd;
		    }
		}
		$chrEnd = $chrStart + $chrLen - 1;
		printf RF "chr%s_random\t%d\t%d\t%d\t%s\t%s\t%d\t%d\t%s\n",
		    $chrN, $chrStart, $chrEnd, $partCount++, $ctgType,
			$contigName, $ctgStart, $ctgEnd, $ctgStrand;
		$chrStart += $chrLen;
		$prevChrEnd = $partEnd;
	    }
	}
#    printf "chr%s_random\t%d\t%d\t%d\t%s\t%s\t%d\t%d\t%s\t%d\n", $chrN, $start, $stop,
#	++$seqId, $type, $name, $start, $stop, $strand, $fragCount;
    }
}

close (FH);
close (RC);
close (RF);

printf STDERR "# found $randomContigCount random contigs\n";

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

