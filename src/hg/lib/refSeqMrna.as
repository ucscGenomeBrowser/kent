table refSeqMrna
"A refSeq mRNA and associated info"
    (
    string name;        "Name displayed in UI"
    string mrnaAcc;	"mRNA accession"
    string protAcc;	"protein accession"
    uint geneName;	"pointer to geneName table"
    uint prodName;	"pointer to product name table"
    uint locusLinkId;	"Locus Link ID"
    )
