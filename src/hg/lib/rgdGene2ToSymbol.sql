CREATE TABLE rgdGene2ToSymbol (
    rgdId  varchar(40)  not null,	# RGD Gene ID
    rgdIdNum   varchar(40)  not null,	# RGD Gene Number
    geneSymbol varchar(40) not null,	# UniProt Accession
              #Indices
    INDEX(rgdId(20)),
    INDEX(rgdIdNum(20)),
    INDEX(geneSymbol(20))

);
