table stsInfoMouse
"Constant STS marker information"
    (
    uint identNo;                 "UCSC identification number"
    string name;                  "Official UCSC name"
   
         
    uint MGIPrimerID;		   "sts Primer's MGI ID or 0 if N/A"
    string primerName;		   "sts Primer's name"
    string primerSymbol;	   "sts Primer's symbol"
    string primer1;          	   "primer1 sequence"
    string primer2;          	   "primer2 sequence"
    string distance;               "Length of STS sequence"
    uint sequence;                 "Whether the full sequence is available (1) or not (0) for STS"
    
   
    uint MGIMarkerID;		    "sts Marker's MGI ID or 0 if N/A"
    string stsMarkerSymbol;           "symbol of  STS marker"
    string Chr;                     "Chromosome in Genetic map"
    float geneticPos;               "Position in Genetic map. -2 if N/A, -1 if syntenic "
    string stsMarkerName;	    "Name of sts Marker."

    uint LocusLinkID;		    "Locuslink Id, 0 if N/A"
    )
