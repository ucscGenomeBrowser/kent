# Create interact track from Ren lab enhancer/gene map and EPDnew promoters

# TODO: rewrite in C

# usage:  makeInteract map.bed promoters.bed cross-ref.bed outdir/

# creates files in outdir/: 
#       interactions.X.bed
#       enhancers.X.bed
#       promoters.X.bed (colors added)
#
# where X is one of:  all, replicated

# map file (interactions)
# chrom   start   end     ensembl symbol  SCC     Z       p-value (z)     p-value (empirical)
# transform SCC .25-1.0 to score 1-1000
# use symbol as gene name
# drop p-values, ensembl gene ID and Z-score for now

# promoters file
# chrom, start, end, gene

# cross-reference file
# ensg, gene, acc, description

use English;            # for built-in variable names, use awk names
use POSIX;
use feature 'say';      # append newline to prints
use autodie;
use strict;
use List::Util qw(min max);

# color genes: blue red gold purple green from GeneHancer
# then, dark blue, dark red, dark green, orange
my @colorSet = qw(
        0,114,189
        217,83,25
        237,177,32
        126,47,142
        119,172,48,
        0,0,128,
        128,0,0,
        0,128,0,
        255,140,0
);

sub getGeneNameFromPromoterId {
# get gene name from promoter ID (strip trailing _#)

    my ($promoterId) = @_;
    my $geneName = $promoterId;
    $geneName =~ s/_.*//;
    return $geneName;
}

sub getGeneColorFromPromoterInfo {
# get gene color from promoter info string: chrom|start|end|strand|color
    my ($promoterInfo) = @_;
    my ($chrom, $promoterStart, $promoterEnd, $strand, $color) = split('\|', $promoterInfo);
    return $color;
}


# process args
die "usage:  makeInteract map.bed promoters.bed cross-ref.bed outdir/\n" if scalar @ARGV != 4;
my ($mapFile, $promoterFile, $refFile, $outDir) = @ARGV;

# name output files
my ($mapFileAll, $promoterFileAll, $enhancerFileAll) = 
        qw(interactions.all.bed promoters.all.bed enhancers.all.bed);
my ($mapFileRep, $promoterFileRep, $enhancerFileRep) = 
        qw(interactions.rep.bed promoters.rep.bed enhancers.rep.bed);

my %promoters;
my %geneNames;
my %rescues;

open(my $fh, $refFile) or die "Can't open file $refFile\n";
while (<$fh>) {
    chomp;
    my ($ensGene, $geneName, $accession, $description) = split;
    $geneNames{$ensGene} = $geneName;
}
close $fh;

say STDERR "Creating output files in directory $outDir/";

# read promoter file, merge promoters for each gene and assign colors
my $fh;
open($fh, $promoterFile) or die "Can't open file $promoterFile\n";
my @colors = @colorSet;
my $color;
while (<$fh>) {
    chomp;
    my ($promChrom, $promStart, $promEnd, $promId, $score, $strand) = split;
    my $geneName = getGeneNameFromPromoterId($promId);
    my $promoter = $promoters{$geneName};
    if (not defined $promoter) {
        # assign color
        if (@colors == 0) {
            @colors = @colorSet;
        }
        $color = shift @colors;
    } else {
        # merge
        my ($chrom, $start, $end, $strand, $color) = split('\|', $promoter);
        $promStart = min($start, $promStart);
        $promEnd = max($end, $promEnd);
    }
    $promoters{$geneName} = "$promChrom|$promStart|$promEnd|$strand|$color";
}
close $fh;

# read enhancer/gene map (merged reps) and create a hash of enhancer regions, generating an id for each

open($fh, $mapFile) or die "Can't open file $mapFile\n";

my %enhancersAll;
my %enhancersRep;
my $enhancerId = 1;
my $key;
while (<$fh>) {
    chomp;
    my ($interChrom, $enhancerStart, $enhancerEnd, $ensGene, $geneName, $scc, $pval, $count) = split;
    $key = "$interChrom-$enhancerStart-$enhancerEnd";
    if (not exists $enhancersAll{$key}) {
        $enhancersAll{$key} = "e" . $enhancerId++;
    }
}
close $fh;

# create an interaction for each item in map file
my %promotersRep;
my %promotersAll;
open($fh, $mapFile) or die "Can't open file $mapFile\n";
open(my $fhMapAll, '>', "$outDir/$mapFileAll") or die "Can't create file $outDir/$mapFileAll\n";
open(my $fhMapRep, '>', "$outDir/$mapFileRep") or die "Can't create file $outDir/$mapFileRep\n";
my $lastGeneName = 'none';
my $found = 0;
my $missing = 0;
my %missingPromoters;
while (<$fh>) {
    my $line = $_;
    chomp;
    my ($interChrom, $enhancerStart, $enhancerEnd, $ensGene, $geneName, $scc, $pval, $count) = split;
    my $promoter = $promoters{$geneName};
    if (not defined $promoter) {
        # try finding via ensGene and cross-ref file
        $ensGene =~ s/\..*//;       # strip version
        $geneName = $geneNames{$ensGene};
        if (not defined $geneName) {
            $missing++;
            next;
        }
        $promoter = $promoters{$geneName};
        if (not defined $promoter) {
            $missingPromoters{$geneName} = $geneName;
            $missing++;
            next;
        }
        # check valid promoter (must be same chrom)
        my ($chrom, $promoterStart, $promoterEnd, $strand, $color) = split('\|', $promoter);
        if ($chrom ne $interChrom) {
            print STDERR "ERROR: Rejecting promoter for $geneName $ensGene 
                                    on $interChrom ($promoter)\n";
            $missingPromoters{$geneName} = $geneName;
            $missing++;
            next;
        }
        $rescues{$ensGene} = $geneName;
    }
    $found++;
    my ($chrom, $promoterStart, $promoterEnd, $strand, $color) = split('\|', $promoter);
    my $interStart = min($enhancerStart, $promoterStart);
    my $interEnd = max($enhancerEnd, $promoterEnd);
    $key = "$interChrom-$enhancerStart-$enhancerEnd";
    my $enhancerName = $enhancersAll{$key};
    my $interName = "$geneName/$enhancerName";
    my $score = int(($scc * 1200) - 200);
    $lastGeneName = $geneName;

    # TODO: SCC in value column, and use to compute score
    my $color = getGeneColorFromPromoterInfo($promoter);
    if (not defined $color) {
        print STDER "ERROR: color not defined for gene $geneName\n";
        $color = '0,0,0';
    }
    print $fhMapAll "$chrom\t$interStart\t$interEnd\t$interName\t$score\t$scc\t.\t$color\t";
    print $fhMapAll "$chrom\t$enhancerStart\t$enhancerEnd\t$enhancerName\t.\t";
    print $fhMapAll "$chrom\t$promoterStart\t$promoterEnd\t$geneName\t$strand\t$pval\n";
    $promotersAll{$geneName} = 1;

    if ($count == 2) {
        print $fhMapRep "$chrom\t$interStart\t$interEnd\t$interName\t$score\t$scc\t.\t$color\t";
        print $fhMapRep "$chrom\t$enhancerStart\t$enhancerEnd\t$enhancerName\t.\t";
        print $fhMapRep "$chrom\t$promoterStart\t$promoterEnd\t$geneName\t$strand\t$pval\n";
        $promotersRep{$geneName} = 1;
        $enhancersRep{$key} = $enhancersAll{$key};
    }
}
close $fhMapAll;
close $fhMapRep;
close $fh;

# write promoter output files that include gene colors

open($fh, $promoterFile) or die "Can't open file $promoterFile\n";
open(my $fhPromAll, '>', "$outDir/$promoterFileAll") or 
                die "Can't create file $outDir/$promoterFileAll\n";
open(my $fhPromRep, '>', "$outDir/$promoterFileRep") or 
                die "Can't create file $outDir/$promoterFileRep\n";
while (<$fh>) {
    chomp;
    my ($promChrom, $promStart, $promEnd, $promId, $score, $strand, $thickStart, $thickEnd) = split;
    my $geneName = getGeneNameFromPromoterId($promId);
    next if not exists $promotersAll{$geneName};
    my $color = getGeneColorFromPromoterInfo($promoters{$geneName});
    if (not defined $color) {
        print STDERR "Can't find color for gene $geneName\n";
        $color = "0,0,0";
    }
    print $fhPromAll "$promChrom\t$promStart\t$promEnd\t$promId\t$score\t$strand\t";
    print $fhPromAll "$thickStart\t$thickEnd\t$color\n";
    if (exists $promotersRep{$geneName}) {
        print $fhPromRep "$promChrom\t$promStart\t$promEnd\t$promId\t$score\t$strand\t";
        print $fhPromRep "$thickStart\t$thickEnd\t$color\n";
    }
}
close $fhPromAll;
close $fhPromRep;
close $fh;

# write enhancer output files
open(my $fhEnhAll, '>', "$outDir/$enhancerFileAll") or 
                die "Can't create file $outDir/$enhancerFileAll\n";
open(my $fhEnhRep, '>', "$outDir/$enhancerFileRep") or 
                die "Can't create file $outDir/$enhancerFileRep\n";
while (my ($pos, $enhancerName) = each %enhancersAll) {
    my ($chrom, $start, $end) = split('-', $pos);
    say $fhEnhAll "$chrom\t$start\t$end\t$enhancerName";
    if (exists $enhancersRep{$pos}) {
        say $fhEnhRep "$chrom\t$start\t$end\t$enhancerName";
    }
}
close $fhEnhAll;
close $fhEnhRep;

print STDERR "Found $found interactions with promoters, $missing with missing promoters\n";
my $rescues = keys %rescues;
print STDERR "RESCUES: $rescues\n";
print STDERR "$_ $rescues{$_}\n" for (keys %rescues);
my $missingPromoters = keys %missingPromoters;
print STDERR "MISSING PROMOTERS: $missingPromoters\n";
print STDERR "$_\n" for (keys %missingPromoters);

