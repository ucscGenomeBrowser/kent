
#Links from WormBase ORF to SwissProt/trEMBL ID and Description
CREATE TABLE sangerLinks (
    orfName varchar(255) not null,	# WormBase Sequence/ORF name
    protName varchar(255) not null,	# SwissProt/trEMBL ID
    description longblob not null,	# One line description
              #Indices
    INDEX(orfName(10)),
    INDEX(protName(10))
);
