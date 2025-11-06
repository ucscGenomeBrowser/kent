// cart: Send cartJson requests to a CGI that returns JSON responses

// This creates a global object, cart, with methods to
// * set the CGI name to use in the URL
// * queue up cartJson commands, possibly including cgiVars, optionally with custom callbacks
// * queue up cartJson commands and upload a file input identified by a JQuery object
// * (IMPORTANT!) flush the queue; all cgiVars settings from all queued requests have been merged
//   and are sent along with each queued command object, to avoid cart-saving race conditions
//   on the server side.

// Typical usage (note: ImModel includes convenient wrapper methods, use those instead
// if you're subclassing ImModel):
//
// The CGI-generated HTML should include an inline script that sets window.hgsid.
//
// In model's initialize():
//     cart.setCgi('hgMyCgi');
//     cart.send({ getMyInitialState: {} });
//     cart.flush();
//
// In model's UI handler for when user changes some input or clicks on a button:
//     cart.send({ cgiVar: { someCartVar: newVal },
//                 doSomething: { what: 'etc' }
//               },
//               handleServerResponse, handleServerFailure);
//
// In model's UI handler for when user uploads a file:
//     cart.uploadFile({ cgiVar: { someCartVar: newVal },
//                       doSomething: { what: 'etc' }
//                     },
//                     jqueryFileInput,
//                     handleServerResponse, handleServerFailure);
//
// After all UI handlers have executed (ImModel does this):
//     cart.flush();

var cart = (function() {

    'use strict';

    // Private variables:

    var cgiBinUrl = '../cgi-bin/';
    // cart.setCgi(name) sets this to cgiBinUrl + name, and must be called before sending requests:
    var cgiUrl;
    var cgiName; // get sets by cart.setCgi()

    // accumulator for cgiVars passed in to send() before flush() is called:
    var cgiVars = {};

    // queue of commands from send() and uploadFile(), to send with accumulated cgiVars
    // when flush() is called:
    var requestQueue = [];

    // debugging flag for console.log messages
    var debug = false;

    // Private functions:

    function checkCommandObjType(commandObj) {
        // Make sure commandObj has the correct structure: an object of objects.
        // Throw [message, badValue] otherwise.
        if (! _.isPlainObject(commandObj)) {
	    throw(['cart: commandObj is not an object', commandObj]);
        }
        // Make sure that commandObj children are objects.
        _.forEach(commandObj, function(value, key) {
	    if (value && ! _.isPlainObject(value)) {
	        throw(['cart: commandObj.' + key + ' is not an object', value]);
	    }
        });
    }

    function requireCgiUrl() {
        // Make sure that cgiUrl has been set and return it.
        if (! cgiUrl) {
            throw(['cart.setCgi must be called before attempting to send request']);
        }
        return cgiUrl;
    }

    function mergeCgiVars(cgiVarObj) {
        // Merge settings in cgiVarObj with accumulator cgiVars.  Throw if there are
        // any conflicting settings.
        _.merge(cgiVars, cgiVarObj,
            function (oldVal, newVal, setting) {
                if (! _.isUndefined(oldVal) && oldVal !== newVal) {
                    throw [ 'cart mergeCgiVars: conflicting settings for ' + setting +
                            ': old value = "' + oldVal + '", new value = "' + newVal + '"'];
                }
            });
    }

    function processCommandObj(commandObj) {
        // In preparation for queueing this request, extract cgiVar settings
        // from commandObj and merge them with any settings from other requests
        // that have been queued.  Return a copy of commandObj with cgiVar
        // stripped out (if it's included in commandObj).
        checkCommandObjType(commandObj);
        requireCgiUrl();
        mergeCgiVars(commandObj.cgiVar);
        return _.omit(commandObj, 'cgiVar');
    }

    function wrapCommandObj(commandObjNoCgiVar) {
        // Return an object suitable for use as $.ajax's data param, with settings
        // like hgsid and any accumulated settings in cgiVars, and cjCmd set to
        // encoded commandObjNoCgiVar (if non-empty).
        // Throws [message, badValue] if something is not as expected.
        var reqObj = {};
        reqObj.hgsid = window.hgsid;
        if (commandObjNoCgiVar) {
            reqObj.cjCmd = JSON.stringify(commandObjNoCgiVar);
        }
        // Add cart variable settings (if any have been specified) to reqObj.
        // They will be processed by the cart before commands are executed.
        _.assign(reqObj, cgiVars);
        // Add a uniquifier so the browser doesn't use a cached copy:
        reqObj._ = new Date().getTime();
        return reqObj;
    }
    
    function reqToString(reqObj) {
        // Translate ajax request object into a CGI parameter string
        return _.map(reqObj, function(value, key) {
            return key + '=' + encodeURIComponent(value);
        }).join('&');
    }

    function defaultErrorCallback(jqXHR, textStatus) {
        // Ignore incomplete requests, likely due to navigating away from the page
        // http://stackoverflow.com/questions/9229005/how-to-handle-jquery-ajax-post-error-when-navigating-away-from-a-page
        if (jqXHR.readyState < 4) {
            return true;
        }
        console.error('Request failed: ', arguments);
        alert('Request failed: ' + textStatus);
    }

    function debugLog() {
        // If debug is true, use console.log to print info.
        if (debug) {
            console.log(arguments);
        }
    }

    function ajaxParamsForReq(reqObj) {
        // Return an object suitable as the argument to $.ajax for a POST reqObj to the server/CGI.
        var ajaxParams = {
            type: "POST",
            url: requireCgiUrl(),
            data: reqObj,
            dataType: 'json',
            xhrFields: {
                withCredentials: true,
            },
        };
        var paramString = reqToString(reqObj);
        debugLog('cart.flush: data =', reqObj, ', params = ' + paramString);
        return ajaxParams;
    }

    function ajaxParamsForReqWithFile(reqObj, jqFileInput) {
        // Return an object suitable as the argument to $.ajax for a POST reqObj to the server/CGI
        // with the contents of the file identified by jqFileInput.
        // Depending on whether the browser supports the FormData API, use either FormData
        // or a fallback plugin (jquery.bifrost).
        var ajaxParams = {
            type: 'POST',
            url: requireCgiUrl(),
        };
        var fileInputName = jqFileInput.attr('name');
        var paramString = reqToString(reqObj);
        if (window.FormData) {
            // If running on a modern browser that supports FormData, use that to form
            // the data for the AJAX request:
            var formData = new window.FormData();
            formData.append(fileInputName, jqFileInput[0].files[0]);
            _.forEach(reqObj, function(value, key) {
                formData.append(key, value);
            });
            _.assign(ajaxParams, {
                data: formData,
                dataType: 'json',
                // These two are necessary for JQuery to not interfere with FormData:
                processData: false,
                contentType: false
            });
            debugLog('cart.flush: posting FormData for input ' + fileInputName +
                     ', reqObj =', reqObj, ', params = ' + paramString);
        } else {
            // Use JQuery plugin bifrost to upload the file using a hidden iframe as target,
            // in order to support IE <10.  It breaks on IE11 though, go figure.
            _.assign(ajaxParams, {
                data: reqObj,
                // Using 'iframe json' here activates jquery.bifrost:
                dataType: 'iframe json',
                fileInputs: jqFileInput
            });
            debugLog('cart.flush: using jquery.bifrost plugin for input ' + fileInputName +
                     ', data =', reqObj, ', params = ' + paramString);
        }
        return ajaxParams;
    }

    // Return cart object with public methods.
    return {

        defaultErrorCallback: function (jqXHR, textStatus) {
            defaultErrorCallback(jqXHR, textStatus);
        },

        cgi: function() {
            return cgiName;
        },

        setCgi: function(newCgi) {
            // Sets the name of the CGI (e.g. hgIntegrator, hgChooseDb etc).
            // This must be called before cart.send.
            cgiName = newCgi;
            cgiUrl = cgiBinUrl + newCgi;
        },

        setCgiAndUrl: function(newUrl, cgiName) {
            // Sets the full URL of the CGI (e.g. hgIntegrator, hgChooseDb etc).
            // This must be called before cart.send.
            cgiName = cgiName;
            cgiUrl = newUrl;
        },

        send: function(commandObj, successCallback, errorCallback) {
            // Queue up commandObj and callbacks, merging cgiVars with those of othere queued reqs.
            // successCallback and the optional errorCallback are functions(jqXHR, textStatus)
            // Throws [message, badValue] if something is not as expected.
            var cmdObjNoCgiVar = processCommandObj(commandObj);
            // If this request contained only cgiVars (empty cmdObjNoCgiVar) then let those
            // go out with other requests.  Below, flush will make sure that at least one request
            // is sent out if there are cgiVars.
            if (! _.isEmpty(cmdObjNoCgiVar) || successCallback || errorCallback) {
                requestQueue.push({ commandObj: cmdObjNoCgiVar,
                                    successCallback: successCallback,
                                    errorCallback: errorCallback });
            }
        },

        uploadFile: function(commandObj, jqFileInput, successCallback, errorCallback) {
            // Queue up commandObj, jqFileInput and callbacks, merging cgiVars with those
            // of othere queued reqs.
            // successCallback and the optional errorCallback are functions(jqXHR, textStatus)
            // Throws [message, badValue] if something is not as expected.
            var cmdObjNoCgiVar = processCommandObj(commandObj);
            requestQueue.push({ commandObj: cmdObjNoCgiVar,
                                jqFileInput: jqFileInput,
                                successCallback: successCallback,
                                errorCallback: errorCallback });
        },

        flush: function() {
            // Use $.ajax to POST queued-up requests with accumulated cgiVars to the server/CGI.
            // If cgiVars have been given, but no cartJson commands, add one empty request to the
            // empty requestQueue so that the cgiVars are sent.
            if (! _.isEmpty(cgiVars) && _.isEmpty(requestQueue)) {
                requestQueue = [{ commandObj: {} }];
            }
            _.forEach(requestQueue,
                function(queuedReq) {
                    var reqObj = wrapCommandObj(queuedReq.commandObj);
                    var successCallback = queuedReq.successCallback || _.noop();
                    var errorCallback = queuedReq.errorCallback || defaultErrorCallback;
                    var ajaxParams;
                    if (queuedReq.jqFileInput) {
                        ajaxParams = ajaxParamsForReqWithFile(reqObj, queuedReq.jqFileInput);
                    } else {
                        ajaxParams = ajaxParamsForReq(reqObj);
                    }
                    $.ajax(ajaxParams).done(successCallback).fail(errorCallback);
                });
            cgiVars = {};
            requestQueue = [];
        },

        debug: function(isOn) {
            debug = isOn;
        }

    };
})();

// Without this, jshint complains that cart is not used.  Module system would help.
cart = cart;
