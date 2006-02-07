
char *fetchBuiltInCode()
/* Return a string with the built in stuff. */
{
return
"string programName;\n"
"array of string args;\n"
"\n"
"to punt(string message);\n"
"para print(var v);\n"
"to prin(var v);\n"
"to keyIn() into string s;\n"
"to lineIn() into string s;\n"
"to randNum() into double zeroToOne;\n"
"to randInit();\n"
"para sqrt(double x) into double y;\n"
"para atoi(string a) into int i;\n"
"to getEnvArray() into array of string envArray;\n"
"para milliTicks() into long milliseconds;\n"
"para floatString(double f, int digitsBeforeDecimal=0, \n"
"		int digitsAfterDecimal=2, bit scientificNotation=0) \n"
"into (string s);\n"
"para intString(long l, int minWidth, bit zeroPad, bit commas) into (string s);\n"
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
"    para dupe() into string dupe;\n"
"    para start(int size) into string start;\n"
"    para rest(int start) into string rest;\n"
"    para middle(int start, int size) into string part;\n"
"    para end(int size) into string end;\n"
"    para upper();\n"
"    para lower();\n"
"    para append(string s);\n"
"    para find(string s) into int found;\n"
"    para findNext(string s, int startPos) into int found;\n"
"    para nextSpaced(int pos) into (string s, int newPos);\n"
"    para nextWord(int pos) into (string s, int newPos);\n"
"    para fitLeft(int size) into (string s);\n"
"    para fitRight(int size) into (string s);\n"
"    }\n"
"class _pf_array\n"
"    {\n"
"    int size;\n"
"    _operator_ append(var el);\n"
"    }\n"
;
}
