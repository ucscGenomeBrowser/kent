table refSeqMrna
"Link together a refseq mRNA and other stuff"
    (
    string name;        "Name displayed in UI"
    string product;	"Name of protein product"
    string mrnaAcc;	"mRNA accession"
    string protAcc;	"protein accession"
    uint geneId;	"pointer to geneName table"
    uint prodId;	"pointer to prodName table"
    uint locusLinkId;	"Locus Link ID"
    )
