table hprc2Arrange
"HPRC2 assembly rearrangements and indels relative to GRCh38"
    (
    string chrom;       "Chromosome (or contig, scaffold, etc.)"
    uint   chromStart;  "Start position in chromosome"
    uint   chromEnd;    "End position in chromosome"
    string name;        "Name of item"
    uint   score;       "Number of HPRC2 assemblies with this event"
    char[1] strand;     "+ or -"
    uint thickStart;    "Start of where display should be thick"
    uint thickEnd;      "End of where display should be thick"
    uint itemRgb;       "Color"
    lstring source;     "List of assemblies with this event"
    uint querySize;     "Size of item on assembly (query) side"
    string label;       "Label for item"
    uint size;          "Event size (bp)|Deletion/inversion/duplication: span on GRCh38. Insertion: bases inserted from the assembly."
    lstring _mouseover; "mouseOver for item"
    )
