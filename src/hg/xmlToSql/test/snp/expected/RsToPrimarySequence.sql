CREATE TABLE RsToPrimarySequence (
    Rs int not null,
    PrimarySequence int not null,
    INDEX(Rs),
    INDEX(PrimarySequence)
);
