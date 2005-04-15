
char *fetchBuiltinCode()
/* Return a string with the built in stuff. */
{
return
"to fileOpen(string name, string mode) into file f;\n"
"to fileClose(file f);\n"
"to fileWriteString(file f, string s);\n"
"to fileReadLine(file f) into (string s);\n"
"\n"
"class file\n"
"    {\n"
"    string name;\n"
"    to writeString(string s);\n"
"    to readLine() into (string s);\n"
"    to printMe() {print(self.name);}\n"
"    }\n"
;
}
