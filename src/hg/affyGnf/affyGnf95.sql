# Short style expression data

CREATE table affyGnfU95
    (
    name varchar(255) not null,	# Things like 1134_at usually
    expCount int not null,	# Number of experiments
    expScores longblob not null, # Comma separated list of scores
    INDEX(name(8))
    )
