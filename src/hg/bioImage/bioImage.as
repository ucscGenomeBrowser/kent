table location
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

table imageType
"Type of image - RNA in situ, fluorescent antibody, etc."
    (
    int id;		"ID of image type"
    string name;	"Name of image type"
    )

table contributer
"Info on contributor"
    (
    int id;	"ID of contributer"
    string name;	"Name in format like Kent W.J."
    )

table submissionSet
"Info on a batch of images submitted at once"
    (
    int id;			"ID of submission set"
    lstring publication;	"Name of publication"
    lstring pubUrl;		"Publication URL"
    lstring setUrl;		"URL for whole set"
    lstring itemUrl;		"URL for item.  Put $$ where image.submitId should go"
    )

table submissionContributer
"Association between contributers and submissionSets"
    (
    int submissionSet;	"ID in submissionSet table"
    int contributer;	"ID in contributer table"
    )

table sectionSet
"Info on a bunch of sections through same sample"
    (
    int id;	"Section id"
    int count;	"Count of sections"
    )

table image
"A single biological image"
    (
    int id;		"ID of image"
    string name;	"Image name (file name in directory)"
    int fullLocation;	"Location of full image"
    int thumbLocation;	"Location of thumbnail image"
    int submissionSet;	"Submission set this is part of"
    int sectionSet;	"Set of sections this is part of or 0 if none"
    int sectionIx;	"Position (0 based) within set of sections"
    string submitId;	"ID within submission set"
    string gene;	"Gene symbol (HUGO if available)"
    string locusLink;	"Locus link ID or blank if none"
    string refSeq;	"RefSeq ID or blank if none"
    string genbank;	"Genbank accession or blank if none"
    byte defaultImage;	"Set to 1 if this is default for gene"
    int taxon;		"NCBI taxon ID of organism"
    byte isEmbryo;	"TRUE if embryonic.  Age will be relative to conception"
    float age;		"Age in days since birth or conception depending on isEmbryo"
    int bodyPart;	"Part of body image comes from"
    int sliceType;	"How section is sliced"
    int imageType;	"Type of image - in situ, etc."
    int treatment;	"How section is treated"
    )
