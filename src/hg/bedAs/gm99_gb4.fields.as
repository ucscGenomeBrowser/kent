table gm99Gb4Map
"Shows the results of GM99-GB4 radiation hybrid mapping"
(
string chrom;	"Reference sequence chromosome or scaffold"
uint chromStart;	"Start position in chrom;
uint chromEnd;	"End position in chrom;
string name;	"Name of STS marker"

uint score;	"Score of a marker - depends on how many contigs it hits"

uint identNo;	"Identification number of STS"
string rhChrom;	"Chromosome number from the RH map - 0 if there is none"
float distance;	"Distance from the RH map"
string ctgAcc;	"Contig accession number"
string otherAcc;	"Accession number of other contigs that the marker hits"
)
