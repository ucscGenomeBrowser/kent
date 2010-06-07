CREATE TABLE MapLoc (
    id int not null,
    asnFrom int not null,
    asnTo int not null,
    locType varchar(255) not null,
    alnQuality float not null,
    orient varchar(255) not null,
    physMapStr varchar(255) not null,
    physMapInt int not null,
    leftFlankNeighborPos int not null,
    rightFlankNeighborPos int not null,
    leftContigNeighborPos int not null,
    rightContigNeighborPos int not null,
    numberOfMismatches smallint not null,
    numberOfDeletions smallint not null,
    numberOfInsertions int not null,
    PRIMARY KEY(id)
);
