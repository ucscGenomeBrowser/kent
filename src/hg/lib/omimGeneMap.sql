CREATE TABLE omimGeneMap (
    numbering varchar(255) not null,	# Numbering system, in the format  Chromosome.Map_Entry_Number
    month int not null,	# Month entered
    day int not null,	# Day entered
    year int not null,	# Year entered
    location varchar(255) not null,	# Location
    geneSymbol varchar(255) not null,	# Gene Symbol(s)
    geneStatus char(1) not null,	# Gene Status
    title1 varchar(255) not null,	# Title line1
    title2 varchar(255) not null,	# Title line 2
    omimId int not null,	# MIM Number
    method varchar(255) not null,	# Method
    comment1 varchar(255) not null,	# Comments line 1
    comment2 varchar(255) not null,	# Comments line 2
    disorders1 blob not null,	# Disorders line 1
    disorders2 blob not null,	# Disorders line 2
    disorders3 blob not null,	# Disorders line 3
    mouseCorrelate varchar(255) not null,	# Mouse correlate
    reference varchar(255) not null,	# Reference
              #Indices
    KEY(omimId)
);
