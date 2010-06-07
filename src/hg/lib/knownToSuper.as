table knownToSuper
"Map protein superfamilies to known genes"
    (
    string gene;	"Known gene ID"
    int superfamily;	"Superfamily ID"
    int start;	"Start of superfamily domain in protein (0 based)"
    int end;	"End (noninclusive) of superfamily domain"
    float eVal;	"E value of superfamily assignment"
    )
