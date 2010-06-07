table expData
"Expression data (no mapping, just spots)"
    (
    string name; "Name of gene/target/probe etc."
    uint expCount; "Number of scores"
    float[expCount] expScores; "Scores. May be absolute or relative ratio"
    )
