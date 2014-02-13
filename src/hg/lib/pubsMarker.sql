#publications track: matches to markers (snp, gene, band) in text
CREATE TABLE pubsMarkerAnnot (
    articleId bigint not null,	# internal article ID, created during download
    fileId int not null,        # identifier of the file where the marker was found
    annotId int not null,	# unique identifier of this marker within a file
    fileDesc varchar(2000) not null, # description of file where sequence was found 
    markerType enum('snp', 'band', 'gene'), # type of marker: snp, band or gene
    markerId varchar(255), # id of marker, e.g. TP53 or rs12354
    section enum('unknown', 'header', 'abstract', 'intro', 'methods', 'results', 'discussion', 'conclusions', 'ack', 'refs'), 
    snippet varchar(5000) not null,	# flanking text around marker match
        # Indices
    KEY articleIdx(articleId),
    KEY markerIdx(markerId, markerType)
);
