# A xref table for rgdGene2
CREATE TABLE rgdGene2ToSuper (
    name varchar(40) not null,	# RGD gene ID
    value varchar(40),		# Superfamily ID

    key name(name),
    key value(value)
);
