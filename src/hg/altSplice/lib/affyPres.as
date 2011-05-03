table affyPres
"Spread sheet data from Lily and Manny"
(
   string probeId;  "Id of probe."
   string info;     "Information associated with gene."
   int valCount;    "Number of experimental values."
   float[valCount] vals; "Values from experiments."
   int callCount;    "Number of calls."
   string[callCount] calls; "Calls for presence or not."
)
