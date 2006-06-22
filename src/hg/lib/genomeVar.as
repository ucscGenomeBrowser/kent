table genomeVar
"track for mutation data"
    (
    ushort  bin;            "A field to speed indexing"
    string  chrom;          "Chromosome"
    uint    chromStart;     "Start position in chrom"
    uint    chromEnd;       "End position in chrom"
    string  name;           "Official nomenclature description of mutation."
    string  mutId;          "unique ID for this mutation"
    ushort  srcId;          "source ID for this mutation"
    string  baseChangeType; "enum('insertion', 'deletion', 'substitution','duplication','complex','unknown')."
    string  location;       "enum('intron', 'exon', '5'' UTR', '3'' UTR', 'not within known transcription unit')."
    ubyte   coordinateAccuracy; "0=estimated, 1=definite, others?"
    )

table genomeVarSrc
"sources for mutation track"
    (
    ushort srcId;	    "key into genomeVar table"
    string src;		    "name of genome wide source or LSDB"
    string lsdb; 	    "for LSDB name of actual source DB"
    string lsdbAbrev;       "for LSDB abreviation of source name"
    )

table genomeVarAlias
"aliases for mutations"
    (
    string mutId;           "mutation ID from genomeVar table."
    lstring name;           "Another name for the mutation."
    string nameType;	    "common, or ?"
    )

table genomeVarAttr
"attributes asssociated with the mutation"
    (
    string mutId;	    "mutation ID."
    string attrKey;         "attribute name."
    string linkId;          "id for links from this attribute."
    string attrVal;         "value for this attribute"
    )

table genomeVarAttrLink
"links internal or external tied to this attribute"
    (
    string linkId;          "id for attribute link."
    string raKey;           "key into .ra file on how to do link."
    string acc;             "accession or id used by link."
    string displayVal;      "value to display if different from acc."
    )
