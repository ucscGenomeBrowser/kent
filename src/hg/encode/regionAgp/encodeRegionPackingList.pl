#!/usr/local/bin/perl 
# create packing list for a set of organism AGP files

# $Header: /projects/compbio/cvsroot/kent/src/hg/encode/regionAgp/encodeRegionPackingList.pl,v 1.4 2004/10/06 17:07:17 kate Exp $

sub usage() {
    print "usage: encodePackingList <bedfile> <agpdir> <org> <scientific name> <taxon ID> <strain> <freezedate> <db> <build id> [comment]\n", 
"               where: freeze date is MMM-YYYY\n", 
"                      db is UCSC build\n", 
"                      build id is NCBI (or other source) build\n";
    exit;
}

# process command-line args
$#ARGV ==  8  or $#ARGV == 9 or &usage;

my $bedFile = shift @ARGV;
my $agpDir = shift @ARGV;

my $organism = shift @ARGV;
my $scientificName = shift @ARGV;
my $speciesId = shift @ARGV;
my $strain = shift @ARGV;

my $freezeDate = shift @ARGV;
my $ucscId = shift @ARGV;
my $sourceId = shift @ARGV;

my $comment = shift @ARGV;

# open BED file of regions
open(BED, $bedFile) or die "Couldn't open $bedFile: $!\n";

# determine number of parts for each region
my %regions = ();
while (<BED>) {
    # skip comment lines
    if (/^#/) {
        next;
    }

    ($chrom, $chromStart, $chromEnd, $region, $seqNum) = split;
    if (!defined($seqNum) or $seqNum !~ /\d+/) {
        die "Bad format BED line: $_";
    }

    # save in hash
    if (defined $regions{$region}) {
        $regions{$region}++;
    } else {
        $regions{$region} = 1;
    }
}

# rewind BED file
seek(BED, 0, 0);

# creating packing list entry for each line
while (<BED>) {
    # skip comment lines
    if (/^#/) {
        next;
    }
    ($chrom, $chromStart, $chromEnd, $region, $seqNum) = split;
    $chrom =~ s/chr//;

    my $seqName = join('_', $organism, $region, $seqNum);

    # BED file is 1/2-open coords, adjust chromStart to match AGP
    $chromStart++;

    my $length = $chromEnd - $chromStart + 1;

    my $agpFile = "$agpDir/$seqName.agp";
    if (! -e $agpFile) {
        die "ERROR: $agpFile doesn't exist\n";
    }
    print 
        "Region:\t$region\n",
        "Part:\t$seqNum of $regions{$region}\n",
        "TaxID:\t$speciesId\n",
        "Strain:\t$strain\n",
        "SeqName:\t$seqName\n",
        "SeqLength:\t$length\n",
        "AgpFile:\t$seqName.agp\n",
        "Organism:\t$scientificName\n",
        "FreezeDate:\t$freezeDate\n",
        "SourceID:\t$sourceId\n",
        "UcscID:\t$ucscId\n",
        "Chromosome:\t$chrom\n",
        "ChromStart:\t$chromStart\n",
        "ChromEnd:\t$chromEnd\n";
    if (defined($comment)) {
        print "Comment:\t$comment\n";
    }
    print "//\n";
}

