CREATE TABLE omimGeneMap (
    numbering varchar(10),	# Numbering system, in the format  Chromosome.Map_Entry_Number
    month int,		# Month entered
    day int,		# Day entered
    year int not null,	# Year entered
    location varchar(40),	# Location
    geneSymbol varchar(255),	# Gene Symbol(s)
    geneStatus char(1),	# Gene Status
    title1 blob,	# Title line1
    title2 blob,	# Title line 2
    omimId int not null,# MIM Number
    method blob,	# Method
    comment1 blob,	# Comments line 1
    comment2 blob,	# Comments line 2
    disorders1 blob,	# Disorders line 1
    disorders2 blob,	# Disorders line 2
    disorders3 blob,	# Disorders line 3
    mouseCorrelate blob,# Mouse correlate
    reference blob,	# Reference
              #Indices
    KEY(omimId)
);
