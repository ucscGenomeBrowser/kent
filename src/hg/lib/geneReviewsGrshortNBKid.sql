# This table is the temp table used by geneReviews track build
CREATE TABLE geneReviewsGrshortNBKid (
    geneSymbol  varchar(255) not null,   # refSeq gene symbol
    grShort     varchar(255) not null,   # short name for GeneReviews article
    NBKid       varchar(255) not null,   # NCBI book ID of the review article
    index (geneSymbol)
);
