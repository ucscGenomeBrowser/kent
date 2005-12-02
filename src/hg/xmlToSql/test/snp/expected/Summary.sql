CREATE TABLE Summary (
    numRsIds int not null,
    totalSeqLength int not null,
    numContigHits int not null,
    numGeneHits int not null,
    numGiHits int not null,
    num3dStructs smallint not null,
    numStsHits smallint not null,
    PRIMARY KEY(numRsIds)
);
