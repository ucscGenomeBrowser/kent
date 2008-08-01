CREATE TABLE encodeRegulomeProbXXX (  
    bin smallint unsigned not null,  
    chrom varchar(255) not null,  
    chromStart int unsigned not null,  
    chromEnd int unsigned not null,  
    dataValue float not null,  
    INDEX(chrom(6),bin)  
);  
