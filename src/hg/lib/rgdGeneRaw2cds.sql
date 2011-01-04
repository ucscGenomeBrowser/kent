CREATE TABLE rgdGeneRaw2cds (
    seqname varchar(40),
    source varchar(255),
    feature varchar(40),
    start  int,
    end int,
    score  varchar(40),		
    strand varchar(40),	
    frame varchar(40),
    rgdId varchar(255),
    KEY(rgdId)
);
