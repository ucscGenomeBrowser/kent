table stsInfoMouseNew
"Constant STS marker information"
    (
    uint identNo;                 "UCSC identification number"
    string name;                  "Official UCSC name"
    uint MGIId;		   	  "Marker's MGI Id"
    string MGIName;		  "Marker's MGI name"	
    uint UiStsId;		  "Marker's UiStsId"	   
    uint nameCount;		  "Number of alias"
    string alias;		  "alias, or N/A"

    
    string primer1;          	   "primer1 sequence"
    string primer2;          	   "primer2 sequence"
    string distance;               "Length of STS sequence"
    uint sequence;                 "Whether the full sequence is available (1) or not (0) for STS"
    string organis; 		    "Organism for which STS discovered"
   
    string wigName;	    "WI_Mouse_Genetic map"
    string wigChr;                     "Chromosome in Genetic map"
    float wigGeneticPos;               "Position in Genetic map"
    
    string mgiName;	    "MGI map"
    string mgiChr;                     "Chromosome in Genetic map"
    float mgiGeneticPos;               "Position in Genetic map"

    string rhName;	    	    "WhiteHead_RH map"
    string rhChr;                     "Chromosome in Genetic map"
    float rhGeneticPos;               "Position in Genetic map."
    float RHLOD;		    "LOD score of RH map"


    string GeneName;               "Associated gene name"
    string GeneID;                 "Associated gene Id"
    string clone;		    "Clone sequence"
	)
