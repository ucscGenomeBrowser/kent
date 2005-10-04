
table fileLocation
"Location of image, typically a file directory"
    (
    int id;	"ID of location"
    lstring name;	"Directory path usually"
    )

table bodyPart
"Brain, eye, kidney, etc.  Use 'whole' for whole body"
    (
    int id;	"ID of body part"
    string name;	"Name of body part"
    )

table sliceType
"Horizontal, coronal, whole mount, etc."
    (
    int id;	"ID of section"
    string name;	"Name of horizontal/whole mount, etc"
    )

table treatment
"Fixation and other treatment conditions"
    (
    int id;	"ID of treatment"
    string conditions;	"Text string describing conditions"
    )

table contributor
"Info on contributor"
    (
    int id;	"ID of contributor"
    string name;	"Name in format like Kent W.J."
    )

table journal
"Information on a journal"
    (
    int id;	"ID of journal"
    string name;	"Name of journal"
    string url;	"Journal's main url"
    )

table submissionSet
"Info on a batch of images submitted at once"
    (
    int id;			"ID of submission set"
    lstring contributors;	"Comma separated list of contributors in format Kent W.J., Wu F.Y."
    lstring publication;	"Name of publication"
    lstring pubUrl;		"Publication URL"
    int journal;		"Journal for publication"
    lstring setUrl;		"URL for whole set"
    lstring itemUrl;		"URL for item.  Put %s where image.submitId should go"
    )

table submissionContributor
"Association between contributors and submissionSets"
    (
    int submissionSet;	"ID in submissionSet table"
    int contributor;	"ID in contributor table"
    )

table sectionSet
"Info on a bunch of sections through same sample"
    (
    int id;	"Section id"
    )

table antibody
"Information on an antibody"
    (
    int id;	"Antibody ID"
    string name;	"Name of antibody"
    lstring description;	"Description of antibody"
    int taxon;		"Taxon of animal antibody is from"
    )

table gene
"Info on a gene"
    (
    int id;		"ID of gene"
    string name;	"Gene symbol (HGNC if available)"
    string locusLink;	"NCBI locus link ID or blank if none"
    string refSeq;	"RefSeq ID or blank if none"
    string genbank;	"Genbank/EMBL accession or blank if none"
    string uniProt;	"SwissProt/Uniprot accession or blank if none"
    int taxon;		"NCBI taxon ID of organism"
    )

table geneSynonym
"A synonym for a gene"
    (
    int gene;	"ID in gene table"
    string name;	"Synonymous name for gene"
    )

table probeType
"Type of probe - RNA, antibody, etc."
    (
    int id;		"ID of probe type"
    string name;	"Name of probe type"
    )

table probe
"Info on a probe"
    (
    int id;		"ID of probe"
    int gene;		"Associated gene if any"
    int antibody;	"Associated antibody if any"
    int probeType;	"Type of probe - antibody, RNA, etc.."
    string fPrimer;	"Forward PCR primer if any"
    string rPrimer;	"Reverse PCR primer if any"
    lstring seq;	"Associated sequence if any"
    )

table probeColor
"Color - what color probe is in"
    (
    int id;		"Id of color"
    string name;	"Color name"
    )

table imageFile
"A biological image file"
    (
    int id;		"ID of imageFile"
    string fileName;	"Image file name not including directory"
    float priority;	"Lower priorities are displayed first"
    int fullLocation;	"Location of full-size image"
    int screenLocation;	"Location of screen-sized image"
    int thumbLocation;	"Location of thumbnail-sized image"
    int submissionSet;	"Submission set this is part of"
    string submitId;	"ID within submission set"
    )

table image
"An image.  There may be multiple images within an imageFile"
    (
    int id;		"ID of image"
    int imageFile;	"ID of image file"
    int imagePos;	"Position in image file, starting with 0"
    int sectionSet;	"Set of sections this is part of or 0 if none"
    int sectionIx;	"Position (0 based) within set of sections"
    int taxon;		"NCBI taxon ID of organism"
    byte isEmbryo;	"TRUE if embryonic.  Age will be relative to conception"
    float age;		"Age in days since birth or conception depending on isEmbryo"
    int bodyPart;	"Part of body image comes from"
    int sliceType;	"How section is sliced"
    int treatment;	"How section is treated"
    )

table imageProbe
"Associate probe and image"
    (
    int image;	"ID of image"
    int probe;	"ID of probe"
    int probeColor;	"ID of probeColor"
    )

