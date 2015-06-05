// Send requests to a CGI that returns JSON responses

var cart = (function() {

    'use strict';

    // The CGI-generated HTML should include an inline script that sets window.hgsid:
    var hgsid = window.hgsid || '0';

    var cgi = 'cartJson';
    var baseUrl = '../cgi-bin/' + cgi + '?hgsid=' + hgsid;

    function commandObjToString(commandObj) {
        // Make sure commandObj has the correct structure: an object of objects.
        // Throws [message, badValue] if something is not as expected.
        var commandString, cmdNoCgiVar;
        if (commandObj) {
            if (! $.isPlainObject(commandObj)) {
	        throw(['commandObjToString: commandObj is not an object',
		       commandObj]);
            }
            // Make sure that commandObj children are objects.
            _.forEach(commandObj, function(value, key) {
	        if (value && ! _.isPlainObject(value)) {
	            throw(['commandObjToString: commandObj.' + key +
		          ' is not an object', value]);
	        }
            });
            // Setting cart variables will be done the usual way, in the CGI request string.
            // Make an object cmdNoCgiVar that contains all children of commandObj
            // except cgiVar if present.
            cmdNoCgiVar = _.omit(commandObj, 'cgiVar');
            commandString = 'cjCmd=' + encodeURIComponent(JSON.stringify(cmdNoCgiVar));
            // If cart variables are specified, pull those out of commandObj and add
            // to the CGI string.  They will be processed by the cart before commands
            // are executed.
            _.forEach(commandObj.cgiVar, function(value, key) {
	        commandString += '&' + key + '=' + value;
	    });
        }
        return commandString;
    }
    
    function makeUrl(commandString) {
        var url = baseUrl;
        if (commandString) {
            url += '&' + commandString;
        }
        // Add a uniquifier at the end so the browser doesn't use a cached copy:
        url += '&_=' + new Date().getTime();
        return url;
    }

    function defaultErrorCallback(jqXHR, textStatus) {
        console.log('Request failed: ', arguments);
        alert('Request failed: ' + textStatus);
    }

    return {
        setCgi: function(newCgi) {
            cgi = newCgi;
            baseUrl = '../cgi-bin/' + cgi + '?hgsid=' + hgsid;
        },

        getBaseUrl: function() {
            return baseUrl;
        },

        send: function(commandObj, successCallback, errorCallback) {
            var commandString = commandObjToString(commandObj);
            var url = makeUrl(commandString);
            console.log(url);
            errorCallback = errorCallback || defaultErrorCallback;
            $.getJSON(url).done(successCallback).fail(errorCallback);
        }
    };
})();

// Without this, jshint complains that cart is not used.  Module system would help.
cart = cart;
