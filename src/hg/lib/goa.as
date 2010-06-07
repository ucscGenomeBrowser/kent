table goa
"GO Association. See http://www.geneontology.org/doc/GO.annotation.html#file"
    (
    string db;	"Database - SPTR for SwissProt"
    string dbObjectId;	"Database accession - like 'Q13448'"
    string dbObjectSymbol;	"Name - like 'CIA1_HUMAN'"
    string notId;  "(Optional) If 'NOT'. Indicates object isn't goId"
    string goId;   "GO ID - like 'GO:0015888'"
    string dbReference; "something like SGD:8789|PMID:2676709"
    string evidence;  "Evidence for association.  Somthing like 'IMP'"
    string withFrom; "(Optional) Database support for evidence I think"
    string aspect;  " P (process), F (function) or C (cellular component)"
    string dbObjectName; "(Optional) multi-word name of gene or product"
    string synonym; "(Optional) field for gene symbol, often like IPI00003084"
    string dbObjectType; "Either gene, transcript, protein, or protein_structure"
    string taxon; "Species (sometimes multiple) in form taxon:9606"
    string date; "Date annotation made in YYYYMMDD format"
    string assignedBy; "Database that made the annotation. Like 'SPTR'"
    )
