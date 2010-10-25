CREATE TABLE rgdGene2ToUniProt (
    name  varchar(40)  not null,	# RGD Gene ID
    value varchar(40) not null,	# UniProt Accession
              #Indices
    INDEX(name(20)),
    INDEX(value(20))

);
