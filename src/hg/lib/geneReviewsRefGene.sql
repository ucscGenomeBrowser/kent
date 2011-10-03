CREATE TABLE geneReviewsRefGene (
    geneSymbol  varchar(255) not null,   # refSeq gene symbol         
    grShort     varchar(255) not null,   # short name for GeneReviews article
    diseaseID   int unsigned not null,   # Disease ID of the review article
    diseaseName varchar(255) not null,   # Disease name of the review article
    index (geneSymbol)
);
