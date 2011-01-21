CREATE TABLE omimDisorderMap (
    disorder blob not null,		# disorder
    geneSymbols varchar(255) not null,	# Gene Symbol(s)
    omimId int not null,		# OMIM ID
    location varchar(255) not null,	# Location
    questionable int(1) not null,	# questionable, if it is 1, otherwise 0
    #Indices
    KEY(omimId)
);
