create table affyGnfU74A (
    bin int,
    chrom varchar(255) not null,	# Human chromosome or FPC contig
    chromStart int unsigned not null,	# Start position in chromosome
    chromEnd int unsigned not null,	# End position in chromosome
    name varchar(255) not null,	# Name of item
    score int,
    strand char(2),
    thickStart int unsigned,
    thickEnd int unsigned,
    reserved int,
    blockCount int,
    blockSizes blob,
    blockStarts blob,
    expCount int,
    expIds blob,
    expScores blob,
    index(chrom(8),bin),
    index(name(10))
);
