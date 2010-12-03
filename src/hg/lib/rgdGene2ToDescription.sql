# A xref table for rgdGene2
CREATE TABLE rgdGene2ToDescription (
    name varchar(40) not null,	# RGD gene ID
    value text,			# Description

    key name(name)
);
