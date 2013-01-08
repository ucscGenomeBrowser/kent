CREATE TABLE coriellDelDup (
    chrom varchar(255) not null,    # Reference sequence chromosome or scaffold
    chromStart int unsigned not null,   # Start position in chromosome
    chromEnd int unsigned not null,     # End position in chromosome
    name varchar(255) not null, # Short Name of item
    score int unsigned not null default 0, # score == (CN state * 100) -> (0,100,200,300,400)
    strand char(1),     # + or -
    thickStart int unsigned,  # Start of where display should be thick (start codon)
    thickEnd int unsigned,  # End of where display should be thick (stop codon)
    reserved int unsigned,      # color from CN state
    CN_State enum("0", "1", "2", "3", "4") not null default "2",
    cellType enum("B_Lymphocyte", "Fibroblast", "Amniotic_fluid_cell_line", "Chorionic_villus_cell_line") not null default "B_Lymphocyte",
    description varchar(255) not null,  # Description (Diagnosis)
    ISCN varchar(255) not null,         # ISCN nomenclature
              #Indices
    INDEX(chrom, chromStart)
);
