table cnpIafrate2
"CNP data from Iafrate lab"
    (
    string  chrom;      "Reference sequence chromosome or scaffold"
    uint    chromStart; "Start position in chrom"
    uint    chromEnd;   "End position in chrom"
    string  name;       "BAC name"
    uint    normalGain; "Number of normal individuals with gain"
    uint    normalLoss; "Number of normal individuals with loss"
    uint    patientGain; "Number of patients with gain"
    uint    patientLoss; "Number of patients with loss"
    uint    total;       "Total gain and loss"
    string  cohortType;   "{Control},{Patient},{Control and Patient}"
    )
