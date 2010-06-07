CREATE TABLE feature (
    id2 int not null,
    id varchar(255) not null,
    uniquename int not null,
    organismId varchar(255) not null,
    typeId int not null,
    name int not null,
    timelastmodified varchar(255) not null,
    seqlen smallint not null,
    dbxrefId int not null,
    featureSynonym int not null,
    featureloc int not null,
    featureRelationship int not null,
    md5checksum int not null,
    residues int not null,
    isAnalysis tinyint not null,
    analysisfeature int not null,
    PRIMARY KEY(id2)
);
