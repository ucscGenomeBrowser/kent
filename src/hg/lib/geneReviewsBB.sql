# sql to create geneReviewsBB table 
DROP TABLE IF EXISTS geneReviewsBB;
CREATE TABLE geneReviewsBB (
    fileName varchar(255) not null      # geneReviews.bb location (url)
);
