
/* CSP3 Usage Notes

NOT IN USE YET. My guess is that we will not be able to use CSP3 until the year 2021.
BUT CSP2 is working very well for us.  We can add script tags dynamically to the page
and set their nonce value with script.setAttribute(nonce, value);

We almost went with CSP3 even though it is very new at this time (2017-01-26).
It has one important new script-src directive 'strict-dynamic' which does 3
things:

First, the word "strict" means that it only uses nonces -- the whitelist of sites and paths
is completely ignored. When we adopt it someday we will need to go through
our source code and add the nonce='random' to our non-inline js script includes.
There are probably only a few dozen places in the code that do this and it should
be easy. The reason that whitelists can be a big problem is that they are brittle,
can grow to be very large and unweildy and hard to maintain. Furthermore, an 
entire large portal often has other stuff there which hackers can exploit and which
you were not intending to authorize. If the whitelisted site has jsonp endpoints,
redirection-scripts, AugustusJS, and other fancy js frameworks, or even old jquery includes with bugs,
then there are exploits that whitelists do not protect against.

Second, the word "dynamic" in the directive means that it dynamically delegates trust authorization
to other scripts, so that whatever stuff libraries include are automaically trusted recursively. 
Non-inline script libraries, both local and off-site will require nonces alike.

Third, the final thing that 'strict-dynamic' does is that
when a nonced-script dynamically adds a script to the DOM dynamically with createElement('script')
and document.head.appendChild(script);, the nonce value is added automatically,
unlike CSP2.  'script-dynamic' specifically encourages such DOM dynamic additions
while discouraging more dangerous methods which require raw html string parsing followed by eval such as
innerHTML and document.write.

These changes are aimed at making CSP more user-friendly and deployable.

So to deploy CSP3 is fairly easy from a code-change point of view. Just add 'strict-dynamic'
to the CSP policy and add some nonces to the js library include lines.  But we do not 
accrue that much value from it over CSP2 at this time. Plus, because there is currently
no easy way to tell which CSP level a browser supports, and we have no huge whitelist at this time,
there is not much benefit.  

Also with CSP2, they gave us the trick of using 'unsafe-inline' together
with 'nonce=random' so that CSP1 would have no enforcement, but would at least run without errors
and allow inline js, while CSP2 DOES have nonce-protected inline js.  BUT They did not create
any equivalent policy for backwards compatibility that would likewise disable enforcement in CSP2 browsers
while enabling full enforcement in CSP3 browsers.  Without that trick we are basically stuck
with a CSP2 level policy until basically nearly all browsers have CSP3. This could take several years,
even though Chrome and FF already support CSP3 and even MS Edge will have it soon.
I contacted the designers of 'strict-dynamic', but it was too late to change CSP3.
*/
