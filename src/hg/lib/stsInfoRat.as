table stsInfoRat
"Constant STS marker information"
    (
    uint identNo;                 "UCSC identification number"
    string name;                  "Official UCSC name"
    uint RGDId;		   	  "Marker's RGD Id"
    string RGDName;		  "Marker's RGD name"	
    uint UiStsId;		  "Marker's UiStsId"	   
    uint nameCount;		  "Number of alias"
    string alias;		  "alias, or N/A"

    
    string primer1;          	   "primer1 sequence"
    string primer2;          	   "primer2 sequence"
    string distance;               "Length of STS sequence"
    uint sequence;                 "Whether the full sequence is available (1) or not (0) for STS"
    string organis; 		    "Organism for which STS discovered"
   
    string fhhName;	    "Name in FHH_x_ACI map"
    string fhhChr;                     "Chromosome in Genetic map"
    float fhhGeneticPos;               "Position in Genetic map"
    
    string shrspName;	    "Name in SHRSP_x_BN"
    string shrspChr;                     "Chromosome in Genetic map"
    float shrspGeneticPos;               "Position in Genetic map"

    string rhName;	    	    "Name in RH map"
    string rhChr;                     "Chromosome in Genetic map"
    float rhGeneticPos;               "Position in Genetic map."
    float RHLOD;		    "LOD score of RH map"


    string GeneName;               "Associated gene name"
    string GeneID;                 "Associated gene Id"
    string clone;		    "Clone sequence"
	)
