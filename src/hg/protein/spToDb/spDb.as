table displayId
"Relate ID and primary accession. A good table to use just get handle on all records. */
    (
    char[6] acc;	"Primary accession"
    char[10] val;	"SwissProt display ID"
    )

table otherAcc
"Relate ID and other accessions"
    (
    char[6] acc;	"Primary accession"
    char[6] val;	"Secondary accession"
    )

table organelle
"A part of a cell that has it's own genome"
    (
    int id;	"Organelle ID - we create this"
    string val;	"Text description"
    )

table singles
"Main SwissProt table - everything that is single instance per record and small"
    (
    char[6] acc;	"Primary accession"
    byte isCurated;	"True if curated (SwissProt rather than trEMBL)"
    int aaSize;		"Size in amino acids"
    int molWeight;	"Molecular weight"
    string createDate;	"Creation date"
    string seqDate;	"Sequence last update date"
    string annDate;	"Annotation last update date"
    int organelle;	"Pointer into organelle table"
    )

table description
"Description lines"
    (
    char[6] acc;	"Primary accession"
    lstring val; 	"SwissProt DE lines"
    )

table geneLogic
"Gene including and/or logic if multiple"
    (
    char[6] acc;	"Primary accession"
    lstring val;	"Gene(s) and logic to relate them."
    )

table gene
"Gene/accession relationship. Both sides can be multiply valued."
    (
    char[6] acc;	"Primary accession"
    string val;		"Single gene name"
    )

table taxon
"An NCBI taxon"
    (
    int id;		"Taxon NCBI ID"
    string binomial;	"Binomial format name"
    lstring toGenus;	"Taxonomy - superkingdom to genus"
    )

table commonName
"Common name for a taxon"
    (
    int taxon;	"Taxon table ID"
    string val; "Common name"
    )

table accToTaxon
"accession/taxon relationship"
    (
    char[6] acc;	"Primary accession"
    int	taxon;		"ID in taxon table"
    )

table keyword
"A keyword"
    (
    int id;	"Keyword ID - we create this"
    string val;	"Keyword itself"
    )

table accToKeyword
"Relate keywords and accessions"
    (
    char[6] acc;	"Primary accession"
    int keyword;	"ID in keyword table"
    }

table commentType
"A type of comment"
    (
    int id;	"Comment type ID, we create this"
    string val;	"Name of comment type"
    )

table commentVal
"Text of a comment"
    (
    int id;	"Comment value ID - we create this"
    string val;	"Text of comment."
    )

table comment
"A structured comment"
    (
    char[6] acc;     "Primary accession"
    int commentType; "ID in commentType table"
    int commentVal;  "ID in commentVal table"
    )

table protein
"Amino acid sequence"
    (
    char[6] acc;	"Primary accession"
    lstring val;	"Amino acids"
    )

table extDb
"Name of another database"
    (
    int id;	"Database id - we make this up"
    string val;	"Name of database"
    )

table extDbRef
"A reference to another database"
    (
    char[6] acc;	"Primary SwissProt accession"
    int extDb;		"ID in extDb table"
    string extAcc;	"External accession"
    int rank;	"Which 1st, 2nd, etc accession - 1 is primary */
    )

table featureClass
"A class of feature"
    (
    int id;	"Database id - we make this up"
    string val;	"Name of class"
    )

table featureType
"A type of feature"
    (
    int id;	"Database id - we make this up"
    string val;	"Name of type"
    )

table feature
"A description of part of a protein"
    (
    char[6] acc;	"Primary accession"
    int start;	"Start coordinate (zero based)"
    int end;	"End coordinate (non-inclusive)"
    int featureClass;	"ID of featureClass"
    int featureType;    "ID of featureType"
    )

table author
"A single author"
    (
    int id;	"ID of this author"
    string val;	"Name of author"
    )

table reference
"An article (or book or patent) in literature."
    (
    int id;	"ID of this reference"
    lstring title; "Title"
    string cite; "Enough info to find journal/patent/etc."
    string pubMed; "Pubmed cross-reference"
    string medline; "Medline cross-reference"
    )

table referenceAuthors
"This associates references and authors"
    (
    int reference;	"ID in reference table"
    int author;		"ID in author table"
    )

table citationRp
"SwissProt RP (Reference Position) line.  Often includes reason for citing."
    (
    int id;	"ID of this citationRp"
    lstring val;	"Reason for citing/position in sequence of cite."
    )

table citation
"A SwissProt citation of a reference"
    (
    int id;		"ID of this citation"
    char[6] acc;	"Primary accession"
    int reference;	"ID in reference table"
    int rp;		"ID in rp table"
    );

table rcType
"Types found in a swissProt reference RC (reference comment) line"
    (
    int id;	"ID of this one"
    string val; "name of this"
    )

table rcVal
"Values found in a swissProt reference RC (reference comment) line"
    (
    int id;	"ID of this"
    string val; "associated text"
    )

table citationRc
"Reference comments associated with citation"
    (
    int citation;	"ID in citation table"
    int rcType;		"ID in rcType table"
    int rcVal;		"ID in rcVal table"
    )

