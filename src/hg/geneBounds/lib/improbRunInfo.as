table improbRunInfo
"Information on one Improbizer run plus motifs."
    (
    string name;			"Motif name."
    float runScore;		"Score of motifFound."
    float bestControlScore;	"Score of best control run."
    float meanControlScore;	"Mean of control runs."
    float sdControlScore;	"Control run standard deviation."
    int seqCount;		"Number of sequences used to generate it."
    string consensus;		"Motif consensus sequence."
    float runPos;		"Position of motif in run."
    float runPosSd;		"Standard deviation of position in run."
    int columnCount;		"Count of columns in motif."
    float[columnCount] aProb;		"Probability of A's in each column."
    float[columnCount] cProb;		"Probability of C's in each column."
    float[columnCount] gProb;		"Probability of G's in each column."
    float[columnCount] tProb;		"Probability of T's in each column."
    int controlCount;		"Count of controls."
    float[controlCount] controlScores;	"Scores of controls."
    )


