table gtexSample
"GTEx sample description"
(
    string sampleId;    "GTEx sample identifier"
    string tissue;      "Tissue name. Links to tissue table"
    string donor;       "GTEx subject identifier. Links to donor table"
    int autolysisScore; "Level of tissue self-digestion (0-3; none,mild,moderate,severe, -1 if unknown)"
    string ischemicTime; "Time from tissue removal to preservation, in 4hr intervals"
    float rin;          "RNA Integrity Number"
    string collectionSites;  "GTEx Biospecimen Source Site list"
    string batchId;     "Nucleic acid isolation batch ID"
    string isolationType;   "Type of nucleic acid isolation"
    string isolationDate;   "Date of nucleic acid isolation"
    string pathNotes;   "Pathology report notes"
)
