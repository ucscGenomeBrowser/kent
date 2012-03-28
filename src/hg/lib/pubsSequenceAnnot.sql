#publications track sequence data table
CREATE TABLE pubsSequenceAnnot (
    articleId bigint not null,	# identifier of the article where the sequence was found
    fileId int not null,        # identifier of the file where the sequence was found
    seqId bigint not null,	# unique identifier of this sequence within a file
    annotId bigint not null,    # articleId(10)+fileId(3)+seqId(5), refd by pubsSequenceAnnot
    fileDesc varchar(2000) not null, # description of file where sequence was found 
    sequence longblob not null,	# sequence
    snippet longblob not null,	# flanking characters around sequence in article
    locations varchar(5000),	# comma-sep list of genomic locations where this sequence matches
        #Indices
    KEY seqIdIdx(seqId),
    KEY annotIdx(annotId),
    KEY artIdIdx(articleId)
);
