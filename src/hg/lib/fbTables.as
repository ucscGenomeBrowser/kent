table fbGene
"Links FlyBase IDs, gene symbols and gene names"
    (
    string geneId;	"FlyBase ID"
    string geneSym;	"Short gene symbol"
    string geneName;	"Gene name - up to a couple of words"
    )

table fbTranscript
"Links FlyBase gene IDs and BDGP transcript IDs"
    (
    string geneId;	"FlyBase Gene ID"
    string transcriptId; "BDGP Transcript ID"
    )

table fbSynonym
"Links all the names we call a gene to it's flybase ID"
    (
    string geneId;	"FlyBase ID"
    string name;	"A name (synonym or real)"
    )

table fbAllele
"The alleles of a gene"
    (
    int id;	"Allele ID"
    string geneId;	"Flybase ID of gene"
    string name;	"Allele name"
    )

table fbRef
"A literature or sometimes database reference"
    (
    int id;	"Reference ID"
    lstring text;	"Usually begins with flybase ref ID, but not always"
    )

table fbRole
"Role of gene in wildType"
    (
    string geneId;	"Flybase Gene ID"
    int fbAllele;	"ID in fbAllele table or 0 if not allele-specific"
    int fbRef;		"ID in fbRef table"
    lstring text;	"Descriptive text"
    )

table fbPhenotype
"Observed phenotype in mutant.  Sometimes contains gene function info"
    (
    string geneId;	"Flybase Gene ID"
    int fbAllele;	"ID in fbAllele table or 0 if not allele-specific"
    int fbRef;		"ID in fbRef table"
    lstring text;	"Descriptive text"
    )

table fbGo
"Links FlyBase gene IDs and GO IDs/aspects"
    (
    string geneId;      "Flybase Gene ID"
    string goId;        "GO ID"
    string aspect;      "P (process), F (function) or C (cellular component)"
    )
