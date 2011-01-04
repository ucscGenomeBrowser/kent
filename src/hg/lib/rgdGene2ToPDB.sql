# A xref table for rgdGene2
CREATE TABLE rgdGene2ToPDB (
    name varchar(40) not null,	# RGD gene ID
    value varchar(40),			# Description

    key name(name),
    key value(value)
);
