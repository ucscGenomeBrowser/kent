# A xref table for rgdGene2
CREATE TABLE rgdGene2ToDescription (
    name varchar(40) not null,	# RGD gene ID
    val blob,			# Description

    key name(name)
);
