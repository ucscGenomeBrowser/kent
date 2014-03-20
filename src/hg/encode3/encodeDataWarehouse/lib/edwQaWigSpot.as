
table edwQaWigSpot
"Information about proportion of signal in a wig that lands under spots in a peak or bed file"
    (
    uint id primary auto; "Id of this wig/spot intersection"
    uint wigId index;	"Id of bigWig file"
    uint spotId index;  "Id of a bigBed file probably broadPeak or narrowPeak"
    double spotRatio; "Ratio of signal in spots to total signal,  between 0 and 1"
    double enrichment;	"Enrichment in spots compared to genome overall"
    bigInt basesInGenome; "Number of bases in genome"
    bigInt basesInSpots; "Number of bases in spots"
    double sumSignal; "Total signal"
    double spotSumSignal; "Total signal in spots"
    )

