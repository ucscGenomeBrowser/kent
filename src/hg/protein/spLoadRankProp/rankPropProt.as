table rankPropProt
"Output RankProp protein ranking for a pair of proteins. this is the output of build process, with protein ids, not the table"
    (
    string qSpId;       "swissprot id of query protein"
    string tSpId;       "swissprot id of target protein"
    float score;        "rankp score"
    double qtEVal;	"query to target psi-blast E-value"
    double tqEVal;	"target to query psi-blast E-value"
    )
