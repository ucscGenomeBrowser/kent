table refLink
"Link together a refseq mRNA and other stuff"
    (
    string name;        "Name displayed in UI"
    string product;	"Name of protein product"
    string mrnaAcc;	"mRNA accession"
    string protAcc;	"protein accession"
    uint geneName;	"pointer to geneName table"
    uint prodName;	"pointer to prodName table"
    uint locusLinkId;	"Entrez ID (formerly LocusLink ID)"
    uint omimId;	"OMIM ID"
    )
