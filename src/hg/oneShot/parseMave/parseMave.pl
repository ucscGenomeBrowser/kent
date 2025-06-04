# parseMave.pl - parse the dcd_mappings json files provided by MaveDB into a series of bed records,
# where each record is a heatmap of the data for one experiment.  Also writes out a separate fasta
# file containing the reference sequence for each experiment.

use JSON::PP;
use HTML::Entities;
use Encode;
use List::Util qw(min max);


sub cleanWhitespace($)
# There seems to be a variety of weird newline stuff, depending on who curated the record.  There's a few
# dos newlines in records somewhere, others have "\r\n" (like that actual string, not the characters).
# Going to try to replace them all with <br> here, and reduce all other whitespace to single spaces.
{
    my $str = shift;
    $str =~ s/\\r\\n/<br>/g;
    $str =~ s/\\n/<br>/g;
    $str =~ s/(\012|\015)+/<br>/g;
    $str =~ s/\s+/ /g;
    return $str;
}

$jsonBlob = "";
while ($line = <STDIN>)
{
    $jsonBlob .= $line;
}

if ($jsonBlob !~ m/hgvs\.p/)
{
    # no point continuing
    exit(0);
}

$jsonObj = decode_json($jsonBlob);

# Start indexing into the json object to find all of the mapped scores
#
$mapped_scores_ref = $$jsonObj{"mapped_scores"};

$itemStart = -1;
$itemEnd = -1;
$chrom = "";
$urnID = "";

%exonList = ();   # key: "A,B,C", where A and B are the start and end coordinates of the exon in BED coordinates.
                  #      C is the HGVS-like p string for the variant
                  # value: "$thisID $pString: $scoreTrunc", being the id, HGVS-like p. string, and score
                  # The %exonDups list is keyed the same way, containing overflow items for each exon (if any)
%txStarts = ();
%txSizes = ();

foreach $record_ref (@$mapped_scores_ref)
{
    # record_ref should be a hash
    # We'll want "mavedb_id" (example "urn:mavedb:00000097-s-1#2")
    #            "score" (example -0.0757349427170253)
    #            "post_mapped" -> "expressions" -> "value" (example "NP_009225.1:p.Asn1774Thr")
    #               ASSUMING that expressions -> syntax is hgvs.p.
    #               NOTE: expressions is an array, so you'll have to loop through the contents to find the hgvs

    $id = $$record_ref{"mavedb_id"};
    $score = $$record_ref{"score"};
    next if ($score eq "");
    $post = $$record_ref{"post_mapped"};
    $type = $$post{"type"};

    next if ($type eq "Haplotype");  # we're not set up to handle this right now
    next if ($type ne "VariationDescriptor");  # we're not set up to handle anything but VariationDescriptor right now

    $expr = $$post{"expressions"};
    $hgvs = "";
    foreach $expr_i (@$expr)
    {
        if ($$expr_i{"syntax"} eq "hgvs.p")
        {
            $hgvs = $$expr_i{"value"};
        }
    }
    next if ($hgvs eq "");

    # There are some HGVS term types (or HGVS-like terms) that we're skipping over for now.
    # NP_003336.1:p.159delinsHis  is causing a core dump
    # NP_003336.1:p.Ser158_159delinsSerSer is being mapped to
    #       chr16   1324787 1324790 Ser158_159delinsSerSer
    # Seems like that should be more than one codon wide.

    if ($hgvs =~ m/(NP_[^:]+):p\.[a-zA-Z]{3}\d+[a-zA-Z]{3}$/)
        { $ncbiProt = $1; }
    elsif ($hgvs =~ m/(NP_[^:]+):p\.[a-zA-Z]{3}\d+=$/)
        { $ncbiProt = $1; }
    else
        { next; }

    $partialHgvs = $hgvs;
    $partialHgvs =~ s/(\d+)\D+$/$1/;  # strip off the destination AA; we only care about the genome mapping
    if (!defined $coords{$partialHgvs})
    {
        # Using another oneShot tool to convert HGVS terms to BED records
        $bedMapping = `~/bin/x86_64/hgvsToBed hg38 "$hgvs"` or die "hgvsToBed failed on $hgvs; have you built it?";
        $coords{$partialHgvs} = $bedMapping;
    }
    else
    {
        $bedMapping = $coords{$partialHgvs};
    }
    next if ($bedMapping eq ""); # if we can't map it, it's useless.  Discard

    if ($bedMapping !~ m/^\S+\s+\d+\s+\d+/)
    {
        # something weird happened.  Panic.
        die "Something went wrong, hgvs.p $hgvs was mapped to $bedMapping";
    }

    @f = split /\s+/, $bedMapping;
    if ($chrom eq "")
    {
        $chrom = $f[0];
    }
    elsif ($chrom ne $f[0])
    {
        die "Whoa, multiple chromosomes found in file (compare $chrom, " . $f[0] . ")";
    }

    if ($itemStart == -1)
        { $itemStart = $f[1]; }
    elsif ($itemStart > $f[1])
        { $itemStart = $f[1]; }     
    if ($itemEnd == -1)
        { $itemEnd = $f[2]; }
    elsif ($itemEnd < $f[2])
        { $itemEnd = $f[2]; }

    $oldUrnID = $urnID;
    if ($id =~ m/^urn:mavedb:([^#]+)(#\d+)/)
    {
        ($urnID, $thisID) = ($1, $2);
        if ($oldUrnID ne "" && $oldUrnID ne $urnID)
        {
            die "I guess we change urn IDs mid-file now?  $oldUrnID vs $urnID";
        }
    }
    else
    {
        die "Unexpected id string: $id did not contain an urn:mavedb identifier";
    }


    # Right about here, we want to parse f[1] and f[2], which are the start and end of the codon
    # We want to compare that against the exon coordinates against the bounds from the PSL record for
    # the transcript.  Which means we need to get that.
    if (!defined $txStarts{$ncbiProt})
    {
        $st_size = `hgsql hg38 -Ne 'select psl.tStarts, psl.blockSizes from ncbiRefSeqPsl psl join ncbiRefSeqLink link on psl.qName = link.mrnaAcc where link.protAcc = "$ncbiProt"'` or die "No exons for $ncbiProt in ncbiRefSeqLink/ncbiRefSeqPsl";
        chomp $st_size;
        @g = split /\t/, $st_size;
        $txStarts{$NM} = $g[0];
        $txSizes{$NM} = $g[1];
    }
    @thisStarts = split /,/, $txStarts{$NM};
    @thisSizes = split /,/, $txSizes{$NM};
    $thisStart = $f[1];
    $thisEnd = $f[2];
    # hgvsToBed mapped us to this position using the same record that we pulled txStarts and txSizes from,
    # but it ignored the exon structure.  We need to build (maybe) this one "exon" into multiple exons.
    # Going to do this kinda in reverse, actually.  Loop through the transcript's exons.  Either delete
    # them if they have no overlap with the mapped range, or else truncate them to it (by adjusting
    # sizes and maybe starts.
    # Except actual splicing would screw with array offsets, and we don't need that kind of headache.
    # So we'll just set thisStarts[$k] = -1 and filter them out subsequently.

    for ($k=0; $k<@thisStarts; $k++)
    {
        if ($thisStart > $thisStarts[$k] + $thisSizes[$k] || $thisEnd < $thisStarts[$k])
        {
            $thisStarts[$k] = -1;
            next;
        }
        if ($thisStarts[$k] < $thisStart)
        {
            # move the start up and trim the size down
            $thisSizes[$k] -= ($thisStart - $thisStarts[$k]);
            $thisStarts[$k] = $thisStart;
        }
        if ($thisStarts[$k] + $thisSizes[$k] > $thisEnd)
        {
            # trim back the size to fit
            $thisSizes[$k] = $thisEnd - $thisStarts[$k];
        }
    }

    for ($k=0; $k<@thisStarts; $k++)
    {
        next if $thisStarts[$k] == -1;
        $posString = $thisStarts[$k] . "," . ($thisStarts[$k]+$thisSizes[$k]);
        if ($hgvs =~ m/:(.*)/)
        {
            $pString = $1;
            $posString .= ",$pString";
            $scoreTrunc = sprintf("%.4f", $score);
            if (defined $exonList{$posString})
            {
                $exonDups{$posString} .= "<br>$thisID $pString: $scoreTrunc";
            }
            else
            {
                $exonList{$posString} = "$thisID $pString: $scoreTrunc";
            }
        }
        else
        {
            die "Huh, hgvs string didn't have a p term?  $hgvs";
        }
    }
}

# let's check that something actually happened, and skip out if the file was functionally
# empty (e.g. it was only haplotypes)
if (scalar(keys %exonList) == 0)
{
    exit(0);
}


# At this point, %exonList takes position strings (of exons) to the label-type info for them


# need to manually sort the exons at this point
# a bit of a hack here, but numeric sort should sort by the start positions of each exon since
# that's the start of each key in %exonList.

#@sorted_exons = sort {$a <=> $b} (keys %exonList);

# so %coords is actually the list of distinct mappings from partial HGVS terms to bed coordinates
# That makes its size the number of exons that we need.  We can sanity check the ranges in that
# list if we want.
#
# Okaaaay, so that's no longer accurate.  Some of those codon bed mappings may have resulted in multiple
# "exons" because they were split across intron boundaries.  We can recover what we need
# from the keys of %exonList instead.  
# I WAS using coords because it imposed uniqueness on the exon coordinates.  So I have to reimplement that.

%uniquify = ();
foreach $key (keys %exonList)
{
    $bedStr = $key;
    $bedStr =~ m/^(\d+),(\d+),/;
    $st_en = "$1,$2";
    if (!defined $uniquify{$st_en})
    {
        $uniquify{$st_en} = 1;
        $start_end[@start_end] = $st_en;
    }
}
@sorted_pos = sort {$a <=> $b} (@start_end);
$sizeString = "";
$exonStarts = "";
$lasten = "";
$lastst = "";
%exonIxs = ();

for ($i=0; $i<@sorted_pos; $i++)
{
    $sorted_pos[$i] =~ m/(\d+),(\d+)/;
    ($st, $en) = ($1, $2);
    if ($i > 0 && $st < $lasten)
    {
        # something went horribly wrong - one exon started before the previous one ended.
        # Track down the previous coordinates so we can report both sets.
        # inefficient, but at least we don't do it often
        foreach $key (keys %uniquify)
        {
            $bedStr = $uniquify{$key};
            $bedStr =~ m/^(\d+),(\d+)/;
            $st_en = "$1,$2";
            if ($st_en eq "$lastst,$lasten")
            {
                $prevbad = $key;
            }
            if ($st_en eq "$st,$en")
            {
                $thisbad = $key;
            }
        }
        die "One item started at $st ($thisbad) after another ended at $lasten ($prevbad)";
    }
    $width = $en - $st;
    $sizeString .= "$width,";
    $exonStarts .= ($st - $itemStart) . ",";

    $exonIxs{"$st,$en"} = $i;

    $lasten = $en;
    $lastst = $st;
}


$outstring = "$chrom\t$itemStart\t$itemEnd\t$urnID\t1000\t+\t$itemStart\t$itemEnd\t0\t";
$outstring .= scalar(@sorted_pos) . "\t";

##################################################
# Now to assemble the score and label arrays

%aaNames = (
    "Ala" => 'A',
    "Arg" => 'R',
    "Asn" => 'N',
    "Asp" => 'D',
    "Cys" => 'C',
    "Glu" => 'E',
    "Gln" => 'Q',
    "Gly" => 'G',
    "His" => 'H',
    "Ile" => 'I',
    "Leu" => 'L',
    "Lys" => 'K',
    "Met" => 'M',
    "Phe" => 'F',
    "Pro" => 'P',
    "Ser" => 'S',
    "Thr" => 'T',
    "Trp" => 'W',
    "Tyr" => 'Y',
    "Val" => 'V',
    "Ter" => '*',
    "del" => '-'
);

# This takes a single-letter code to the index of the row in the heatmap it appears in
%aaIndex = (
    'A' => 0,
    'V' => 1,
    'L' => 2,
    'I' => 3,
    'M' => 4,
    'F' => 5,
    'Y' => 6,
    'W' => 7,
    'R' => 8,
    'H' => 9,
    'K' => 10,
    'D' => 11,
    'E' => 12,
    'S' => 13,
    'T' => 14,
    'N' => 15,
    'Q' => 16,
    'G' => 17,
    'C' => 18,
    'P' => 19,
    '-' => 20,
    '*' => 21,
    '=' => 22
);

# This should probably be replaced with the keys of the hash, but we want to make sure they
# appear in the right order!
$rowLabels = "A,V,L,I,M,F,Y,W,R,H,K,D,E,S,T,N,Q,G,C,P,-,*,=";

$rowCount = scalar(keys %aaIndex);

@exonScores = ();
for ($i=0; $i < scalar(@sorted_pos)*$rowCount; $i++)
{
    $exonScores[$i] = "";
    $exonLabels[$i] = "";
}


$hasDup = 0;

foreach $exon (keys %exonList)
{
    $exon =~ m/^(\d+),(\d+)/;
    ($exonStart, $exonEnd) = ($1, $2);
    if (defined $exonIxs{"$exonStart,$exonEnd"})
    {
        $exonIx = $exonIxs{"$exonStart,$exonEnd"};
    }
    else
    {
        # shouldn't ever happen, since we just populated exonIxs from the same list
        # that filled exonList
        die "Unexpected exon in the bagging area - $exonStart,$exonEnd vs " . (join ";", keys %exonIxs);
    }

    # now I need to populate the exonScores and exonLabels matrices.  There's a fixed order for the destination AA, so
    # Really I just have to calculate the position in the list as $i + AA_ix*exon_count

    $label = $exonList{$exon};
    if ($label =~ m/#\d+ (\S+): (\S+)/)
    {
        ($hgvs, $score) = ($1, $2);
    }
    else
    {
        die "unexpected label failure: $label";
    }

    if ($hgvs =~ m/\.(\w{3})\d+=$/)
    {
        $destAA = $1;
        $sourceAA = $destAA;
    }
    elsif ($hgvs =~ m/(\w{3})\d+(\w{3})$/)
    {
        $destAA = $2;
        $sourceAA = $1;
    }
    else
    {
        die "unexpected failure to find dest AA: $hgvs";
    }
    $aaLetter = $aaNames{$destAA};
    if (!defined $aaIndex{$aaLetter})
    {
        die "Don't have an index for AA: -$aaLetter- (from $destAA)";
    }
    $aaIx = $aaIndex{$aaLetter};
    $ix = $exonIx + $aaIx*scalar(@sorted_pos);

    if ($destAA ne $sourceAA)
    {
        $exonScores[$ix] = $score;
        $exonLabels[$ix] = $label;

        if (defined $exonDups{$exon})
        {
            $hasDupe = 1;
            if ($exonScores[$ix] !~ m/\|/)
                {
                $exonCount = 1 + scalar(() = $exonDups{$exon} =~ m/<br>/g);  # awkward, but gives the right count
                $exonScores[$ix] .= "|$exonCount";
                }
            $exonLabels[$ix] .= $exonDups{$exon};
        }
    }
    else
    {
        # this is synonymous
        $exonScores[$ix] = "#FFD700";
        $exonLabels[$ix] = "Wild type";

        $aaIx = $aaIndex{"="};
        $ix = $exonIx + $aaIx*scalar(@sorted_pos);

        $exonScores[$ix] = $score;
        $exonLabels[$ix] = $label;

        if (defined $exonDups{$exon})
        {
            $hasDupe = 1;
            if ($exonScores[$ix] !~ m/|/)
                {
                $exonCount = 1 + scalar(() = $exonDups{$exon} =~ m/<br>/g);  # awkward, but gives the right count
                $exonScores[$ix] .= "|$exonCount";
                }
            $exonLabels[$ix] .= $exonDups{$exon};
        }

    }

}

if ($hasDupe)
{
    # just a san check, also sometimes useful for noting which datasets have dups for investigation
    # of whether that bit of mouseover code is working properly
    print STDERR "This one has dups\n";
}

# at this point, we have
# $sizeString : the exon sizes string
# $exonStarts: the exon starts string
# 21 (we know it's the number of rows)
# "A,R,N,D,C,E,Q,G,H,I,L,K,M,F,P,S,T,W,Y,V,*" (this is the row labels)
# @exonScores
# @exonLabels
# <a href="https://mavedb.org/score-sets/urn:mavedb:ID" target=_blank>ID</a>, where ID needs to be replaced with $urnID
#
# All of this is getting appended to $outstring


# sanity check
if (scalar(@exonScores) != $rowCount * scalar(@sorted_pos))
{
    die "exonScores list is wrong, compare " . scalar(@exonScores) . " with " . ($rowCount*scalar(@sorted_pos));
}
if (scalar(@exonScores) != scalar(@exonLabels))
{
    die "mismatch in scores and labels lengths, compare " . scalar(@exonScores) . " with " . scalar(@exonLabels);
}

$outstring .= "$sizeString\t$exonStarts";

# that's the end of the basic bed fields.  Now for the extra heatmap-specific fields

$outstring .= "\t$rowCount\t$rowLabels\t";

# now write out the lower bound, midpoint, and upper bound for the track
# These should be in ascending order

@justScores = grep (/^[^#]/, @exonScores);  # if all positive or negative, the #color values and "" would screw this up
$outstring .= join ",", (min(@justScores), (min(@justScores)+max(@justScores))/2, max(@justScores));
$outstring .= ",\t";

# now the colors for each block

$outstring .= join ",", ("blue", "silver", "red");
$outstring .= ",\t";

# now for the cell scores themselves, along with the cell labels

$outstring .= join ",", @exonScores;
$outstring .= ",\t";
$outstring .= join ",", @exonLabels;
$outstring .= ",\t";

$metadata_ref = $$jsonObj{"metadata"};
$shortDescription = $$metadata_ref{"shortDescription"};
$shortDescription =~ s/\\u(.{4})/chr(hex($1))/ge;  # thanks to https://www.perlmonks.org/?node_id=796799
                                                   # this deals with unicode characters.
$shortDescription = encode_entities($shortDescription);  # this HTML-encodes everything
$shortDescription = cleanWhitespace($shortDescription);
$outstring .= "$shortDescription";

# everything above was mandatory fields for a heatmap (including a string for the heatmap title area)
# What follows next is metadata fields for hgc display

$url = "<a href=\"https://mavedb.org/score-sets/urn:mavedb:ID\" target=_blank>ID</a>";
$url =~ s/ID/$urnID/g;
$outstring .= "\t$url";

$abstract = $$metadata_ref{"abstractText"};
$abstract =~ s/\\u(.{4})/chr(hex($1))/ge;
$abstract = encode_entities($abstract);  # this HTML-encodes everything
$abstract = cleanWhitespace($abstract);
$outstring .= "\t$abstract";


$methodText = $$metadata_ref{"methodText"};
$methodText =~ s/\\u(.{4})/chr(hex($1))/ge;
$methodText = encode_entities($methodText);  # this HTML-encodes everything
$methodText = cleanWhitespace($methodText);

$expRef = $$metadata_ref{"experiment"};
$expMethodText = $$expRef{"methodText"};
if ($expMethodText =~ m/\w+/)
{
    $expMethodText =~ s/\\u(.{4})/chr(hex($1))/ge;
    $expMethodText = encode_entities($expMethodText);  # this HTML-encodes everything
    $expMethodText = cleanWhitespace($expMethodText);

    $methodText .= "<br><br>$expMethodText";
}

$outstring .= "\t$methodText";

$outstring .= "\n";
print "$outstring";

open($seqFile, ">>", "MaveDBSeqFile.fa");
$targetGenesRef = $$metadata_ref{"targetGenes"};
foreach $targetGene_ref (@$targetGenesRef)
{
    $targetGene_name = $$targetGene_ref{"name"};
    $targetSeq_ref = $$targetGene_ref{"targetSequence"};
    $sequence = $$targetSeq_ref{"sequence"};
    print $seqFile ">$urnID $targetGene_name\n";
    print $seqFile "$sequence\n";
}
close($seqFile);
