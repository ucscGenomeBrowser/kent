table rankProp
"RankProp protein ranking for a pair of proteins"
    (
    string qKgId;       "known genes id of query protein"
    string tKgId;       "known genes id of target protein"
    float score;        "rankp score"
    double qtEVal;	"query to target psi-blast E-value"
    double tqEVal;	"target to query psi-blast E-value"
    )
