#!/usr/bin/env perl
# gtexMakeEqtlBeds
#
# Create BED files of GTEx eQTL's 

# input files:
#       1. UCSC tissue names            gtexTissue.tab
#       2. GTEx gene names              gtexGeneName.tab
#       2. GTEx variants (rsIds)        gtexVariant.tab
#       3. GTEx eQTLs (effect, pvalue, TSS dist) ../cis/GTEx_Analysis_v6p_eQTL/*_pairs.txt"
#       4. Caviar2 set files (causal set)    CAVIAR_output/<tissue>_Analysis/out/*_set
#       5. Caviar2 post files (probabilities) CAVIAR_output/<tissue>_Analysis/out/*_post
#
# Caviar filenames: file_ENSG*_<tissue>_Analysis_{post,set}
#
# output files:
#       1. UCSC_output/gtexEqtlGeneCluster.bed
#       2. UCSC_output/gtexEqtl<tissue>.bed
#       3. UCSC_output/trackDb.ra

use English;            # for built-in variable names, use awk names
use POSIX;
use feature 'say';      # append newline to prints
use autodie;
use strict;

my $outDir = 'UCSC_output';
if (! -d $outDir) {
    die "Output directory $outDir doesn't exist\n";
}

sub parseGtexVariant {
    # Get BED fields from GTEx format variant identifier:  chr_start_A_ATT_b37
    my ($snp) = @ARG[0];
    my ($chrNum, $start, $ref, $alt, $db) = split('_', $snp);
    my $chrom = "chr$chrNum";
    my $chromStart = $start-1;
    my $lref = length($ref);
    my $size = $lref;
    my $lalt = length($alt);
    if ($lref != $lalt) {
        # indel
        $chromStart += 1;       # skip over initial flanking base
        if ($lref > $lalt) {
            # deletion
            $size = abs($lref - $lalt);
        } else {
            # insertion
            $size = 0;
        }
    }
    my $chromEnd = $chromStart + $size;
    return ($chrom, $chromStart, $chromEnd, $snp);
}

sub assignColorByEffect {
    my ($effect) = @ARG[0];
    # red for +, blue for -.  bright for high effect, pale for low.
    # data range is -9264 to 15362. Q1: -.42  Q3: .33 
    # let's start with threshold of +-2
    # NOTE: these colors are also in hgTracks code for display of clusters
    #   with user-selected tissue sets
    if ($effect == 0.0) {
        return 0x696969;        # gray (mixed-effect, intended for clusters)
    }
    my $cutoff = 2.0;
    if ($effect < 0) {
        # - effect, color blue
        if ($effect < 0 - $cutoff) {
            # high (bright)
            return 0x0000ff;
        } else {
            # low (pale)
            return 0xa0a0ff;
        }
    } else {
        # + effect, color red
        if ($effect > $cutoff) {
            # high (bright)
            return 0xff0000;
        } else {
            # low (pale)
            return  0xffa0a0;
        }
    }
}

#DEBUG
my $lastTis = 'Adipose_Visceral_Omentum';

#########
# Main

$OUTPUT_AUTOFLUSH = 1;  # flush stdout (log) after every print

my $f;

# Get tissue shortLabels
# hgsql hgFixed -Ne 'select name, description, hex(color) from gtexTissue' > gtexTissue.tab
# Replace spaces with underscore and remove parens, 0-pad short values (e.g. Thyroid)
my %tissueNames;
my %tissueColors;
open($f, '<', 'gtexTissue.tab');
while (<$f>) {
    my ($name, $description, $color_hex) = split;
    $tissueNames{$description} = $name;
    my $color_rgb = join(',', map $_, unpack 'C*', pack 'H*', $color_hex); # TYVM stack overflow
    $tissueColors{$description} = $color_rgb;
say "DEBUG colors: $name $description $color_hex $color_rgb";
}
close $f;
say 'DEBUG: got tissues';

# get GTEx gene names 
# hgsql hg19 -Ne 'select name, geneId from gtexGene' > gtexGeneName.tab
my %geneNames;
open($f, '<', 'gtexGeneName.tab');
while (<$f>) {
    my ($name, $id) = split;
    $geneNames{$id} = $name;
}
close $f;
say 'DEBUG: got gene names';

# Get rsID's for hg19 dbSNP142
# Format: chr, start, gtexId, ref, alt, num, rsId
my %rsIds;
open($f, '<', 'gtexVariant.tab');
#open($f, '<', 'testVariant.tab');
my $header = <$f>;
while (<$f>) {
    my ($chrNum, $start, $gtexId, $ref, $alt, $num, $rsId) = split;
    next if $rsId eq '.';      # no rsID in dbSNP142
    $rsIds{$gtexId} = $rsId;
}
close $f;
# DEBUG
my $size = keys %rsIds;
say "DEBUG: got rsIds, size: $size";

# get credible set and probabilities from CAVIAR files

#my %byTissue;
my %byGene;

my @tisDirs = glob("CAVIAR_output/*_Analysis/");
#TODO: command line arg for CAVIAR dist dir
foreach my $dir (@tisDirs) {
    # note: must match hyphen in EBV-transformed
    $dir =~ m/CAVIAR_output\/([\w-]+)_Analysis/;
    my $tis = $1;
say "DEBUG: $tis";

    # get credible set (_set files)
    # format: <snp>
    my @files = glob($dir . "out/*_set"); 
    foreach my $file (@files) {
        $file =~ m/out\/file_(ENSG\d+\.\d+)/;
        my $gene = $1;
    #say "DEBUG: gene: $gene";
        open($f, '<', $file);
        my $count = 0;
        while (<$f>) {
            my ($snp) = split;
    #say "DEBUG: snp: $snp";
            $byGene{$snp}{$gene}{$tis}{'causal'} = 1;
            #$byTissue{$snp}{$tis}{$gene}{'causal'} = 1;
            $count++;
        }
        close $f;
        next if ($count == 0);

        # get probabilities (_post files)
        # format: <snp> <prob>
        $file =~ s/set/post/;
        if (! -e $file) {
            say "WARN: Missing file $file ($count variants in _set)";
            next;
        }
        open($f, '<', $file);
        while (<$f>) {
            my ($snp, $rawProb) = split;
#say "DEBUG: snp $snp gene $gene prob $prob";
            if ($byGene{$snp}{$gene}{$tis}{'causal'}) {
                my $prob = sprintf "%.3f", $rawProb;
#say "DEBUG: SETTING snp $snp gene $gene prob $prob";
                $byGene{$snp}{$gene}{$tis}{'prob'} = $prob;
                #$byTissue{$snp}{$tis}{$gene}{'prob'} = $prob;
            }
        }
        close $f;

    }
    say "DEBUG: got tissue $tis";
    # DEBUG
    #last if $tis eq $lastTis;
}
say 'DEBUG: got CAVIAR set and probabilities';

# get effect sizes from GTEx portal V6p downloads

my @tisFiles = glob("../cis/GTEx_Analysis_v6p_eQTL/*_pairs.txt");
#TODO: command line arg for GTEx eQTL distribution dir

# format variant_id      gene_id tss_distance    pval_nominal    slope   slope_se slope_fpkm   
# GTEx portal uses 'slope' for effect size.  
# I'm currently thinking 'slope_fpkm' is more user-friendly (will check this with Casey Brown)

my $globalMinPval = 0.0;
foreach my $file (@tisFiles) {
    $file =~ m/GTEx_Analysis_v6p_eQTL\/([\w-]+)_Analysis/;
    my $tis = $1;
    say "DEBUG: $tis";
    open($f, '<', $file);
    my $header = <$f>;  # skip header
    while (<$f>) {
        my ($snp, $gene, $dist, $pval, $slope, $slopeSe, $slopeFpkm) = split;
#say "DEBUG: snp $snp  gene $gene effect $slopeFpkm";
        my $count = 0;
        if ($byGene{$snp}{$gene}{$tis}{'causal'}) {
            my $effect = sprintf "%.3f", $slopeFpkm;
            $byGene{$snp}{$gene}{$tis}{'effect'} = $effect;
            my $mlogPval = -log($pval)/log(10);
            $byGene{$snp}{$gene}{$tis}{'pval'} = sprintf "%.3f", $mlogPval;
            if ($mlogPval > $globalMinPval) {
                $globalMinPval = $mlogPval;
            }
            # TSS distance is per gene/snp, but it's in the tissue files, so we're duplicating
            $byGene{$snp}{$gene}{$tis}{'dist'} = $dist;
        }
    }
    close $f;
    say "DEBUG: got tissue $tis";
    # DEBUG
    #last if $tis eq $lastTis;
}
say "DEBUG: got effect, pvalue, and TSS distance for all tissues";

# create tab-sep BED files
$OFS = "\t";
open(my $gf, '>', "$outDir/gtexEqtlGeneCluster.bed");
my %tfs;
foreach my $tis (keys %tissueNames) {
    my $track = "$outDir/gtexEqtlTissue" . ucfirst($tissueNames{$tis});
    open(my $fh, '>', $track . '.bed');
    $tfs{$tis} = $fh;
}
foreach my $snp (keys %byGene) {
    my ($chrom, $start, $end, $name) = parseGtexVariant($snp);
    foreach my $gene (keys %{$byGene{$snp}}) {
        my $geneName = ($geneNames{$gene}) ? $geneNames{$gene} : $gene;
#say "DEBUG: gene: $gene  name: $geneName";
        my @tissues = ();
        my @effects = ();
        my @pvals = ();
        my @probs = ();
        foreach my $tis (keys %{$byGene{$snp}{$gene}}) {
#say "DEBUG: $tis";
            push @tissues, $tis;
        }
        my $tisCount = scalar @tissues;
        next if $tisCount == 0;
        my $rsId = ($rsIds{$snp}) ? $rsIds{$snp} : $name;
#say "DEBUG: snp: $snp rsId: $rsId";
        my $maxProb = 0;
        my $maxEffect = 0.0;
        my $minEffect = 0.0;
        my $minPval = $globalMinPval;
        my $score;
        my @filteredTissues = ();
        my $dist = 0;
        my $color;
        foreach my $tis (sort @tissues) {
            next if !$byGene{$snp}{$gene}{$tis}{'effect'};    # not in LDACC signif set
            my $effect = $byGene{$snp}{$gene}{$tis}{'effect'};
            next if !$byGene{$snp}{$gene}{$tis}{'prob'};      # missing Caviar file
            my $prob = $byGene{$snp}{$gene}{$tis}{'prob'};
            my $pval = $byGene{$snp}{$gene}{$tis}{'pval'};
            $dist = $byGene{$snp}{$gene}{$tis}{'dist'};
            push @filteredTissues, $tis;
            push @effects, $effect;
            push @pvals, $pval;
            push @probs, $prob;
            if ($pval < $minPval) {
                $minPval = $pval;
            }
            if ($prob > $maxProb) {
                $maxProb = $prob;
            }
            if ($effect > 0 && $effect > $maxEffect) {
                $maxEffect = $effect;
            }
            if ($effect < 0 && $effect < $minEffect) {
                $minEffect = $effect;
            }
            # print to per-tissue file (BED 9+5);
            $score = ceil($prob * 1000);
            my $fh = $tfs{$tis};
            $color = assignColorByEffect($effect);
            say $fh $chrom, $start, $end, "${rsId}/${geneName}", $score, ".", $start, $end, $color, 
                        $rsId, $gene, $geneName, $dist, 
                        $effect, $pval, $prob;
        }
        # print to clusters file
        my $tisCount = scalar @filteredTissues;
        next if $tisCount == 0;
        my @tissueLabels;
        foreach my $tis (@filteredTissues) {
            push @tissueLabels, $tissueNames{$tis};
        }
        # TODO: sort tissues after transforming to labels
        my $tisList = join(',', @tissueLabels).',';
        my $effectsList = join(',', @effects).',';
        my $pvalsList = join(',', @pvals).',';
        my $probsList = join(',', @probs).',';
        $score = ceil($maxProb * 1000);

        # get max absolute effect, and whether effect is mixed in cluster
        my $effectType = "+";
        if ($maxEffect > 0.0 && $minEffect < 0.0) {
            # mixed effect
            $effectType = "0";
        } elsif (abs($minEffect) > abs($maxEffect)) {
            # negative effect
            $effectType = "-";
            $maxEffect = $minEffect;    # for output
        }
        say $gf $chrom, $start, $end, $rsId, $score, $gene, $geneName, $dist,
                $maxEffect, $effectType, $minPval,
                $tisCount, $tisList, $effectsList, $pvalsList, $probsList;
    }
}
# write trackDb with tissue-specific files
open(my $tdb, '>', "$outDir/trackDb.gtexEqtl.ra");
foreach my $tis (sort keys %tissueNames) {
    my $tisName = $tissueNames{$tis};
    my $track = 'gtexEqtlTissue' . ucfirst($tisName);
    say $tdb "track $track";
    say $tdb "parent gtexEqtlTissue on";
    say $tdb "shortLabel $tisName";
    say $tdb "longLabel Expression QTL in $tis from GTEx V6";
    say $tdb "color $tissueColors{$tis}";
    say $tdb "idInUrlSql select gene from $track where name='%s'";
    say $tdb "";

    # also close tissue-specific files
    close($tfs{$tis});
}
close($tdb);
close $gf;

say "DONE";
