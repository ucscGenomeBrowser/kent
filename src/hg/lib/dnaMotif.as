table dnaMotif
"A gapless DNA motif"
    (
    string name;			"Motif name."
    int columnCount;			"Count of columns in motif."
    float[columnCount] aProb;		"Probability of A's in each column."
    float[columnCount] cProb;		"Probability of C's in each column."
    float[columnCount] gProb;		"Probability of G's in each column."
    float[columnCount] tProb;		"Probability of T's in each column."
    )


