/* web.c - some functions to output HTML code */

#ifndef WEB_H
#define WEB_H

void webStartText();
/* output the head for a text page */

void webStart(char* format,...);
/* output a CGI and HTML header with the given title in printf format */

void webNewSection(char* format, ...);
/* create a new section on the web page */

void webEnd();
/* output the footer of the HTML page */

void webAbort(char* title, char* format, ...);
/* an abort function that outputs a error page */

#endif /* WEB_H */
