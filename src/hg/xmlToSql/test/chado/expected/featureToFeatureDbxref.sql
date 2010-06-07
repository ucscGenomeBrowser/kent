CREATE TABLE featureToFeatureDbxref (
    feature int not null,
    featureDbxref int not null,
    INDEX(feature),
    INDEX(featureDbxref)
);
