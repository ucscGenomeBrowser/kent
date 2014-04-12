# This table is the temp table used by geneReviews track build
CREATE TABLE geneReviewsGrshortTitleNBKid (
    grShort     varchar(255) not null,   # short name for GeneReviews article
    grTitle     varchar(255) not null,   # full geneReviews article name
    NBKid       varchar(255) not null,   # NCBI book ID of the review article
    index (grShort)
);
