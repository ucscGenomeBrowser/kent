table transMapGene
"shared, gene-specific transMap information.  This is also a cdsSpec object"
    (
    string id;          "unique sequence id"
    string cds; 	"CDS specification, in NCBI format."
    char[16] db;        "source db"
    string geneName;    "gene name"
    )
