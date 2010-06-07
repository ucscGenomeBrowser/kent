table cdsOrtho
"Information about a CDS region in another species, created by looking at multiple alignment."
    (
    string name; "Name of transcript"
    int start;	 "CDS start within transcript"
    int end;	 "CDS end within transcript"
    string species; "Other species (or species database)"
    int missing; "Number of bases missing (non-aligning)"
    int orthoSize; "Size of orf in other species"
    int possibleSize; "Possible size of orf in other species"
    double ratio;  "orthoSize/possibleSize"
    )
