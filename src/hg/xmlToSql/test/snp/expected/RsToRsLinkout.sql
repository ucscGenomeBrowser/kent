CREATE TABLE RsToRsLinkout (
    Rs int not null,
    RsLinkout int not null,
    INDEX(Rs),
    INDEX(RsLinkout)
);
