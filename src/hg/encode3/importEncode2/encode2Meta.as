table series 
"A series of experiments of the same type"
    (
    uint id; "Unique unsigned integer for this series"
    string term; "Unique text identifier for series, starts with wgEncode"
    string dataType; "Type of data - RnaSeq, DnaseSeq, ChipSeq, etc."
    string grantee; "The PI on the grant this was funded by"
    )

table experiment 
"An experiment that might include multiple replicates."
    (
    uint id; "Unique unsigned integer ID for this experiment"
    string updateTime; "Last update time for experiment"
    string series; "Name of series (composite) experiment is part of"
    string accession; "Stable unique string identifier for experiment"
    uint organism; "Key into id field of organism table"
    uint lab; "Key into id field of lab table"
    uint dataType; "Key into id field of dataType table"
    uint cellType; "Key into id field of cellType table"
    uint age; "Key into id field of age table"
    uint antibody; "Key into id field of antibody table"
    uint attic; "Key into id field of attic table"
    uint category; "Key into id field of category table"
    uint control; "Key into id field of control table"
    uint fragSize; "Key into id field of fragSize table"
    uint grantee; "Key into id field of grantee table"
    uint insertLength; "Key into id field of insertLength table"
    uint localization; "Key into id field of localization table"
    uint mapAlgorithm; "Key into id field of mapAlgorithm table"
    uint objStatus; "Key into id field of objStatus table"
    uint phase; "Key into id field of phase table"
    uint platform; "Key into id field of platform table"
    uint promoter; "Key into id field of promoter table"
    uint protocol; "Key into id field of protocol table"
    uint readType; "Key into id field of readType table"
    uint region; "Key into id field of region table"
    uint restrictionEnzyme; "Key into id field of restrictionEnzyme table"
    uint rnaExtract; "Key into id field of rnaExtract table"
    uint seqPlatform; "Key into id field of seqPlatform table"
    uint sex; "Key into id field of sex table"
    uint strain; "Key into id field of strain table"
    uint tissueSourceType; "Key into id field of tissueSourceType table"
    uint treatment; "Key into id field of treatment table"
    version; "If multiple versions, a non-zero version number"
    )
table result 
"A file representing an experimental result."
    (
    uint id; "Unique unsigned integer for this result"
    uint experiment; "Links to id field in experiment table"
    string replicate; "A small positive integer or something like 'both'"
    string view; "The type of data such as 'Signal' or 'Alignments' or 'RawData'"
    string objType; "Always 'file' here. More diverse in metaDb, but we just loaded files"
    string fileName; "File name (no directory)"
    string md5sum; "MD5 sum calculated from file contents, used to make sure data hasn't changed"
    string tableName; "Name of associated table,  always empty string"
    string dateSubmitted; "Date result originally submitted"
    string dateResubmitted; "Date resubmitted or empty string if not resubmitted"
    string dateUnrestricted; "Date people can use in publications without contacting submitting lab"
    )
table cellType
"Cell line or tissue used as the source of experimental material."
    (
    uint id;	"Unique unsigned integer identifier for this item"
    string term;	"A relatively short label, no more than a few words"
    string tag;	"A short human and machine readable symbol with just alphanumeric characters."
    string deprecated;	"If non-empty, the reason why this entry is obsolete."
    lstring description;	"A description up to a paragraph long of plain text."
    string color;	"Red,green,blue components of color to visualize, each 0-255."
    string karyotype;	"Status of chromosomes in cell - usually either normal or cancer."
    string lineage;	"High level developmental lineage of cell."
    lstring orderUrl;	"Web page to order regent."
    string organism;	"Common name of donor organism."
    string protocol;	"Scientific protocol used for growing cells"
    string sex;	"M for male, F for female, B for both, U for unknown."
    string termId;	"ID of term in external controlled vocabulary. See also termUrl."
    string termUrl;	"External URL describing controlled vocabulary."
    uint tier;	"ENCODE cell line tier. 1-3 with 1 being most commonly used, 3 least."
    string tissue;	"Tissue source of sample."
    string vendorId;	"Catalog number of other way of identifying reagent."
    string vendorName;	"Name of vendor selling reagent."
    uint lots;	"The specific lots of reagent used."
    string label;	"A relatively short label, no more than a few words"
    string childOf;	"Name of cell line or tissue this cell is descended from."
    string lab;	"Scientific lab producing data."
    string age;	"Age of donor organism."
    string category;	"Category of cell source - Tissue, primaryCells, etc."
    string strain;	"Strain of organism."
    )

table antibody
"The antibody to a specific protein. Used in immuno-precipitation to target certain fractions of biological interest."
    (
    uint id;	"Unique unsigned integer identifier for this item"
    string term;	"A relatively short label, no more than a few words"
    string tag;	"A short human and machine readable symbol with just alphanumeric characters."
    string deprecated;	"If non-empty, the reason why this entry is obsolete."
    string label;	"A relatively short label, no more than a few words"
    lstring antibodyDescription;	"Short description of antibody itself."
    string lab;	"Scientific lab producing data."
    string lots;	"The specific lots of reagent used."
    string orderUrl;	"Web page to order regent."
    string target;	"Molecular target of antibody."
    string targetClass;	"Classification of the biological function of the target gene"
    lstring targetDescription;	"Short description of antibody target."
    string targetId;	"Identifier for target, prefixed with source of ID, usually GeneCards"
    string targetUrl;	"Web page associated with antibody target."
    lstring validation;	"How antibody was validated to be specific for target."
    string vendorId;	"Catalog number of other way of identifying reagent."
    string vendorName;	"Name of vendor selling reagent."
    )

table mapAlgorithm
"Algorithm used in high-throughput sequencing experiments to map sequenced tags to a particular location in the reference genome."
    (
    uint id;	"Unique unsigned integer identifier for this item"
    string term;	"A relatively short label, no more than a few words"
    string tag;	"A short human and machine readable symbol with just alphanumeric characters."
    string deprecated;	"If non-empty, the reason why this entry is obsolete."
    string description;	"A description up to a paragraph long of plain text."
    string label;	"A relatively short label, no more than a few words"
    )

table readType
"Specific information about cDNA sequence reads including length, directionality and single versus paired read."
    (
    uint id;	"Unique unsigned integer identifier for this item"
    string term;	"A relatively short label, no more than a few words"
    string tag;	"A short human and machine readable symbol with just alphanumeric characters."
    string deprecated;	"If non-empty, the reason why this entry is obsolete."
    string description;	"A description up to a paragraph long of plain text."
    )

table insertLength
"The length of the insertion for paired reads for RNA-seq experiments."
    (
    uint id;	"Unique unsigned integer identifier for this item"
    string term;	"A relatively short label, no more than a few words"
    string tag;	"A short human and machine readable symbol with just alphanumeric characters."
    string deprecated;	"If non-empty, the reason why this entry is obsolete."
    string description;	"A description up to a paragraph long of plain text."
    string label;	"A relatively short label, no more than a few words"
    )

table fragSize
"length of GIS DNA PET fragments, which has different values than fragLength"
    (
    uint id;	"Unique unsigned integer identifier for this item"
    string term;	"A relatively short label, no more than a few words"
    string tag;	"A short human and machine readable symbol with just alphanumeric characters."
    string deprecated;	"If non-empty, the reason why this entry is obsolete."
    string description;	"A description up to a paragraph long of plain text."
    string label;	"A relatively short label, no more than a few words"
    )

table localization
"The cellular compartment from which RNA is extracted. Primarily used by the Transcriptome Project."
    (
    uint id;	"Unique unsigned integer identifier for this item"
    string term;	"A relatively short label, no more than a few words"
    string tag;	"A short human and machine readable symbol with just alphanumeric characters."
    string deprecated;	"If non-empty, the reason why this entry is obsolete."
    string description;	"A description up to a paragraph long of plain text."
    string label;	"A relatively short label, no more than a few words"
    string termId;	"ID of term in external controlled vocabulary. See also termUrl."
    string termUrl;	"External URL describing controlled vocabulary."
    )

table rnaExtract
"Fraction of total cellular RNA selected for by an experiment. This includes size fractionation (long versus short) and feature frationation (PolyA-, PolyA+, rRNA-)."
    (
    uint id;	"Unique unsigned integer identifier for this item"
    string term;	"A relatively short label, no more than a few words"
    string tag;	"A short human and machine readable symbol with just alphanumeric characters."
    string deprecated;	"If non-empty, the reason why this entry is obsolete."
    string description;	"A description up to a paragraph long of plain text."
    string label;	"A relatively short label, no more than a few words"
    )

table promoter
"Target Promoter"
    (
    uint id;	"Unique unsigned integer identifier for this item"
    string term;	"A relatively short label, no more than a few words"
    string tag;	"A short human and machine readable symbol with just alphanumeric characters."
    string deprecated;	"If non-empty, the reason why this entry is obsolete."
    string description;	"A description up to a paragraph long of plain text."
    )

table control
"The type of control (or 'input') used in ChIP-seq experiments to remove background noise before peak calling."
    (
    uint id;	"Unique unsigned integer identifier for this item"
    string term;	"A relatively short label, no more than a few words"
    string tag;	"A short human and machine readable symbol with just alphanumeric characters."
    string deprecated;	"If non-empty, the reason why this entry is obsolete."
    string description;	"A description up to a paragraph long of plain text."
    string label;	"A relatively short label, no more than a few words"
    )

table treatment
"Treatment used as an experimental variable in a series of experiments."
    (
    uint id;	"Unique unsigned integer identifier for this item"
    string term;	"A relatively short label, no more than a few words"
    string tag;	"A short human and machine readable symbol with just alphanumeric characters."
    string deprecated;	"If non-empty, the reason why this entry is obsolete."
    lstring description;	"A description up to a paragraph long of plain text."
    string label;	"A relatively short label, no more than a few words"
    )

table protocol
"Lab specific protocol that may cover a number of steps in an experiment. Most typically this identifies methods for building a DNA or RNA library."
    (
    uint id;	"Unique unsigned integer identifier for this item"
    string term;	"A relatively short label, no more than a few words"
    string tag;	"A short human and machine readable symbol with just alphanumeric characters."
    string deprecated;	"If non-empty, the reason why this entry is obsolete."
    string description;	"A description up to a paragraph long of plain text."
    string label;	"A relatively short label, no more than a few words"
    )

table phase
"The phase in a cell cycle that corresponds to a defined DNA content range determined by flow cytometry measurements that includes G1 (gap phase 1), S phase (the DNA synthesis phase), and G2/M (the gap 2 and mitosis phases; here abbreviated G2). Different portions of S phase are obtained by dividing the cell cycle into seven DNA content windows based on the relative peak positions corresponding to the G1 and G2 cell cycle fractions. The G1 peak position is routinely set at \"60\" relative UV fluorescence, thus producing G2 peak values in the 116-120 range"
    (
    uint id;	"Unique unsigned integer identifier for this item"
    string term;	"A relatively short label, no more than a few words"
    string tag;	"A short human and machine readable symbol with just alphanumeric characters."
    string deprecated;	"If non-empty, the reason why this entry is obsolete."
    string description;	"A description up to a paragraph long of plain text."
    string label;	"A relatively short label, no more than a few words"
    )

table region
"Genomic region(s) targeted by an experiment that is not whole-genome"
    (
    uint id;	"Unique unsigned integer identifier for this item"
    string term;	"A relatively short label, no more than a few words"
    string tag;	"A short human and machine readable symbol with just alphanumeric characters."
    string deprecated;	"If non-empty, the reason why this entry is obsolete."
    string description;	"A description up to a paragraph long of plain text."
    string label;	"A relatively short label, no more than a few words"
    )

table restrictionEnzyme
"The restriction enzyme used in an experiment, typically for DNA library preparation for a high-throughput sequencing experiment."
    (
    uint id;	"Unique unsigned integer identifier for this item"
    string term;	"A relatively short label, no more than a few words"
    string tag;	"A short human and machine readable symbol with just alphanumeric characters."
    string deprecated;	"If non-empty, the reason why this entry is obsolete."
    string description;	"A description up to a paragraph long of plain text."
    string label;	"A relatively short label, no more than a few words"
    )

table view
"Different track formats often allow different views of the data of a single experiment. These views sometimes represent different stages of processing, such as experimental 'signal' resulting directly from high-throughput sequencing and called 'peaks' which result from further analysis."
    (
    uint id;	"Unique unsigned integer identifier for this item"
    string term;	"A relatively short label, no more than a few words"
    string tag;	"A short human and machine readable symbol with just alphanumeric characters."
    string deprecated;	"If non-empty, the reason why this entry is obsolete."
    lstring description;	"A description up to a paragraph long of plain text."
    string label;	"A relatively short label, no more than a few words"
    )

table dataType
"The types of experiments such as ChIP-seq, DNAse-seq and RNA-seq."
    (
    uint id;	"Unique unsigned integer identifier for this item"
    string term;	"A relatively short label, no more than a few words"
    string tag;	"A short human and machine readable symbol with just alphanumeric characters."
    string deprecated;	"If non-empty, the reason why this entry is obsolete."
    string description;	"A description up to a paragraph long of plain text."
    string label;	"A relatively short label, no more than a few words"
    string dataGroup;	"High level grouping of experimental assay type."
    )

table strain
"The strain of the donor organism used in an experiment."
    (
    uint id;	"Unique unsigned integer identifier for this item"
    string term;	"A relatively short label, no more than a few words"
    string tag;	"A short human and machine readable symbol with just alphanumeric characters."
    string deprecated;	"If non-empty, the reason why this entry is obsolete."
    string description;	"A description up to a paragraph long of plain text."
    string label;	"A relatively short label, no more than a few words"
    )

table age
"The age of the organism used to produce tissue or cell line."
    (
    uint id;	"Unique unsigned integer identifier for this item"
    string term;	"A relatively short label, no more than a few words"
    string tag;	"A short human and machine readable symbol with just alphanumeric characters."
    string deprecated;	"If non-empty, the reason why this entry is obsolete."
    string description;	"A description up to a paragraph long of plain text."
    string label;	"A relatively short label, no more than a few words"
    string stage;	"High level place within life cycle of donor organism."
    )

table attic
"Indicates file status as in the attic or not, hosted for download only, it is an internal piece of metaData."
    (
    uint id;	"Unique unsigned integer identifier for this item"
    string term;	"A relatively short label, no more than a few words"
    string tag;	"A short human and machine readable symbol with just alphanumeric characters."
    string deprecated;	"If non-empty, the reason why this entry is obsolete."
    string description;	"A description up to a paragraph long of plain text."
    string label;	"A relatively short label, no more than a few words"
    )

table category
"Cell type category, such as T for tissue, L for cell line, P for primary cells"
    (
    uint id;	"Unique unsigned integer identifier for this item"
    string term;	"A relatively short label, no more than a few words"
    string tag;	"A short human and machine readable symbol with just alphanumeric characters."
    string deprecated;	"If non-empty, the reason why this entry is obsolete."
    string description;	"A description up to a paragraph long of plain text."
    string label;	"A relatively short label, no more than a few words"
    )

table sex
"The sex of a cell line or tissue sample affects the genome target of an experiment."
    (
    uint id;	"Unique unsigned integer identifier for this item"
    string term;	"A relatively short label, no more than a few words"
    string tag;	"A short human and machine readable symbol with just alphanumeric characters."
    string deprecated;	"If non-empty, the reason why this entry is obsolete."
    string description;	"A description up to a paragraph long of plain text."
    )

table objStatus
"The status of the file or table object (revoked, replaced, etc)"
    (
    uint id;	"Unique unsigned integer identifier for this item"
    string term;	"A relatively short label, no more than a few words"
    string tag;	"A short human and machine readable symbol with just alphanumeric characters."
    string deprecated;	"If non-empty, the reason why this entry is obsolete."
    string description;	"A description up to a paragraph long of plain text."
    )

table organism
"The type of organism for an experiment."
    (
    uint id;	"Unique unsigned integer identifier for this item"
    string term;	"A relatively short label, no more than a few words"
    string tag;	"A short human and machine readable symbol with just alphanumeric characters."
    string deprecated;	"If non-empty, the reason why this entry is obsolete."
    string description;	"A description up to a paragraph long of plain text."
    )

table tissueSourceType
"Source of tissue from either an indiviual organism or pooled set of organisms"
    (
    uint id;	"Unique unsigned integer identifier for this item"
    string term;	"A relatively short label, no more than a few words"
    string tag;	"A short human and machine readable symbol with just alphanumeric characters."
    string deprecated;	"If non-empty, the reason why this entry is obsolete."
    string description;	"A description up to a paragraph long of plain text."
    )

table seqPlatform
"Sequencing platform used in high-throughput sequencing experiment."
    (
    uint id;	"Unique unsigned integer identifier for this item"
    string term;	"A relatively short label, no more than a few words"
    string tag;	"A short human and machine readable symbol with just alphanumeric characters."
    string deprecated;	"If non-empty, the reason why this entry is obsolete."
    string description;	"A description up to a paragraph long of plain text."
    string label;	"A relatively short label, no more than a few words"
    string geoPlatformName;	"Short description of sequencing platform. Matches term used by GEO."
    )

table platform
"Platform used in experiment."
    (
    uint id;	"Unique unsigned integer identifier for this item"
    string term;	"A relatively short label, no more than a few words"
    string tag;	"A short human and machine readable symbol with just alphanumeric characters."
    string deprecated;	"If non-empty, the reason why this entry is obsolete."
    string description;	"A description up to a paragraph long of plain text."
    string geoPlatformName;	"Short description of sequencing platform. Matches term used by GEO."
    )

table lab
"The name of the lab producing the data. Often many labs are working together under one grant or one project."
    (
    uint id;	"Unique unsigned integer identifier for this item"
    string term;	"A relatively short label, no more than a few words"
    string tag;	"A short human and machine readable symbol with just alphanumeric characters."
    string deprecated;	"If non-empty, the reason why this entry is obsolete."
    string description;	"A description up to a paragraph long of plain text."
    string label;	"A relatively short label, no more than a few words"
    string grantPi;	"Last name of primary investigator on grant paying for data."
    string labInst;	"The institution where the lab is located."
    string labPi;	"Last name or other short identifier for lab's primary investigator"
    string labPiFull;	"Full name of lab's primary investigator."
    string organism;	"Common name of donor organism."
    )

table grantee
"Principle investigator holding the grant by which a set of experiments are financed. Several labs led by other PI's may be under one grant."
    (
    uint id;	"Unique unsigned integer identifier for this item"
    string term;	"A relatively short label, no more than a few words"
    string tag;	"A short human and machine readable symbol with just alphanumeric characters."
    string deprecated;	"If non-empty, the reason why this entry is obsolete."
    string description;	"A description up to a paragraph long of plain text."
    string grantInst;	"Name of instution awarded grant paying for data."
    string projectName;	"Short name describing grant."
    string label;	"A relatively short label, no more than a few words"
    )

table typeOfTerm
"Types of terms used frequently in controlled vocabulary or metadata should be defined here."
    (
    uint id;	"Unique unsigned integer identifier for this item"
    string term;	"A relatively short label, no more than a few words"
    string tag;	"A short human and machine readable symbol with just alphanumeric characters."
    string deprecated;	"If non-empty, the reason why this entry is obsolete."
    lstring description;	"A description up to a paragraph long of plain text."
    string label;	"A relatively short label, no more than a few words"
    string cvDefined;	"Is there a controlled vocabulary for this term. Is 'yes' or 'no.'"
    string hidden;	"Hide field in user interface? Can be 'yes' or 'no' or a release list"
    string optionalVars;	"Optional fields for a term of this type."
    uint priority;	"Order to display or search terms, lower is earlier."
    string requiredVars;	"Required fields for a term of this type."
    string searchable;	"Describes how to search for term in Genome Browser. 'No' for unsearchable."
    string validate;	"Describes how to validate field typeOfTerm refers to. Use 'none' for no validation."
    )

