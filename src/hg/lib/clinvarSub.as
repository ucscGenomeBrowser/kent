table clinvarSub
"ClinVar variant submission info"
    (
    int varId index;   "the identifier assigned by ClinVar and used to build the URL, namely https://ncbi.nlm.nih.gov/clinvar/VariationID"
    string clinSign; "interpretation of the variation-condition relationship"
    string dateLastEval;  "the last date the variation-condition relationship was evaluated by this submitter"
    lstring description; "an optional free text description of the basis of the interpretation"
    lstring subPhenoInfo; "the name(s) or identifier(s)  submitted for the condition that was interpreted relative to the variant"
    lstring repPhenoInfo; "the MedGen identifier/name combinations ClinVar uses to report the condition that was interpreted. 'na' means there is no public identifer in MedGen for the condition."
    string revStatus; "the level of review for this submission, namely http//www.ncbi.nlm.nih.gov/clinvar/docs/variation_report/#review_status"
    string collMethod;"the method by which the submitter obtained the information provided"
    string originCounts; "origin and the number of observations for each origin"
    string submitter; "the submitter of this record"
    string scv; "the accession and current version assigned by ClinVar to the submitted interpretation of the variation-condition relationship"
    string subGeneSymbol; "the gene symbol reported in this record"
    lstring explOfInterp; "more details if ClinicalSignificance is 'other' or 'drug response'"
    )
