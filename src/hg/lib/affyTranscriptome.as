table affyTranscriptome
"Affymetrix Transcriptome"
(
	string	chrom;		"Reference sequence chromosome or scaffold"
	uint 	chromStart;	"Start in Chromosome"
	uint	chromEnd;	"End in Chromosome"
	string	name;		"Name"
	uint	score;		"Score"
	char[1]	strand;		"Strand"
	uint	sampleCount;	"Number of points in this record"
	lstring	samplePosition;	"Posititions of the oligos"
	lstring	sampleHeight;	"Intentisy of hybridization"
)
