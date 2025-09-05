table genomePriority
"listing all UCSC genomes, dbDb or GenArk, with search priority, and any potentila can be requested assembly"
    (
    string name;	"UCSC genome: dbDb name or GenArk/NCBI accession"
    uint priority;	"assigned search priority"
    string commonName;	"a common name"
    string scientificName;	"binomial scientific name"
    uint taxId;	"Entrez taxon ID: www.ncbi.nlm.nih.gov/taxonomy/?term=xxx"
    string clade;	"approximate clade: primates mammals birds fish ... etc ..."
    string description;	"other description text"
    ubyte browserExists;	"1 == this assembly is available at UCSC, 0 == can be requested"
    string hubUrl;      "path name to hub.txt: GCF/000/001/405/GCF_000001405.39/hub.txt"
    )
