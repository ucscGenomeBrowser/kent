CREATE TABLE omimAv (
    avId varchar(40) not null,	# MIM AV Number
    omimId int not null,	# MIM ID
    seqNo  int not null,	# sequence number
    
    geneSymbol varchar(255),	# gene symbol
    replacement varchar(255),	# AA replacement
    repl1 varchar(255),	# part 1 of replacement
    repl2 varchar(255),	# part 2 of replacement
    dbSnpId varchar(255),	# dbSNP ID if available
              #Indices
    description blob,
    KEY(avId),
    KEY(omimId)
);
