
char *fetchBuiltInCode()
/* Return a string with the built in stuff. */
{
return
"string programName;\n"
"array of string args;\n"
"\n"
"to punt(string message);\n"
"to print(var v);\n"
"to prin(var v);\n"
"to keyIn() into string s;\n"
"to lineIn() into string s;\n"
"to randNum() into double zeroToOne;\n"
"to randInit();\n"
"to sqrt(double x) into double y;\n"
"to getEnvArray() into array of string envArray;\n"
"to milliTicks() into long milliseconds;\n"
"to floatString(double f, int digitsBeforeDecimal=0, \n"
"		int digitsAfterDecimal=2, bit scientificNotation=0) \n"
"into (string s);\n"
"to intString(long l, int minWidth, bit zeroPad, bit commas) into (string s);\n"
"\n"
"class file\n"
"    {\n"
"    string name;\n"
"    to close();\n"
"    to writeString(string s);\n"
"    to readLine() into (string s);\n"
"    to readBytes(int count) into (string s);\n"
"    to put(var v);\n"
"    to get(var justForType) into var x;\n"
"    }\n"
"\n"
"to fileOpen(string name, string mode) into file f;\n"
;
}

char *fetchStringDef()
/* Return a string with definition of string. */
{
return
"class _pf_string\n"
"    {\n"
"    int size;\n"
"    to dupe() into string dupe;\n"
"    to start(int size) into string start;\n"
"    to rest(int start) into string rest;\n"
"    to middle(int start, int size) into string part;\n"
"    to end(int size) into string end;\n"
"    to upper();\n"
"    to lower();\n"
"    to append(string s);\n"
"    to find(string s) into int found;\n"
"    to findNext(string s, int startPos) into int found;\n"
"    to nextSpaced(int pos) into (string s, int newPos);\n"
"    to nextWord(int pos) into (string s, int newPos);\n"
"    to fitLeft(int size) into (string s);\n"
"    to fitRight(int size) into (string s);\n"
"    }\n"
"class _pf_array\n"
"    {\n"
"    int size;\n"
"    }\n"
;
}
