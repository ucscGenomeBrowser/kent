table transMapGene
"shared, gene-specific transMap information.  This is also a cds specification"
    (
    string id;          "unique transcript id"
    string cds; 	"CDS specification, in NCBI format."
    char[16] db;        "source db"
    string geneName;    "gene name"
    string geneId;      "database-specific gene id"
    )
