// hgSession.js - the experimental client-rendered "My Sessions" page.
//
// An opt-in modern alternative to the classic server-rendered hgSession page, applying hgBlat's
// facelift strategy (#37996): hgSession.c emits the session list and page config as an inline JSON
// global (hgSessionData) into an empty #sessionApp container, and this file builds the UI - a
// save-current-view card, a searchable/sortable DataTable of saved sessions with inline
// Overwrite/Share/Edit/Delete, and an "Advanced" panel for loading and backup.
//
// The inline table actions POST to small JSON endpoints in hgSession.c (hgS_doDeleteJson, etc.) and
// update the table in place.  Navigation actions (load a session, load from URL/file, save to file,
// reset) are ordinary form submits/links against the existing hgSession actions.
//
// Styling: shared house-style components in gbModern.css (.gbPill, .gbCard, .gbModal*, .gbTable,
// .gbBanner, .gbSection), session-specific layout in hgSession.css.

/* global $, hgSessionData, convertTitleTagsToMouseovers, htmlEncode, commify */

// Cart action variables (must match the hgs* defines in hgSession.h; hgSessionPrefix is "hgS_").
var SESS_ACT = {
    save:      'hgS_doSaveSessionJson',
    rename:    'hgS_doRenameSessionJson',
    del:       'hgS_doDeleteJson',
    share:     'hgS_doShareJson',
    gallery:   'hgS_doGalleryJson',
    overwrite: 'hgS_doOverwriteJson',
    describe:  'hgS_doDescribeJson'
};
var SESS_P = {
    oldName:  'hgS_oldSessionName',
    newName:  'hgS_newSessionName',
    share:    'hgS_newSessionShare',
    descr:    'hgS_newSessionDescription',
    shareAnon:'hgS_shareAnon'
};

var sessData = null;   // set in sessionBuild: {config, sessions}
var sessDt = null;     // the DataTable API
var sessSelectMode = false;   // bulk-select (checkbox column) shown?

function sessEnc(s) {
    // HTML-escape via the shared utils.js helper (escapes quotes too, so it is attribute-safe).
    return (typeof htmlEncode === 'function') ? htmlEncode(String(s == null ? '' : s)) : String(s);
}

function sessNum(n) {
    return (typeof commify === 'function') ? commify(n) : String(n);
}

function sessRandomShareName() {
    // Mirror the server's auto/anonymous share-name convention ("share_" + 8 URL-safe alphanumeric
    // chars).  hgSession.c's doSaveSessionJson generates the same style server-side for the top-right
    // "Share a link"; we generate it here so the confirm dialog can show the name before saving.
    var chars = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789';
    var s = '';
    for (var i = 0; i < 8; i++) {
        s += chars.charAt(Math.floor(Math.random() * chars.length));
    }
    return 'share_' + s;
}

function sessCommifyPos(pos) {
    // Add thousands separators to each number in a position string ("chr7:155799529-155812871" ->
    // "chr7:155,799,529-155,812,871") using utils.js commify.
    if (typeof commify !== 'function') { return String(pos); }
    return String(pos).replace(/\d+/g, function(n) { return commify(n); });
}

function sessMsg(text, cls) {
    // Show a transient status line at the top of the app (cls: 'ok' | 'err' | '').
    var el = document.getElementById('sessMsg');
    if (!el) { return; }
    el.className = 'sessMsg' + (cls ? ' ' + cls : '');
    el.innerHTML = text ? sessEnc(text) : '';
}

// ---- AJAX ----------------------------------------------------------------
// Post an action to hgSession and get JSON back.  Always carry the session id so the CGI loads the
// right cart.  onOk(resp) is called for {success:true,...} or {name,url}; onErr(msg) for {error}.

function sessAjax(params, onOk, onErr) {
    var data = $.extend({}, params);
    data[sessData.config.cartVar] = sessData.config.hgsid;
    $.ajax({
        type: 'POST',
        url: 'hgSession',
        data: data,
        dataType: 'json',
        cache: false,
        success: function(resp) {
            if (resp && resp.error) {
                if (onErr) { onErr(resp.error); } else { sessMsg(resp.error, 'err'); }
            } else {
                if (onOk) { onOk(resp); }
            }
        },
        error: function() {
            var m = 'Sorry, that action could not be completed. Please try again.';
            if (onErr) { onErr(m); } else { sessMsg(m, 'err'); }
        }
    });
}

// ---- modal (generic) -----------------------------------------------------
// One reusable overlay lives inside #sessionApp (so the gbModern tokens inherit into it).  Open on
// backdrop click and Esc close, matching hgBlat's modal behavior.

function sessModalEnsure() {
    if (document.getElementById('sessModalBg')) { return; }
    var bg = document.createElement('div');
    bg.id = 'sessModalBg';
    bg.className = 'gbModalBg';
    bg.style.display = 'none';
    bg.innerHTML = '<div class="gbModal" role="dialog" aria-modal="true" id="sessModal"></div>';
    document.getElementById('sessionApp').appendChild(bg);
    $(bg).on('click', function(ev) { if (ev.target === this) { sessModalClose(); } });
    $(document).on('keydown.sessModal', function(ev) {
        var b = document.getElementById('sessModalBg');
        if (b && b.style.display !== 'none' && ev.key === 'Escape') { sessModalClose(); }
    });
}

function sessModalOpen(html) {
    sessModalEnsure();
    document.getElementById('sessModal').innerHTML = html;
    document.getElementById('sessModalBg').style.display = 'flex';
}

function sessModalClose() {
    var bg = document.getElementById('sessModalBg');
    if (bg) { bg.style.display = 'none'; }
}

// A confirmation dialog with Cancel + a primary/danger OK.  opts: {title, bodyHtml, okLabel,
// okClass, onOk}.  bodyHtml is caller-built safe HTML.  The OK button is focused on open so the
// user can confirm with just the Enter key.
function sessConfirm(opts) {
    var okClass = opts.okClass || 'primary';
    sessModalOpen(
        '<div class="gbModalTitle">' + sessEnc(opts.title) + '</div>' +
        '<div class="gbModalText">' + opts.bodyHtml + '</div>' +
        '<div class="gbModalBtns">' +
        '<button type="button" class="gbPill" id="sessCfCancel">Cancel</button>' +
        '<button type="button" class="gbPill ' + okClass + '" id="sessCfOk">' +
        sessEnc(opts.okLabel || 'OK') + '</button></div>');
    $('#sessCfCancel').on('click', sessModalClose);
    $('#sessCfOk').on('click', function() { opts.onOk(); });
    document.getElementById('sessCfOk').focus();
}

// ---- session lookup / row helpers ---------------------------------------

function sessByEnc(enc) {
    var list = sessData.sessions;
    for (var i = 0; i < list.length; i++) {
        if (list[i].encName === enc) { return list[i]; }
    }
    return null;
}

function sessRowByEnc(enc) {
    // Return the DataTables row API for the session with this encName, or null.
    var found = null;
    sessDt.rows().every(function() {
        if (this.data().encName === enc) { found = this; }
    });
    return found;
}

// ---- table cell rendering ------------------------------------------------

// Inline icons (Font Awesome solid paths, embedded as SVG so they do not depend on the site's Font
// Awesome version).  fill:currentColor picks up the button's text/danger color.
var SESS_TRASH_SVG = '<svg class="sessIcon" xmlns="http://www.w3.org/2000/svg" viewBox="0 0 448 512"' +
    ' aria-hidden="true"><path fill="currentColor" d="M135.2 17.7L128 32H32C14.3 32 0 46.3 0 64S14.3' +
    ' 96 32 96H416c17.7 0 32-14.3 32-32s-14.3-32-32-32H320l-7.2-14.3C307.4 6.8 296.3 0 284.2 0H163.8' +
    'c-12.1 0-23.2 6.8-28.6 17.7zM416 128H32L53.2 467c1.6 25.3 22.6 45 47.9 45H346.9c25.3 0 46.3-19.7' +
    ' 47.9-45L416 128z"/></svg>';
// Font Awesome "floppy-disk" (save) solid path.
var SESS_SAVE_SVG = '<svg class="sessIcon" xmlns="http://www.w3.org/2000/svg" viewBox="0 0 448 512"' +
    ' aria-hidden="true"><path fill="currentColor" d="M64 32C28.7 32 0 60.7 0 96V416c0 35.3 28.7 64' +
    ' 64 64H384c35.3 0 64-28.7 64-64V173.3c0-17-6.7-33.3-18.7-45.3L352 50.7C340 38.7 323.7 32 306.7' +
    ' 32H64zm0 96c0-17.7 14.3-32 32-32H288c17.7 0 32 14.3 32 32v64c0 17.7-14.3 32-32 32H96c-17.7 0-32' +
    '-14.3-32-32V128zM224 288a64 64 0 1 1 0 128 64 64 0 1 1 0-128z"/></svg>';
// Font Awesome "lock" solid path - marks a private (not-shared) session, since sharing is the default.
var SESS_LOCK_SVG = '<svg class="sessLock" xmlns="http://www.w3.org/2000/svg" viewBox="0 0 448 512"' +
    ' aria-hidden="true"><path fill="currentColor" d="M144 144v48H304V144c0-44.2-35.8-80-80-80s-80' +
    ' 35.8-80 80zM80 192V144C80 64.5 144.5 0 224 0s144 64.5 144 144v48h16c35.3 0 64 28.7 64 64V448c0' +
    ' 35.3-28.7 64-64 64H64c-35.3 0-64-28.7-64-64V256c0-35.3 28.7-64 64-64H80z"/></svg>';

function sessActionsHtml(row) {
    // Order: Share, Edit, then the icon-only Overwrite (floppy) and Delete (trash).
    var e = sessEnc(row.encName);
    return '<button type="button" class="gbPill" data-act="share" data-enc="' + e + '" ' +
        'title="Copy a shareable link or change who can see this session">Share</button>' +
        '<button type="button" class="gbPill" data-act="edit" data-enc="' + e + '" ' +
        'title="Rename this session or edit its description">Edit</button>' +
        '<button type="button" class="gbPill" data-act="overwrite" data-enc="' + e + '" ' +
        'aria-label="Overwrite with current view" ' +
        'title="Overwrite this session with your current browser view">' + SESS_SAVE_SVG + '</button>' +
        '<button type="button" class="gbPill danger" data-act="delete" data-enc="' + e + '" ' +
        'aria-label="Delete this session" title="Delete this session">' + SESS_TRASH_SVG + '</button>';
}

function sessNameCellHtml(row) {
    var html = '<a href="' + sessEnc(row.shareUrl) + '" ' +
        'title="Load this session in the Genome Browser">' + sessEnc(row.name) + '</a>';
    if (row.description) {
        html += ' <span class="sessInfo" title="' + sessEnc(row.description) + '">&#9432;</span>';
    }
    // Sessions are shared by default; mark only the exceptions: a lock for private, a badge for the
    // public gallery.  A plain shared-by-link session gets no marker.
    if (row.shared === 0) {
        html += ' <span class="sessLockWrap" title="Private — only you can load this session">' +
            SESS_LOCK_SVG + '</span>';
    } else if (row.shared >= 2) {
        html += ' <span class="sessPublic" title="Listed in the Public Sessions gallery">Public</span>';
    }
    return html;
}

// ---- Overwrite -----------------------------------------------------------

function sessDoOverwrite(row) {
    sessAjax(sessActParams(SESS_ACT.overwrite, row), function(resp) {
        row.useCount = resp.useCount;
        if (resp.created) { row.created = resp.created; }
        if (resp.db) { row.db = resp.db; }
        var r = sessRowByEnc(row.encName);
        if (r) { r.data(row).draw(false); }
        sessModalClose();
        sessMsg('Overwrote “' + row.name + '” with your current browser view.', 'ok');
    });
}

function sessOpenOverwrite(row) {
    sessConfirm({
        title: 'Overwrite session',
        bodyHtml: 'Replace the saved session <b>' + sessEnc(row.name) + '</b> with the view you are ' +
            'looking at now? The name and description stay the same; the previously saved view is lost.',
        okLabel: 'Overwrite',
        onOk: function() { sessDoOverwrite(row); }
    });
}

// ---- Delete --------------------------------------------------------------

function sessOpenDelete(row) {
    sessConfirm({
        title: 'Delete session',
        bodyHtml: 'Delete the session <b>' + sessEnc(row.name) + '</b>? This cannot be undone. ' +
            'Any shared links to it will stop working.',
        okLabel: 'Delete',
        okClass: 'danger',
        onOk: function() {
            sessAjax(sessActParams(SESS_ACT.del, row), function() {
                var r = sessRowByEnc(row.encName);
                if (r) { r.remove().draw(false); }
                var list = sessData.sessions;
                for (var i = 0; i < list.length; i++) {
                    if (list[i].encName === row.encName) { list.splice(i, 1); break; }
                }
                sessModalClose();
                sessMsg('Deleted “' + row.name + '”.', 'ok');
            });
        }
    });
}

// ---- Edit (rename + description) ----------------------------------------

function sessOpenEdit(row) {
    var html = '<div class="gbModalTitle">Edit session</div>' +
        '<label class="gbModalLabel" for="sessEditName">Name</label>' +
        '<input id="sessEditName" class="gbModalInput" type="text" maxlength="255">' +
        '<label class="gbModalLabel" for="sessEditDesc">Description ' +
        '(shown in the Public Sessions gallery)</label>' +
        '<textarea id="sessEditDesc" class="gbModalInput" rows="4"></textarea>' +
        '<label class="gbModalLabel" style="font-weight:400;color:inherit;margin-top:12px">' +
        '<input type="checkbox" id="sessEditPrivate"> Only I can load it ' +
        '(others cannot load it, even with the link)</label>' +
        '<div class="gbModalText err" id="sessEditErr" style="display:none"></div>' +
        '<div class="gbModalBtns">' +
        '<button type="button" class="gbPill" id="sessEditCancel">Cancel</button>' +
        '<button type="button" class="gbPill primary" id="sessEditOk">Save</button></div>';
    sessModalOpen(html);
    document.getElementById('sessEditName').value = row.name;
    document.getElementById('sessEditDesc').value = row.description || '';
    document.getElementById('sessEditPrivate').checked = (row.shared === 0);
    document.getElementById('sessEditName').focus();
    $('#sessEditCancel').on('click', sessModalClose);
    $('#sessEditOk').on('click', function() { sessSaveEdit(row); });
}

function sessSaveEdit(row) {
    var newName = document.getElementById('sessEditName').value.trim();
    var newDesc = document.getElementById('sessEditDesc').value;
    if (!newName) {
        var err = document.getElementById('sessEditErr');
        err.style.display = 'block';
        err.innerHTML = sessEnc('Please enter a name.');
        return;
    }
    var descChanged = (newDesc !== (row.description || ''));
    var nameChanged = (newName !== row.name);
    var wantPrivate = document.getElementById('sessEditPrivate').checked;
    var privChanged = (wantPrivate !== (row.shared === 0));

    // Order matters: apply description and privacy while the session still has its OLD name, then
    // rename last (renaming changes the DB key the other endpoints look up by).
    function finish() {
        if (nameChanged) {
            // encName changes on rename; reload for authoritative state.
            window.location.reload();
        } else {
            var r = sessRowByEnc(row.encName);
            if (r) { r.data(row).draw(false); }
            sessModalClose();
            sessMsg('Saved changes to “' + row.name + '”.', 'ok');
        }
    }
    function doRename() {
        if (nameChanged) {
            var rp = sessActParams(SESS_ACT.rename, row);
            rp[SESS_P.newName] = newName;
            sessAjax(rp, finish, function(m) { sessEditError(m); });
        } else { finish(); }
    }
    function doPriv() {
        if (privChanged) {
            var sp = sessActParams(SESS_ACT.share, row);
            sp[SESS_P.share] = wantPrivate ? 0 : 1;
            sessAjax(sp, function(resp) { row.shared = resp.shared; doRename(); },
                     function(m) { sessEditError(m); });
        } else { doRename(); }
    }
    function doDesc() {
        if (descChanged) {
            var dp = sessActParams(SESS_ACT.describe, row);
            dp[SESS_P.descr] = newDesc;
            sessAjax(dp, function() { row.description = newDesc; doPriv(); },
                     function(m) { sessEditError(m); });
        } else { doPriv(); }
    }
    doDesc();
}

function sessEditError(msg) {
    var err = document.getElementById('sessEditErr');
    if (err) { err.style.display = 'block'; err.innerHTML = sessEnc(msg); }
    else { sessMsg(msg, 'err'); }
}

// ---- Share (copy link + sharing level) ----------------------------------

function sessOpenShare(row) {
    var mailBody = encodeURIComponent('Here is a UCSC Genome Browser session I would like to ' +
        'share with you: ') + encodeURIComponent(row.shareUrl);
    var mailto = 'mailto:?subject=' + encodeURIComponent('UCSC Genome Browser session ' + row.name) +
        '&body=' + mailBody;
    var html = '<div class="gbModalTitle">Share “' + sessEnc(row.name) + '”</div>' +
        '<div class="gbShareBox" style="padding:0;border:0;background:none">' +
        '<input id="sessShareUrl" class="gbShareInput" type="text" readonly>' +
        '<button type="button" class="gbPill" id="sessShareCopy" ' +
        'title="Copy the link to the clipboard">Copy</button>' +
        '<a class="gbPill" id="sessShareEmail" href="' + sessEnc(mailto) + '" ' +
        'title="Compose an email with this link">Email</a></div>' +
        '<div class="gbModalText" style="margin-top:14px">Sessions are loadable by anyone with the ' +
        'link by default; use <b>Edit</b> to make one private.</div>' +
        '<label class="gbModalLabel" style="font-weight:400;color:inherit">' +
        '<input type="checkbox" id="sessGalleryChk"> List it in the ' +
        '<a href="' + sessEnc(sessData.config.publicSessionsUrl) + '" target="_blank">' +
        'Public Sessions</a> gallery</label>' +
        '<div class="gbModalText err" id="sessShareErr" style="display:none"></div>' +
        '<div class="gbModalBtns">' +
        '<button type="button" class="gbPill primary" id="sessShareClose">Done</button></div>';
    sessModalOpen(html);
    var inp = document.getElementById('sessShareUrl');
    inp.value = row.shareUrl;
    document.getElementById('sessGalleryChk').checked = (row.shared >= 2);
    $('#sessShareClose').on('click', sessModalClose);
    $('#sessShareCopy').on('click', function() {
        inp.focus(); inp.select();
        if (navigator.clipboard) { navigator.clipboard.writeText(row.shareUrl); }
        else { try { document.execCommand('copy'); } catch (e) { /* ignore */ } }
        this.textContent = 'Copied';
    });
    $('#sessGalleryChk').on('change', function() { sessSetGallery(row, this.checked ? 1 : 0); });
}

function sessShareErr(msg) {
    var err = document.getElementById('sessShareErr');
    if (err) { err.style.display = 'block'; err.innerHTML = sessEnc(msg); }
}

function sessAfterSharedChange(row, newShared) {
    row.shared = newShared;
    var c = document.getElementById('sessShareChk');
    var g = document.getElementById('sessGalleryChk');
    if (c) { c.checked = (newShared >= 1); }
    if (g) { g.checked = (newShared >= 2); }
    var r = sessRowByEnc(row.encName);
    if (r) { r.data(row).draw(false); }
}

function sessSetGallery(row, want) {
    var p = sessActParams(SESS_ACT.gallery, row);
    p[SESS_P.share] = want;
    sessAjax(p, function(resp) { sessAfterSharedChange(row, resp.shared); }, function(m) {
        sessShareErr(m);
        document.getElementById('sessGalleryChk').checked = (row.shared >= 2);
    });
}

// Build the base params for an action on a given session (decoded name; the CGI re-encodes it).
function sessActParams(action, row) {
    var p = {};
    p[action] = '1';
    p[SESS_P.oldName] = row.name;
    return p;
}

// ---- Save current view ---------------------------------------------------

function sessDoSave() {
    var name = document.getElementById('sessSaveName').value.trim();
    if (!name) {
        // Empty name: offer to save under a server-style random "share_XXXXXXXX" name, after
        // confirming the user really meant to leave it blank.
        var rand = sessRandomShareName();
        sessConfirm({
            title: 'Save without a name?',
            bodyHtml: 'You left the session name empty. Your session will be saved under the ' +
                'randomly generated name <b>' + sessEnc(rand) + '</b>.<br><br>You can also create ' +
                'these quick share links any time from the <b>Share a link</b> option at the top ' +
                'right of every Genome Browser page.',
            okLabel: 'Save session',
            onOk: function() { sessModalClose(); sessDoSaveWithName(rand); }
        });
        return;
    }
    sessDoSaveWithName(name);
}

function sessDoSaveWithName(name) {
    var priv = document.getElementById('sessSavePrivate').checked;
    var descEl = document.getElementById('sessSaveDesc');
    var desc = descEl ? descEl.value.trim() : '';
    var p = {};
    p[SESS_ACT.save] = '1';
    p[SESS_P.newName] = name;
    // doSaveSessionJson always saves shared-by-link (the default); chain the optional description
    // and, if the user asked for "only I can load it", make it private, then reload to show the row.
    function afterDesc() {
        if (priv) {
            var sp = {};
            sp[SESS_ACT.share] = '1';
            sp[SESS_P.oldName] = name;
            sp[SESS_P.share] = 0;
            sessAjax(sp, function() { window.location.reload(); },
                     function() { window.location.reload(); });
        } else {
            window.location.reload();
        }
    }
    sessAjax(p, function() {
        if (desc) {
            var dp = {};
            dp[SESS_ACT.describe] = '1';
            dp[SESS_P.oldName] = name;
            dp[SESS_P.descr] = desc;
            sessAjax(dp, afterDesc, afterDesc);
        } else {
            afterDesc();
        }
    });
}

// ---- Advanced panel (navigation forms) ----------------------------------

function sessAdvancedHtml(C) {
    var sid = '<input type="hidden" name="' + sessEnc(C.cartVar) + '" value="' + sessEnc(C.hgsid) + '">';
    var loadUser = '';
    if (C.loggedIn) {
        loadUser =
        '<div class="sessAdvItem"><span class="lab">Load another user’s session</span>' +
        '<form class="sessAdvRow" action="hgSession" method="POST">' + sid +
        '<input class="sessAdvInput" type="text" name="hgS_otherUserName" placeholder="User">' +
        '<input class="sessAdvInput" type="text" name="hgS_otherUserSessionName" placeholder="Session name">' +
        '<button type="submit" class="gbPill" name="hgS_doOtherUser" value="submit" ' +
        'title="Load the named session belonging to another user">Load</button></form></div>';
    }
    var loadUrl =
        '<div class="sessAdvItem"><span class="lab">Load settings from a URL</span>' +
        '<form class="sessAdvRow" action="hgSession" method="POST">' + sid +
        '<input class="sessAdvInput" type="text" name="hgS_loadUrlName" placeholder="https://…">' +
        '<button type="submit" class="gbPill" name="hgS_doLoadUrl" value="submit" ' +
        'title="Load browser settings from a session file at this URL">Load</button></form></div>';
    var loadFile =
        '<div class="sessAdvItem"><span class="lab">Load settings from a file</span>' +
        '<form class="sessAdvRow" action="hgSession" method="POST" enctype="multipart/form-data">' + sid +
        '<input class="sessAdvInput" type="file" name="hgS_loadLocalFileName">' +
        '<button type="submit" class="gbPill" name="hgS_doLoadLocal" value="submit" ' +
        'title="Load browser settings from a session file on your computer">Load</button></form></div>';
    var saveFile =
        '<div class="sessAdvItem"><span class="lab">Save settings to a file</span>' +
        '<form class="sessAdvRow" action="hgSession" method="POST">' + sid +
        '<input class="sessAdvInput" type="text" name="hgS_saveLocalFileName" ' +
        'placeholder="File name (blank = show in browser)">' +
        '<label class="sessSaveCheck"><input type="checkbox" name="hgS_saveLocalFileCompress" ' +
        'value="gzip"> gzip</label>' +
        '<button type="submit" class="gbPill" name="hgS_doSaveLocal" value="submit" ' +
        'title="Download the current browser settings as a session file">Save</button></form></div>';
    var backup =
        '<div class="sessAdvItem"><span class="lab">Back up custom tracks</span>' +
        '<form class="sessAdvRow" action="hgSession" method="POST">' + sid +
        '<button type="submit" class="gbPill" name="hgS_showDownload_" value="Submit" ' +
        'title="Download your custom tracks as a .tar.gz archive you can reload later">' +
        'Back up custom tracks (.tar.gz)</button></form></div>';
    var other =
        '<div class="sessAdvItem"><span class="lab">Other</span>' +
        '<div class="sessAdvLinks">' +
        '<a href="' + sessEnc(C.resetUrl) + '" ' +
        'title="Reset all browser settings to their defaults">Reset the browser to defaults</a>' +
        '</div></div>';

    return '<div class="sessAdv">' +
        '<div class="sessAdvHead" id="sessAdvHead"><span class="caret">▸</span>' +
        '<span>Advanced — load another user’s session, load from a URL or file, ' +
        'save to a file, reset the browser</span></div>' +
        '<div class="sessAdvBody" id="sessAdvBody" style="display:none">' +
        loadUser + loadUrl + loadFile + saveFile + backup + other + '</div></div>';
}

// ---- build the whole page -----------------------------------------------

function sessAccountHtml(C) {
    // The signed-in account line (Signed in as X · Sign out · Change password) now lives in the
    // top-right menu bar, so it is intentionally not rendered here.  Kept commented out so QA can
    // add it back if wanted:
    /*
    if (C.loggedIn) {
        var s = 'Signed in as <b>' + sessEnc(C.userName) + '</b>';
        if (C.logoutUrl) { s += ' · <a href="' + sessEnc(C.logoutUrl) + '">Sign out</a>'; }
        if (C.changePasswordUrl) {
            s += ' · <a href="' + sessEnc(C.changePasswordUrl) + '">Change password</a>';
        }
        return s;
    }
    */
    if (!C.loggedIn && C.loginAvail && C.loginUrl) {
        return 'You are not signed in. <a href="' + sessEnc(C.loginUrl) +
            '">Sign in</a> to save and manage named sessions.';
    }
    return '';
}

function sessSaveCardHtml(C) {
    if (!C.loggedIn) { return ''; }
    // Assembly and position separated by a colon (e.g. "hg38: chr7:1-1,000"); assembly is already
    // the accession for hubs (trackHubSkipHubName on the server).
    var loc = '';
    if (C.db && C.position) { loc = sessEnc(C.db) + ': ' + sessEnc(C.position); }
    else if (C.db) { loc = sessEnc(C.db); }
    else if (C.position) { loc = sessEnc(C.position); }
    if (loc && C.trackCount) {
        loc += ', ' + sessNum(C.trackCount) + ' track' + (C.trackCount === 1 ? '' : 's') + ' shown';
    }
    var what = loc ? '<span class="sessSaveWhat">' + loc + '</span>' : '';
    return '<div class="sessSaveCard">' +
        '<div class="sessSaveHead"><span class="sessSaveTitle">' +
        'Save the current view as a stable session link</span>' + what + '</div>' +
        '<div class="sessSaveRow">' +
        '<input id="sessSaveName" class="sessSaveInput" type="text" maxlength="255" ' +
        'placeholder="Session name — or leave empty to save with a randomly generated name">' +
        '<label class="sessSaveCheck"><input type="checkbox" id="sessSavePrivate"> ' +
        'Only I can load it</label>' +
        '<button type="button" class="gbPill primary" id="sessSaveBtn" ' +
        'title="Save your current browser view as a named session">Save session</button>' +
        '</div>' +
        '<input id="sessSaveDesc" class="sessSaveInput" type="text" maxlength="512" ' +
        'placeholder="Description (optional) — shown on hover and in the Public Sessions gallery">' +
        '</div>';
}

function sessRecentHtml(recent) {
    // A quick shortcut to re-save the session the user most recently saved, keeping its name and
    // description.  Hidden when there are no saved sessions.
    if (!recent) { return ''; }
    return '<div class="sessRecent">Most recently saved session: <b>' + sessEnc(recent.name) +
        '</b> <span class="sessRecentTime">(' + sessEnc(recent.lastUse) + ')</span> ' +
        '<button type="button" class="gbPill" id="sessUpdateNow" ' +
        'title="Overwrite this session with the currently active view; keeps the session name and ' +
        'description identical">Update now</button></div>';
}

function sessTableHtml(C) {
    if (!C.loggedIn) { return ''; }
    return '<div class="gbSection">Your saved sessions</div>' +
        '<table id="sessionAppTable" class="gbTable" style="width:100%"></table>';
}

function sessionBuild() {
    sessData = hgSessionData;
    var C = sessData.config;
    var app = $('#sessionApp');

    var intro = '<div class="sessIntro">' + sessAccountHtml(C) +
        (sessAccountHtml(C) ? '<br>' : '') +
        'A session is a stable link to a Genome Browser view that you can save, load later, share ' +
        'or copy into a manuscript. See the ' +
        '<a href="' + sessEnc(C.helpUrl) + '" target="_blank">Sessions User’s Guide</a> and the ' +
        '<a href="' + sessEnc(C.galleryUrl) + '" target="_blank">Session Gallery</a>.</div>';

    // The session most recently saved/overwritten (max lastUse), for the one-click "Update now".
    var recent = null;
    (sessData.sessions || []).forEach(function(s) {
        if (!recent || s.lastUseEpoch > recent.lastUseEpoch) { recent = s; }
    });

    app.html(
        intro +
        '<div id="sessMsg" class="sessMsg"></div>' +
        sessRecentHtml(recent) +
        sessSaveCardHtml(C) +
        sessAdvancedHtml(C) +
        sessTableHtml(C)
    );

    // Save current view.  Enter in either the name or the description field saves.
    $('#sessSaveBtn').on('click', sessDoSave);
    $('#sessSaveName, #sessSaveDesc').on('keydown', function(ev) {
        if (ev.key === 'Enter') { ev.preventDefault(); sessDoSave(); }
    });

    // One-click update of the most recently saved session.
    if (recent) {
        $('#sessUpdateNow').on('click', function() {
            sessConfirm({
                title: 'Update session',
                bodyHtml: 'Overwrite <b>' + sessEnc(recent.name) + '</b> with the view you are ' +
                    'looking at now? The session name and description stay the same.',
                okLabel: 'Update now',
                onOk: function() { sessDoOverwrite(recent); }
            });
        });
    }

    // Advanced toggle.
    $('#sessAdvHead').on('click', function() {
        var body = document.getElementById('sessAdvBody');
        var open = body.style.display !== 'none';
        body.style.display = open ? 'none' : 'grid';
        $(this).find('.caret').html(open ? '▸' : '▾');
    });

    // Session table.
    if (C.loggedIn) { sessBuildTable(); }

    if (typeof convertTitleTagsToMouseovers === 'function') { convertTitleTagsToMouseovers(); }
}

// ---- bulk select / delete ------------------------------------------------

function sessToggleSelect() {
    if (sessSelectMode) { sessDeleteSelected(); } else { sessSetSelectMode(true); }
}

function sessSetSelectMode(on) {
    sessSelectMode = on;
    sessDt.column(0).visible(on);   // the leading checkbox column
    var b = document.getElementById('sessSelectBtn');
    if (on) {
        b.textContent = 'Delete all selected';
        b.classList.add('primary');
    } else {
        b.textContent = 'Select';
        b.classList.remove('primary');
        $('#sessionAppTable .sessSelChk, #sessSelAll').prop('checked', false);
    }
}

function sessDeleteSelected() {
    var encs = [];
    $('#sessionAppTable tbody .sessSelChk:checked').each(function() {
        encs.push(this.getAttribute('data-enc'));
    });
    if (encs.length === 0) { sessSetSelectMode(false); return; }   // nothing picked: just exit
    sessConfirm({
        title: 'Delete selected sessions',
        bodyHtml: 'Delete <b>' + encs.length + '</b> selected session' +
            (encs.length === 1 ? '' : 's') + '? This cannot be undone.',
        okLabel: 'Delete ' + encs.length,
        okClass: 'danger',
        onOk: function() {
            sessModalClose();
            var remaining = encs.length;
            function done() { if (--remaining === 0) { window.location.reload(); } }
            encs.forEach(function(enc) {
                var row = sessByEnc(enc);
                if (!row) { done(); return; }
                sessAjax(sessActParams(SESS_ACT.del, row), done, done);
            });
        }
    });
}

function sessBuildTable() {
    sessDt = $('#sessionAppTable').DataTable({
        data: sessData.sessions,
        pageLength: 15,
        lengthChange: true,
        lengthMenu: [[15, 50, 100, -1], [15, 50, 100, 'All']],
        order: [[3, 'desc']],   // newest (Created) first by default
        dom: '<"sessToolbar"fl>rt<"sessFoot"ip>',
        language: { searchPlaceholder: 'Search sessions', search: '', lengthMenu: 'Show _MENU_',
                    emptyTable: 'No saved sessions yet.' },
        columns: [
            { title: '<input type="checkbox" id="sessSelAll" title="Select all on this page">',
              className: 'sessSelCol', data: null, orderable: false, searchable: false, visible: false,
              render: function(d, type, row) {
                  return '<input type="checkbox" class="sessSelChk" data-enc="' +
                      sessEnc(row.encName) + '">'; } },
            { title: 'Session', className: 'sessNameCol', data: 'name',
              render: function(d, type, row) {
                  return (type === 'display') ? sessNameCellHtml(row) : row.name; } },
            { title: 'Assembly', data: 'db',
              render: function(d, type, row) {
                  if (type !== 'display') { return row.db || ''; }
                  var html = sessEnc(row.db || 'n/a');
                  if (row.position) {
                      html += ' <span class="sessPos">' + sessEnc(sessCommifyPos(row.position)) +
                          '</span>';
                  }
                  return html; } },
            { title: 'Created', data: 'createdEpoch',
              render: function(d, type, row) {
                  return (type === 'display') ? sessEnc(row.created) : row.createdEpoch; } },
            { title: 'Last used', data: 'lastUseEpoch',
              render: function(d, type, row) {
                  return (type === 'display') ? sessEnc(row.lastUse) : row.lastUseEpoch; } },
            { title: 'Views', data: 'useCount',
              render: function(d, type) { return (type === 'display') ? sessNum(d) : d; } },
            { title: 'Actions', className: 'sessActionsCol', data: null, orderable: false,
              searchable: false,
              render: function(d, type, row) { return (type === 'display') ? sessActionsHtml(row) : ''; } }
        ]
    });

    // Delegate the row action buttons.
    $('#sessionAppTable tbody').on('click', 'button[data-act]', function() {
        var act = this.getAttribute('data-act');
        var row = sessByEnc(this.getAttribute('data-enc'));
        if (!row) { return; }
        if (act === 'overwrite') { sessOpenOverwrite(row); }
        else if (act === 'share') { sessOpenShare(row); }
        else if (act === 'edit') { sessOpenEdit(row); }
        else if (act === 'delete') { sessOpenDelete(row); }
    });

    // Bulk-select control: a "Select" button after the length dropdown reveals a checkbox column and
    // becomes a primary "Delete all selected" button.
    var selBtn = document.createElement('button');
    selBtn.type = 'button';
    selBtn.id = 'sessSelectBtn';
    selBtn.className = 'gbPill';
    selBtn.textContent = 'Select';
    selBtn.title = 'Select multiple sessions to delete them at once';
    document.querySelector('#sessionApp .sessToolbar').appendChild(selBtn);
    $(selBtn).on('click', sessToggleSelect);
    // Header "select all" toggles every checkbox on the current page.
    $('#sessionAppTable').on('change', '#sessSelAll', function() {
        $('#sessionAppTable tbody .sessSelChk').prop('checked', this.checked);
    });

    // Column-header tooltips (set title, then let utils.js convert to styled mouseovers).
    var tips = {
        'Session': 'Click a name to load that session in the Genome Browser',
        'Assembly': 'The genome assembly this session was saved on',
        'Created': 'When the session was first saved',
        'Last used': 'When the session was last saved or loaded',
        'Views': 'How many times this session has been loaded',
        'Actions': 'Overwrite with your current view, share, edit, or delete'
    };
    $('#sessionAppTable thead th').each(function() {
        var t = tips[$(this).text().trim()];
        if (t) { $(this).attr('title', t); }
    });
}

$(document).ready(function() {
    if (typeof hgSessionData !== 'undefined' && document.getElementById('sessionApp')) {
        sessionBuild();
    }
});
