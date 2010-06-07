CREATE TABLE featureToFeatureprop (
    feature int not null,
    featureprop int not null,
    INDEX(feature),
    INDEX(featureprop)
);
