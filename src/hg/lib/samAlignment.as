table samAlignment
"The fields of a SAM short read alignment, the text version of BAM."
    (
    string qName;	"Query template name - name of a read"
    ushort flag;	"Flags.  0x10 set for reverse complement.  See SAM docs for others."
    string rName;	"Reference sequence name (often a chromosome)"
    uint pos;		"1 based position"
    ubyte mapQ;		"Mapping quality 0-255, 255 is best"
    string cigar;	"CIGAR encoded alignment string."
    string rNext;	"Ref sequence for next (mate) read. '=' if same as rName, '*' if no mate"
    int pNext;		"Position (1-based) of next (mate) sequence. May be -1 or 0 if no mate"
    int tLen;	        "Size of DNA template for mated pairs.  -size for one of mate pairs"
    string seq;		"Query template sequence"
    string qual;	"ASCII of Phred-scaled base QUALity+33.  Just '*' if no quality scores"
    string tagTypeVals; "Tab-delimited list of tag:type:value optional extra fields"
    )
