/* correlate.h - a help text string for correlate.c */

#ifndef CORRELATE_H
#define CORRELATE_H

static char *corrHelpText = "\
<HR>\
<H2>Description</H2>\
<P>\
<EM>Please note, this function is under development June-July 2005. \
Functionality and features are subject to change.</EM>\
</P>\
<H2>Methods</H2>\
<P>\
The data points from each table are projected down to the base level. \
At this base level, the two data sets are intersected such that they each \
have data values at the same bases, resulting in data sets of equal length n. \
These two data sets (X,Y) are then used \
in a standard linear correlation function, computing the \
correlation coefficient: <BR> \
<em>r = s<sub>xy</sub> / (s<sub>x</sub> * s<sub>y</sub>)</em><BR> \
where <em>s<sub>x</sub></em> and <em>s<sub>y</sub></em> are the \
standard deviations of the data sets X and Y, and \
<em>s<sub>xy</sub></em> is the <em><b>covariance</b></em> computed: <BR>\
<em>s<sub>xy</sub> = ( sum(X<sub>i</sub>*Y<sub>i</sub>) - \
((sum(X<sub>i</sub>)*sum(Y<sub>i</sub>))/n) ) / (n - 1)</em> \
</P>\
<H2>Known Limitations</H2>\
<P>The number of bases that can be processed is limited, on the order of \
about 60,000,000 due to memory and processing time limits.  Therefore, \
large chromosomes, e.g. chr1 on human, can not be processed entirely. \
Future improvements to the process are expected to remove this processing \
limit.\
</P>";

#endif /* CORRELATE_H */
