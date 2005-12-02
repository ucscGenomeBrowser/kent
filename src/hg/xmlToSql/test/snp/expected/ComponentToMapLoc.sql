CREATE TABLE ComponentToMapLoc (
    Component int not null,
    MapLoc int not null,
    INDEX(Component),
    INDEX(MapLoc)
);
