CREATE TABLE geneReviewsGeneGRshortNBKidGRtitle (
    geneSymbol  varchar(255) not null,   # refSeq gene symbol
    grShort     varchar(255) not null,   # short name for geneReviews article
    NBKid       varchar(255) not null,   # NCBI book ID of geneRreviews article
    grTitle     varchar(255) not null,   # full geneReviews article name
    index (geneSymbol)
);
