table cutter
"Restriction Enzymes"
(
   string name;                   "Name of Enzyme"
   uint size;                     "Size of recognition sequence"
   uint matchSize;                "size without N's"
   string seq;                    "Recognition sequence"
   uint cut;                      "Cut site on the plus strand"
   int overhang;                  "Overhang"
   ubyte palindromic;             "1 if it's panlidromic, 0 if not."
   ubyte semicolon;               "1 if it's from a REBASE record that has a semicolon in front, 0 if not."
   uint numSciz;                  "Number of isoscizomers"
   string[numSciz] scizs;         "Names of isoscizomers"
   uint numCompanies;             "Number of companies selling this enzyme"
   char[numCompanies] companies;  "Company letters"
   uint numRefs;                  "Number of references"
   uint[numRefs] refs;            "Reference numbers"
)
