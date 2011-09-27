CREATE TABLE omimAvRepl (
    avId varchar(40) not null,	# MIM AV Number
    omimId int not null,	# MIM ID
    
    firstAa char(1),		# first AA
    position int,		# position in the protein sequence
    secondAa char(1),		# 2nd AA
    dbSnpId varchar(255),	# dbSNP ID if available
              #Indices
    replStr varchar(255),	# AA replacement string
    description varchar(255),   # description
    KEY(avId),
    KEY(omimId)
);
