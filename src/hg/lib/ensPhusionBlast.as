table ensPhusionBlast
"Ensembl PhusionBlast Synteny (bed 6 +)."
    (
    string chrom;        "$Organism chromosome"
    uint   chromStart;   "Start position in $Organism chromosome"
    uint   chromEnd;     "End position in $Organism chromosome"
    string name;         "Name of $Xeno chromosome"
    uint   score;        "Integer alignment score"
    char[1] strand;      "Direction of alignment: + or -"
    uint   xenoStart ;   "Start position in $Xeno chromosome"
    uint   xenoEnd;      "End position in $Xeno chromosome"
    )
