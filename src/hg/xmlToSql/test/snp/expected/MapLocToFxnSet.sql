CREATE TABLE MapLocToFxnSet (
    MapLoc int not null,
    FxnSet int not null,
    INDEX(MapLoc),
    INDEX(FxnSet)
);
