table mcnBreakpoints
"Chromosomal breakpoints from the MCN Project"
        (
        string chrom;    "Reference sequence chromosome or scaffold"
        uint   chromStart;  "Start position in genoSeq"
        uint   chromEnd;    "End position in genoSeq"
        string name;       "Shortened Traitgroup Name"
        uint score;     "Always 1000 for now"
	string caseId;	"MCN Case ID"
	string bpId;	"MCN Breakpoint ID"
	string trId;	"MCN Trait ID"
	string trTxt;   "MCN Trait name"
	string tgId;	"MCN Traitgroup ID"
	string tgTxt;	"MCN Traitgroup Name"
   )
