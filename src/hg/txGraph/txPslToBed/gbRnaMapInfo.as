
table gbRnaMapInfo
"Summary of how a genbank transcript maps.  Most fields show . if FALSE, a shortened version of field name if true, just for better readability."
    (
    string chrom;	"Chromosome of alignment"
    int chromStart;	"Start of alignment"
    int chromEnd;	"End of alignment"
    string accession;	"Genbank accession"
    string genbankCds;	"True if came with a genbank CDS record"
    string genbankFullCds; "True if genbank CDS record showed full length"
    string intronNudged;	"True if moved alignment a little to get gt/ag ends"
    string directionFlipped; "True if flipped strand based on splice sites"
    string mergedSmallGap; "True if merged a small gap in alignment into exon"
    string splitLargeGap; "True if split based on a large gap in the alignment"
    string splitLargGapInCds; "True if split large gap in CDS region"
    string smallGapBustsFrame; "True if small gap in alignment breaks frame"
    string largeGapBustsFrame; "True if a large gap in alignment breaks frame"
    string intronNudgeBustsFrame;	"True if we broke frame nudging intron"
    string cdsOutside;	"True if cds outside of bed"
    string outputCds;	"True if output with CDS"
    )
