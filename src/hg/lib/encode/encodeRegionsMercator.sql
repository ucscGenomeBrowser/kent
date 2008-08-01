create table encodeRegionsMercator (
    bin        smallint(5)  unsigned not null,
    chrom      varchar(255)          not null,
    chromStart int(10)      unsigned not null,
    chromEnd   int(10)      unsigned not null,
    name       varchar(255)          not null,
    score      int(10)      unsigned not null,
    strand     char(1)               not null
)
