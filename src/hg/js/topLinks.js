// topLinks.js - behavior for the two top-right menu-bar links, "Login" and "Share a link".
//
//   Login: when the user is logged in, the menu item shows their username and opens a small
//   account dialog (change password, sign out).  When logged out it is a plain link to hgSession,
//   so this script does nothing for it.
//
//   Share a link (browser pages only): saves the current view as a session and shows a copyable
//   short link.  Logged-in users get to name the session and choose whether others may open it;
//   logged-out users get an anonymous link created on the fly.
//
// The C code that builds the menu bar (lib/web.c menuBar() for CGI pages, hgMenubar.c for static
// pages) fills in the data-* attributes that this script reads.  The dialog is a self-contained
// overlay so it works on static pages too (no jQuery UI dependency).

/* jshint esversion: 8 */
/* global $, document, window, URL, getHgsid, copyToClipboard */

var topLinks = (function() {
    "use strict";

    // Small helper to build an element with properties and inline styles set from the DOM
    // (assigning to element.style.* is CSP-safe, unlike a style="" attribute in markup).
    function el(tag, props, styles) {
        var e = document.createElement(tag);
        var k;
        if (props)
            for (k in props)
                if (props.hasOwnProperty(k)) e[k] = props[k];
        if (styles)
            for (k in styles)
                if (styles.hasOwnProperty(k)) e.style[k] = styles[k];
        return e;
    }

    var closeCurrent = null;   // closer for the currently open modal, if any

    function closeModal() {
        if (closeCurrent) { closeCurrent(); closeCurrent = null; }
    }

    // Show a centered modal with the given title and body node.  Returns the body element so the
    // caller can replace its contents later (e.g. swap a spinner for the resulting link).
    function showModal(titleText, bodyNode, widthPx) {
        closeModal();
        var overlay = el("div", {}, {
            position: "fixed", top: "0", left: "0", right: "0", bottom: "0",
            background: "rgba(0,0,0,0.4)", zIndex: "10000"
        });
        var box = el("div", {}, {
            position: "fixed", top: "90px", left: "50%", transform: "translateX(-50%)",
            background: "#fff", color: "#000", border: "1px solid #888", borderRadius: "4px",
            boxShadow: "0 2px 12px rgba(0,0,0,0.4)", width: (widthPx || 430) + "px", maxWidth: "92%",
            zIndex: "10001", fontSize: "14px"
        });
        var titleBar = el("div", {}, {
            background: "#00457c", color: "#fff", padding: "8px 12px", fontWeight: "bold",
            borderTopLeftRadius: "4px", borderTopRightRadius: "4px", position: "relative"
        });
        titleBar.appendChild(document.createTextNode(titleText));
        var x = el("span", {title: "Close", textContent: "×"}, {
            position: "absolute", right: "10px", top: "5px", cursor: "pointer",
            fontSize: "18px", lineHeight: "18px"
        });
        x.addEventListener("click", closeModal);
        titleBar.appendChild(x);
        var body = el("div", {}, {padding: "14px"});
        if (bodyNode) body.appendChild(bodyNode);
        box.appendChild(titleBar);
        box.appendChild(body);
        overlay.appendChild(box);
        document.body.appendChild(overlay);

        // Swallow Escape and clicks so they don't also reach an underlying dialog's own
        // close-on-escape / click-outside handlers (e.g. the hgc item-details popup).
        function onKey(ev) {
            if (ev.key === "Escape") {
                ev.preventDefault();
                ev.stopImmediatePropagation();
                closeModal();
            }
        }
        overlay.addEventListener("click", function(ev) {
            if (ev.target === overlay)
                closeModal();
            ev.stopPropagation();
        });
        document.addEventListener("keydown", onKey, true);   // capture: run before other handlers
        closeCurrent = function() {
            document.removeEventListener("keydown", onKey, true);
            if (overlay.parentNode) overlay.parentNode.removeChild(overlay);
        };
        return body;
    }

    // ---- Login / account dialog --------------------------------------------------------------

    function showLoginDialog(link) {
        var user = link.getAttribute("data-username");
        var logoutUrl = link.getAttribute("data-logouturl");
        var changePwUrl = link.getAttribute("data-changepwurl");
        var body = document.createElement("div");
        var p = el("p");
        p.appendChild(document.createTextNode("Signed in as "));
        p.appendChild(el("b", {textContent: user}));   // textContent avoids HTML injection
        p.appendChild(document.createTextNode("."));
        body.appendChild(p);

        // helper: add an <li><a> to a list
        function addLink(ul, href, text) {
            var li = el("li", {}, {margin: "6px 0"});
            li.appendChild(el("a", {href: href, textContent: text}));
            ul.appendChild(li);
        }

        // "My Data" navigation: sessions, custom tracks, uploaded track hubs (hubSpace).
        var hgsid = getHgsidSafe();
        var navUl = el("ul", {}, {listStyle: "none", margin: "0 0 10px 0", padding: "0"});
        addLink(navUl, "../cgi-bin/hgSession?hgS_doMainPage=1&hgsid=" + hgsid, "My Sessions");
        addLink(navUl, "../cgi-bin/hgCustom?hgsid=" + hgsid, "My Custom Tracks");
        addLink(navUl, "../cgi-bin/hgHubConnect?hgsid=" + hgsid + "#hubUpload", "My Track Hubs");
        body.appendChild(navUl);

        // Account actions.
        var ul = el("ul", {}, {listStyle: "none", margin: "0", padding: "0",
                               borderTop: "1px solid #ddd", paddingTop: "8px"});
        if (changePwUrl)
            addLink(ul, changePwUrl, "Change password");
        addLink(ul, logoutUrl, "Sign out");
        body.appendChild(ul);
        showModal("Account", body);
    }

    // ---- Share a link ------------------------------------------------------------------------

    function getHgsidSafe() {
        if (typeof getHgsid === "function")
            return getHgsid();
        var m = /[?&]hgsid=([^&]+)/.exec(window.location.search);
        return m ? m[1] : "";
    }

    // Clipboard SVG icon (same FontAwesome glyph used by printCopyToClipboardButton in hgSession.c).
    var clipboardSvg = "<svg style='width:0.9em;vertical-align:middle;margin-right:4px' " +
        "xmlns='http://www.w3.org/2000/svg' viewBox='0 0 512 512'><path d='M502.6 70.63l-61.25-61.25" +
        "C435.4 3.371 427.2 0 418.7 0H255.1c-35.35 0-64 28.66-64 64l.0195 256C192 355.4 220.7 384 256 " +
        "384h192c35.2 0 64-28.8 64-64V93.25C512 84.77 508.6 76.63 502.6 70.63zM464 320c0 8.836-7.164 " +
        "16-16 16H255.1c-8.838 0-16-7.164-16-16L239.1 64.13c0-8.836 7.164-16 16-16h128L384 96c0 17.67 " +
        "14.33 32 32 32h47.1V320zM272 448c0 8.836-7.164 16-16 16H63.1c-8.838 0-16-7.164-16-16L47.98 " +
        "192.1c0-8.836 7.164-16 16-16H160V128H63.99c-35.35 0-64 28.65-64 64l.0098 256C.002 483.3 28.66 " +
        "512 64 512h192c35.2 0 64-28.8 64-64v-32h-47.1L272 448z'/></svg>";

    // POST to the hgSession JSON endpoint.  On {name,url} success call onResult(data); on {error}
    // or transport failure show the message in statusEl.
    function postJson(params, statusEl, onResult) {
        statusEl.style.color = "#000";
        statusEl.textContent = "Working…";
        $.ajax({
            type: "POST",
            url: "../cgi-bin/hgSession",
            data: params,
            dataType: "json",
            success: function(data) {
                if (data && data.url)
                    onResult(data);
                else {
                    statusEl.style.color = "#a00";
                    statusEl.textContent = (data && data.error) ? data.error : "Could not create link.";
                }
            },
            error: function() {
                statusEl.style.color = "#a00";
                statusEl.textContent = "Could not reach the server. Please try again.";
            }
        });
    }

    // Append the "how to manage this link" note (session mode only).  Logged in: point to the
    // My Sessions page.  Logged out: invite the user to log in (via the top-right Login link).
    function appendManageNote(body, loggedIn) {
        var note = el("p", {}, {marginTop: "14px"});
        if (loggedIn) {
            note.appendChild(document.createTextNode("All of your saved links appear on your "));
            note.appendChild(el("a", {href: "../cgi-bin/hgSession?hgS_doMainPage=1&hgsid=" +
                                            getHgsidSafe(),
                                      target: "_blank", textContent: "My Sessions"}));
            note.appendChild(document.createTextNode(
                " page, where you can rename, update, or remove them at any time."));
        } else {
            note.appendChild(document.createTextNode("You are not logged in. "));
            var loginEl = document.getElementById("loginLink");
            note.appendChild(el("a", {href: loginEl ? loginEl.getAttribute("href") : "../cgi-bin/hgLogin",
                                      textContent: "Log in"}));
            note.appendChild(document.createTextNode(
                " to give your links a name and to edit or update them later."));
        }
        body.appendChild(note);
    }

    // Show the resulting share URL with a "Copy to clipboard" button.  opts (session mode):
    //   {name: <current session name>, session: true, loggedIn: <bool>}.  A logged-in session link
    //   also gets a "Specify name" button.  url mode passes no opts → just the link + Copy.
    function showResult(body, url, opts) {
        opts = opts || {};
        var canRename = opts.session && opts.loggedIn;
        body.innerHTML = "";
        body.appendChild(el("p", {textContent: "You can share this link with collaborators, put " +
            "it into figure legends or manuscripts. Links never time out:"}, {marginTop: "0"}));
        // Read-only text region (not an <input>) so it's clear the URL isn't meant to be edited.
        var urlBox = el("div", {id: "tlShareUrl", textContent: url},
                        {background: "#f0f0f0", padding: "6px 8px", borderRadius: "4px",
                         wordBreak: "break-all", userSelect: "all", fontFamily: "monospace"});
        urlBox.setAttribute("data-copy", url);   // copyToClipboard() reads data-copy or innerText
        body.appendChild(urlBox);

        var btnRow = el("div", {}, {marginTop: "8px"});
        var copyBtn = el("button", {title: "Copy URL to clipboard"});
        copyBtn.setAttribute("data-target", "tlShareUrl");
        copyBtn.innerHTML = clipboardSvg + "Copy to clipboard";
        copyBtn.addEventListener("click", function(ev) {
            if (typeof copyToClipboard === "function") copyToClipboard(ev);
        });
        btnRow.appendChild(copyBtn);

        if (canRename) {
            var nameBtn = el("button", {textContent: "Specify name"}, {marginLeft: "8px"});
            nameBtn.addEventListener("click", function() { showRename(body, url, opts); });
            btnRow.appendChild(nameBtn);
        }
        body.appendChild(btnRow);

        if (opts.session)
            appendManageNote(body, opts.loggedIn);
        else if (opts.pageNote)
            body.appendChild(el("p", {textContent: "This link shows the page only. It does not " +
                "restore the tracks you currently have displayed."}, {marginTop: "14px"}));
    }

    // The "Specify name" editor: rename the session, then show the updated link.
    function showRename(body, currentUrl, opts) {
        body.innerHTML = "";
        body.appendChild(el("p", {textContent: "Name this link:"}, {marginTop: "0"}));
        var nameInput = el("input", {type: "text", value: "", placeholder: "e.g. my favorite view"},
                           {width: "100%", padding: "4px", margin: "6px 0", boxSizing: "border-box"});
        body.appendChild(nameInput);
        var status = el("div", {}, {margin: "6px 0"});
        body.appendChild(status);

        var row = el("div", {});
        var saveBtn = el("button", {textContent: "Save"});
        saveBtn.addEventListener("click", function() {
            var newName = nameInput.value.trim();
            if (!newName) {
                status.style.color = "#a00";
                status.textContent = "Please enter a name.";
                return;
            }
            postJson({hgsid: getHgsidSafe(), hgS_doRenameSessionJson: 1,
                      hgS_oldSessionName: opts.name, hgS_newSessionName: newName}, status,
                function(data) {
                    showResult(body, data.url, {name: data.name, session: true, loggedIn: true});
                });
        });
        row.appendChild(saveBtn);
        var cancelBtn = el("button", {textContent: "Cancel"}, {marginLeft: "8px"});
        cancelBtn.addEventListener("click", function() { showResult(body, currentUrl, opts); });
        row.appendChild(cancelBtn);
        body.appendChild(row);
        nameInput.focus();
    }

    // Return the current page URL with the hgsid query parameter removed (for hgTrackUi sharing).
    function stripHgsid(url) {
        try {
            var u = new URL(url);
            u.searchParams.delete("hgsid");
            return u.toString();
        } catch (e) {
            return url.replace(/([?&])hgsid=[^&]*/, "$1").replace(/[?&]$/, "");
        }
    }

    // Add name=val to url if it isn't already present (used to keep db= after stripping hgsid).
    function ensureParam(url, name, val) {
        try {
            var u = new URL(url);
            if (!u.searchParams.has(name))
                u.searchParams.set(name, val);
            return u.toString();
        } catch (e) {
            if (new RegExp("[?&]" + name + "=").test(url))
                return url;
            return url + (url.indexOf("?") >= 0 ? "&" : "?") + name + "=" + encodeURIComponent(val);
        }
    }

    // Open a simple "here is the link" dialog for an arbitrary URL, with the hgsid stripped.
    // Used by hgTrackUi (the current page) and by the hgc item-details popup in hgTracks.js.
    // opts (optional): {ensureDb: <db> to add db= if missing, pageNote: true to note it's page-only}.
    function shareUrlDialog(url, opts) {
        opts = opts || {};
        var clean = stripHgsid(url);
        if (opts.ensureDb)
            clean = ensureParam(clean, "db", opts.ensureDb);
        var body = document.createElement("div");
        showModal("Share a link", body, 720);
        showResult(body, clean, {pageNote: opts.pageNote});
    }

    function showShareDialog(link) {
        var mode = link.getAttribute("data-sharemode") || "session";

        // hgTrackUi etc.: the shareable thing is just this page's URL without the hgsid.  Note
        // that it opens the page, not the user's session/tracks (like the hgc popup link).
        if (mode === "url") {
            shareUrlDialog(window.location.href, {pageNote: true});
            return;
        }

        // Session mode (hgTracks): create a link right away so the user can just copy it.
        var loggedIn = link.getAttribute("data-loggedin") === "1";
        var body = document.createElement("div");
        var status = el("p", {textContent: "Creating link…"}, {marginTop: "0"});
        body.appendChild(status);
        showModal("Share a link", body, 720);
        var params = {hgsid: getHgsidSafe(), hgS_doSaveSessionJson: 1};
        if (!loggedIn)
            params.hgS_shareAnon = 1;   // anonymous token link; no rename
        postJson(params, status, function(data) {
            showResult(body, data.url, {name: data.name, session: true, loggedIn: loggedIn});
        });
    }

    // ---- Wire up the menu items --------------------------------------------------------------

    function init() {
        var login = document.getElementById("loginLink");
        // Only intercept the click when logged in (data-username present); otherwise it is a
        // plain link to hgSession and should navigate normally.
        if (login && login.getAttribute("data-username")) {
            login.addEventListener("click", function(ev) {
                ev.preventDefault();
                showLoginDialog(login);
            });
        }
        var share = document.getElementById("shareLink");
        if (share) {
            share.addEventListener("click", function(ev) {
                ev.preventDefault();
                showShareDialog(share);
            });
        }
        // Narrow-screen hamburger: toggle the dropdown of top-right links (CSS shows it only on
        // narrow viewports).  Close on an outside click or after a link inside is chosen.
        var toggle = document.getElementById("trToggle");
        var container = document.getElementById("topRightLinks");
        if (toggle && container) {
            toggle.addEventListener("click", function(ev) {
                ev.preventDefault();
                ev.stopPropagation();
                var open = container.classList.toggle("trOpen");
                toggle.setAttribute("aria-expanded", open ? "true" : "false");
            });
            container.addEventListener("click", function(ev) {
                if (ev.target.tagName === "A") {
                    container.classList.remove("trOpen");
                    toggle.setAttribute("aria-expanded", "false");
                }
            });
            document.addEventListener("click", function() {
                container.classList.remove("trOpen");
                toggle.setAttribute("aria-expanded", "false");
            });
        }
    }

    if (document.readyState === "loading")
        document.addEventListener("DOMContentLoaded", init);
    else
        init();

    return {showLoginDialog: showLoginDialog, showShareDialog: showShareDialog,
            shareUrl: shareUrlDialog};
})();
