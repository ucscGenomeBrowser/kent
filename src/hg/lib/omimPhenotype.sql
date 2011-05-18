CREATE TABLE omimPhenotype (
    omimId int not null,		# OMIM ID
    description blob,			# disorder description
    phenotypeId int,			# phnoteype ID
    phenotypeClass int(1),		# phenotype class
    #Indices
    KEY(omimId),
    KEY(phenotypeId)
);
