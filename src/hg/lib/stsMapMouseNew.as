table stsMapMouseNew
"STS marker and its position on golden path and various maps"
    (
    string chrom;       "Reference sequence chromosome or scaffold"
    int chromStart;     "Start position in chrom - negative 1 if unpositioned"
    uint chromEnd;      "End position in chrom"
    string name;        "Name of STS marker"
    uint score;         "Score of a marker = 1000 when placed uniquely, else 1500/(#placements) when placed in multiple locations"

    uint identNo;       "Identification number of STS"
    string ctgAcc;      "Contig accession number"
    string otherAcc;    "Accession number of other contigs that the marker hits"

    string rhChrom;       "Chromosome (no chr) from RH map or 0 if none"
    float rhPos;          "Position on rh map"
    float rhLod;	  "Lod score of RH map"
    string wigChr;     	  "Chromosome (no chr) from FHHxACI genetic or 0 if none"
    float wigPos;        "Position on FHHxACI map"
    string mgiChrom;        "Chromosome (no chr) from SHRSPxBN geneticmap or 0 if none"
    float mgiPos;           "Position on SHRSPxBN genetic map"
    )
