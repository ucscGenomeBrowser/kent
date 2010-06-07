CREATE TABLE PrimarySequenceToMapLoc (
    PrimarySequence int not null,
    MapLoc int not null,
    INDEX(PrimarySequence),
    INDEX(MapLoc)
);
