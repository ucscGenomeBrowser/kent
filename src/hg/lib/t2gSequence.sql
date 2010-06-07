#Text to Genome project sequence data table
CREATE TABLE t2gSequence (
    pmcId bigint not null,	# PubMedCentral ID of article
    seqId int not null,		# number of sequence in article
    sequence blob not null,	# sequences
              #Indices
    KEY docIdIdx(pmcId)
);
