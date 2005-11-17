table hgMut
"track for mapping human genotype and phenotype data"
    (
    ushort  bin;            "A field to speed indexing"
    string  chrom;          "Chromosome"
    uint    chromStart;     "Start position in chrom"
    uint    chromEnd;       "End position in chrom"
    string  name;           "HGVS description of mutation."
    string  mutId;          "unique ID for this mutation"
    string  src;            "source for this mutation, put LSDB for locus specific"
    char[1] hasPhenData;    "y or n, does this have phenotype data linked"
    string  baseChangeType; "enum('insertion', 'deletion', 'substitution','duplication','complex','unknown')."
    string  location;       "enum('intron', 'exon', '5'' UTR', '3'' UTR', 'not within known transcription unit')."
    )

table hgMutRef 
"accessions and sources for links"
    (
    string mutId;           "mutation ID"
    string acc;             "accession or ID used by source in link"
    int src;                "source ID, foreign key into hgMutLink"
    )

table hgMutLink 
"links for human phenotype detail page"
    (
    int srcId;              "ID for this source, links to hgMutRef table."
    string linkDisplayName; "Display name for this link."
    string url;             "url to substitute ID in for links."
    )

table hgMutAlias
"aliases for mutations in the human phenotype track"
    (
    string mutId;            "first db ID from hgMut table."
    string name;            "Another name for the mutation."
    )

table hgMutAttr
"attributes asssociated with the mutation"
    (
    string mutId;	    "mutation ID."
    int mutAttrClass;       "id for attribute class or category, foreign key."
    int mutAttrName;        "id for attribute name, foreign key."
    string mutAttrVal;      "value for this attribute"
    )

table hgMutAttrClass
"classes or categories of attributes"
    (
    int mutAttrClassId;     "id for attribute class."
    string mutAttrClass;    "class"
    int displayOrder;       "order to display the classes in on the detail page."
    )

table hgMutAttrName
"Names of attributes"
    (
    int mutAttrNameId;     "id for attribute name."
    string mutAttrName;    "name"
    )
