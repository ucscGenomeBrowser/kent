table mapSts
"An STS based map in relationship to golden path"
(
string chrom;	"Reference sequence chromosome or scaffold"
uint chromStart; "Start position in chrom"
uint chromEnd;	"End position in chrom"
string name;	"Name of STS marker"

uint score;	"Score of a marker - depends on how many contigs it hits"

uint identNo;	"Identification number of STS"
string rhChrom;	"Chromosome number from the RH map - 0 if there is none"
float distance;	"Distance from the RH map"
string ctgAcc;	"Contig accession number"
string otherAcc;	"Accession number of other contigs that the marker hits"
)
