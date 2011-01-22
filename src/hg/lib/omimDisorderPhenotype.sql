CREATE TABLE omimDisorderPhenotype (
    omimId int not null,		# OMIM ID
    phenotypeClass int(1) not null,	# phenotype class
    questionable int(1) not null,	# whether it is marked as questionable
    hasBracket int(1) not null,		# has Bracket
    hasBrace int(1) not null,		# has Brace
    phenotypeId int,			# phnoteype ID
    disorder blob,			# disorder description
    #Indices
    KEY(omimId),
    KEY(phenotypeId)
);
