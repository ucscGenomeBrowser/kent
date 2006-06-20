table hgMut
"track for human mutation data"
    (
    ushort  bin;            "A field to speed indexing"
    string  chrom;          "Chromosome"
    uint    chromStart;     "Start position in chrom"
    uint    chromEnd;       "End position in chrom"
    string  name;           "HGVS style description of mutation."
    string  mutId;          "unique ID for this mutation"
    ushort  srcId;          "source ID for this mutation"
    char    hasPhenData;    "no longer used"
    string  baseChangeType; "enum('insertion', 'deletion', 'substitution','duplication','complex','unknown')."
    string  location;       "enum('intron', 'exon', '5'' UTR', '3'' UTR', 'not within known transcription unit')."
    ubyte   coordinateAccuracy; "0=estimated, 1=definite, others?"
    )

table hgMutSrc
"sources for human mutation track"
    (
    ushort srcId;	    "key into hgMut table"
    string src;		    "name of genome wide source or LSDB"
    string details; 	    "for LSDB name of actual source DB"
    )

table hgMutExtLink
"accessions and sources for links"
    (
    string mutId;           "mutation ID"
    string acc;             "accession or ID used by link"
    int linkId;             "link ID, foreign key into hgMutLink"
    )

table hgMutLink 
"links for human mutation detail page"
    (
    int linkId;             "ID for this source, links to hgMutExtLink table."
    string linkDisplayName; "Display name for this link."
    string url;             "url to substitute acc in for links."
    )

table hgMutAlias
"aliases for mutations"
    (
    string mutId;           "mutation ID from hgMut table."
    lstring name;           "Another name for the mutation."
    string nameType;	    "common, or ?"
    )

table hgMutAttr
"attributes asssociated with the mutation"
    (
    string mutId;	    "mutation ID."
    int mutAttrClassId;     "id for attribute class or category, foreign key."
    int mutAttrNameId;      "id for attribute name, foreign key."
    int mutAttrLinkId;      "id for links from this attribute."
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
    int mutAttrClassId;    "id for class this name belongs to."
    string mutAttrName;    "name"
    )

table hgMutAttrLink
"links internal or external tied to this attribute"
    (
    int mutAttrLinkId;     "id for attribute link."
    string mutAttrLink;    "key into .ra file on how to do link."
    string mutAttrAcc;     "accession or id used by link."
    )
