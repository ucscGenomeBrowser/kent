#Text to Genome project sequence data table
CREATE TABLE t2gSequence (
    pmcId bigint not null,	# PMC ID
    seqId int not null,		# display ID
    sequence blob not null,	# list of sequences
              #Indices
    KEY docIdIdx(pmcId)
);
