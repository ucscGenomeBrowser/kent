table cutter
"Restriction Enzymes"
(
   string name;                   "Name of Enzyme"
   uint size;                     "Size of recognition sequence"
   uint matchSize;                "size without N's"
   string seq;                    "Recognition sequence"
   uint cut;                      "Cut site on the plus strand"
   int overhang;                  "Overhang size and direction"
   ubyte palindromic;             "1 if it's panlidromic, 0 if not."
   ubyte semicolon;               "REBASE record: 0 if primary isomer, 1 if not."
   uint numSciz;                  "Number of isoschizomers"
   string[numSciz] scizs;         "Names of isoschizomers"
   uint numCompanies;             "Number of companies selling this enzyme"
   char[numCompanies] companies;  "Company letters"
   uint numRefs;                  "Number of references"
   uint[numRefs] refs;            "Reference numbers"
)
