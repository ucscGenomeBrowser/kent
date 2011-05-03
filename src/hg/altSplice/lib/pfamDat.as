table pfamHit
"Data about a pfam global hit."
(
 string model;    "Model hit with sequence."
 string descript; "Description of Model hit with sequence."
 double score;    "Bit score of hit."
 double eval;     "E-values of hit."
 uint numTimesHit; "Number of times hit was found in sequence."
 )

table pfamDHit
"Data about a pfam domain hit."
(
 string model;    "Model of domain."
 int domain;      "Domain number of hit."
 int numDomain;   "Number of domains for a hit."
 uint seqStart;   "Start in sequence of hit."
 uint seqEnd;     "End in sequence of hit."
 string seqRep;   "String representation of where hit is located in seq, '[.','..','.]','[]'"
 uint hmmStart;   "Start in profile hmm of hit."
 uint hmmEnd;     "End in profile hmm of hit."
 string hmmRep;   "String representation of where hit is located in seq, '[.','..','.]','[]'"
 double dScore;   "Score for domain hit."
 double dEval;    "Evalue for domain."
 string alignment; "Text based alignment."
 )
 

table pfamDat
"Structure to hold results of one hmmpfam run. Distributed by Sean Eddy. See http://www.genetics.wustl.edu/eddy/software/"
(
 string seqName; "Sequence name."
 string sequence; "Sequence run against library."
 object pfamHit pfamHitList;  "Global hits."
 object pfamDHit pfamDHitList; "Domain hits."
)
 
