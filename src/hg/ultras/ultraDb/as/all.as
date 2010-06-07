table udHistory
"Records alterations made to ultra database"
    (
    string who;	"Name of user who made change"
    string program;	"Name of program that made change"
    string time;	"When alteration was made"
    lstring changes;	"CGI-ENCODED string describing changes"
    )

table udPlasmid
"A plasmid sequence"
    (
    string name;	"Name of clone"
    string insert;	"Insert name"
    string vector;	"Vector name"
    string container;	"Plate/box master lives in"
    string well;	"Row/column in container"
    string sequence;	"Primer sequence"
    string target;	"Target sequence name"
    )

table udColony
"Bacterial culture"
    (
    string name;	"Name of colony"
    string plasmid;	"Name of plasmid/bac/etc being grown."
    string strain;	"Bacteria strain"
    )

table udPrimer
"A short sequence used for PCR, sequencing, etc."
    (
    string name;	"Name of primer"
    string sequence;	"Primer sequence"
    string target;	"Target sequence name"
    string container;	"Plate/box master lives in"
    string well;	"Row/column in container"
    )

table udPrimerPair
"A pair of PCR primers"
    (
    string name;	"Name of primer pair"
    string fPrimer;	"Forward primer"
    string rPrimer;	"Reverse primer"
    int expectedSize;	"Expected size of result"
    string expectedSeq;	"Expected sequence"
    )

table udPcrExp
"Info on PCR experiment"
    (
    string name;	"Name of pcr experiment"
    string sequence;	"Sequence (we think)"
    string primerPair;	"Primer pair"
    string template;	"Template DNA"
    string pcrProgram;	"PCR Program name"
    string enzyme;	"Enzyme name"
    float mgCl;		"Magnesium/chloride concentration (mM)"
    byte worked;	"Either +/- or ?"
    lstring comment;	"Comment field"
    string time;	"Time of experiment"
    string who;		"Who did experiment"
    string container;	"plate/box result is in"
    string well;	"Row/column in container"
    )

table udEntryClone
"Information on an entry clone"
    (
    string name;	 "Name of clone"
    string topoPrimers;  "Name of topo primer pair we hope will work"
    string topoPcr;	 "Name of working PCR experiment including topo-engineered primers"
    string topoGoodColonies; "List of colonies that look good after colony PCR"
    )

