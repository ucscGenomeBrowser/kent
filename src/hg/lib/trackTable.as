table trackTable
"This describes an annotation track."
(
string mapName; "Symbolic ID of Track"
string tableName; "Name of table (without any chrN_ if split)"
string shortLabel; "Short label displayed on left"
string longLabel; "Long label displayed in middle"
ubyte visibility; "0=hide, 1=dense, 2=full"
ubyte colorR;	  "Color red component 0-255"
ubyte colorG;	  "Color green component 0-255"
ubyte colorB;	  "Color blue component 0-255"
ubyte altColorR;  "Light color red component 0-255"
ubyte altColorG;  "Light color green component 0-255"
ubyte altColorB;  "Light color blue component 0-255"
ubyte useScore;   "1 if use score, 0 if not"
ubyte isSplit;    "1 if table is split across chromosomes"
ubyte private;	  "1 if only want to show it on test site"
ubyte hardCoded;  "1 if code to interpret it is build in"
)
