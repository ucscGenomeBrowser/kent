table fileLocation
"Location of image, typically a file directory"
    (
    int id;	"ID of location"
    lstring name;	"Directory path usually"
    )

table submissionSource
"Source of data - an external database, a contributor, etc."
    (
    int id;                     "ID of submission source"
    string name;                "Short name: Jackson Lab, Paul Gray, etc."
    )

table submissionSet
"Info on a batch of images submitted at once"
    (
    int id;                     "ID of submission set"
    string name;                "Name of submission set"
    int submissionSource;       "Source of this submission"
    int privateUser;            "ID of user allowed to view. If 0 all can see."
    int copyright;              "Copyright notice"
    )

table copyright
"Copyright information"
    (
    int id;     "ID of copyright"
    lstring notice;     "Text of copyright notice"
    )

table contributor
"Info on contributor"
    (
    int id;     "ID of contributor"
    string name;        "Name in format like Kent W.J."
    )

table imageFile
"A biological image file"
    (
    int id;             "ID of imageFile"
    string fileName;    "Image file name, not including directory"
    float priority;     "Lower priorities are displayed first"
    int imageWidth;     "Width of image in pixels"
    int imageHeight;    "Height of image in pixels"
    int fullLocation;   "Location of full-sized image"
    int thumbLocation;  "Location of thumbnail-sized image"
    int submissionSet;  "Submission set this is part of"
    string submitId;    "ID within submission set"
    )

table image
"An image.  There may be multiple images within an imageFile"
    (
    int id;             "ID of image"
    int submissionSet;  "Submission set this is part of"
    int imageFile;      "ID of image file"
    int imagePos;       "Position in image file, starting with 0"
    )
