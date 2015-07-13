// Send requests to a CGI that returns JSON responses
/* global FormData */

var cart = (function() {

    'use strict';

    // The CGI-generated HTML should include an inline script that sets window.hgsid:
    var hgsid = window.hgsid || '0';

    var cgiBinUrl = '../cgi-bin/';
    var cgiUrl;

    function wrapCommandObj(commandObj) {
        // Return an object suitable for use as $.ajax's data param, with settings
        // like hgsid and (if commandObj is non-empty) cjCmd with encoded commandObj.
        // Make sure commandObj has the correct structure: an object of objects.
        // Throws [message, badValue] if something is not as expected.
        var cmdNoCgiVar;
        var reqObj = {};
        reqObj.hgsid = hgsid;
        if (commandObj) {
            if (! $.isPlainObject(commandObj)) {
	        throw(['cart wrapCommandObj: commandObj is not an object', commandObj]);
            }
            // Make sure that commandObj children are objects.
            _.forEach(commandObj, function(value, key) {
	        if (value && ! _.isPlainObject(value)) {
	            throw(['cart wrapCommandObj: commandObj.' + key + ' is not an object', value]);
	        }
            });
            // Setting cart variables will be done the usual way, in the CGI request.
            // Make an object cmdNoCgiVar that contains all children of commandObj
            // except cgiVar if present, and stringify that as the value of cjCmd.
            cmdNoCgiVar = _.omit(commandObj, 'cgiVar');
            reqObj.cjCmd = JSON.stringify(cmdNoCgiVar);
            // If cart variables are specified, pull those out of commandObj and add
            // to reqObj.  They will be processed by the cart before commands are executed.
            _.assign(reqObj, commandObj.cgiVar);
        }
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
        console.log('Request failed: ', arguments);
        alert('Request failed: ' + textStatus);
    }

    return {

        setCgi: function(newCgi) {
            // Sets the name of the CGI (e.g. hgIntegrator, hgChooseDb etc).
            // This must be called before cart.send.
            cgiUrl = cgiBinUrl + newCgi;
        },

        send: function(commandObj, successCallback, errorCallback, ajaxExtra) {
            // Use $.ajax to POST a request to the server/CGI.
            // successCallback and the optional errorCallback are functions(jqXHR, textStatus)
            // ajaxExtra is an optional object containing extra params for $.ajax.
            // Throws [message, badValue] if something is not as expected.
            var reqObj = wrapCommandObj(commandObj);
            var paramString = reqToString(reqObj);
            var ajaxParams;
            errorCallback = errorCallback || defaultErrorCallback;
            if (! cgiUrl) {
                throw(['cart.send: cart.setCgi must be called before cart.send']);
            }
            console.log('cart.send: url =', cgiUrl, ', data =', reqObj, ', params =', paramString);
            ajaxParams = {
                type: "POST",
                url: cgiUrl,
                data: reqObj,
                dataType: 'json'
            };
            if (ajaxExtra) {
                _.assign(ajaxParams, ajaxExtra);
            }
            $.ajax(ajaxParams)
                   .done(successCallback)
                   .fail(errorCallback);
        },

        uploadFile: function(commandObj, jqFileInput, successCallback, errorCallback) {
            var reqObj = wrapCommandObj(commandObj);
            errorCallback = errorCallback || defaultErrorCallback;
            if (window.FormData) {
                // If running on a modern browser that supports FormData, use that to form
                // the data for the AJAX request:
                var formData = new FormData();
                formData.append(jqFileInput.attr('name'), jqFileInput[0].files[0]);
                _.forEach(reqObj, function(value, key) {
                    formData.append(key, value);
                });
                $.ajax({
                    type: 'POST',
                    url: cgiUrl,
                    data: formData,
                    dataType: 'json',
                    // These two are necessary for JQuery to not interfere with FormData:
                    processData: false,
                    contentType: false
                })
                  .done(successCallback)
                  .fail(errorCallback);
            } else {
                // Use JQuery plugin bifrost to upload the file using a hidden iframe as target,
                // in order to support IE <10.  It breaks on IE11 though, go figure.
                // Using 'iframe json' here activates jquery.bifrost:
                $.ajax({
                    type: 'POST',
                    url: cgiUrl,
                    data: reqObj,
                    dataType: 'iframe json',
                    fileInputs: jqFileInput
                })
                   .done(successCallback)
                   .fail(errorCallback);
            }

        }
    };
})();

// Without this, jshint complains that cart is not used.  Module system would help.
cart = cart;
