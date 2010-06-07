CREATE TABLE blast (
    strand char(2) not null,
    qName varchar(255) not null,
    qStart int unsigned not null,	
    qEnd int unsigned not null,	
    tName varchar(255) not null,	
    tStart int unsigned not null,	
    tEnd int unsigned not null,	
    tSize int unsigned not null,	
    score double not null
);
