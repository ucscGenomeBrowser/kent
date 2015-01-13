// Send requests to a CGI that returns JSON responses

var cart = (function() {
    // The CGI-generated HTML should include an inline script that sets window.hgsid:
    var hgsid = window.hgsid || '0';

    var cgi = 'cartJson';
    var baseUrl = '../cgi-bin/' + cgi + '?hgsid=' + hgsid;

    function commandObjToString(commandObj) {
        // Make sure commandObj has the correct structure: an object of objects.
        // Throws [message, badValue] if something is not as expected.
        var commandString, cgiVarString = '';
        if (commandObj) {
            if (! $.isPlainObject(commandObj)) {
	        throw(['commandObjToString: commandObj is not an object',
		       commandObj]);
            }
            Object.keys(commandObj).forEach(function(key) {
	        var value = commandObj[key];
	        if (value && ! $.isPlainObject(value)) {
	            throw(['commandObjToString: commandObj.' + key +
		          ' is not an object', value]);
	        }
            });
            var cmdNoCgiVar = {};
            Object.keys(commandObj).forEach(function(key) {
	        if (key !== 'cgiVar') {
	            cmdNoCgiVar[key] = commandObj[key];
	        }
            });
            commandString = 'cjCmd=' + encodeURIComponent(JSON.stringify(cmdNoCgiVar));
            // If CGI variables are specified, pull those out of commandObj and add
            // to the cgi string, so those are processed by the cart before commands
            // are executed.
            if (commandObj.cgiVar) {
	        Object.keys(commandObj.cgiVar).forEach(function(key) {
	            cgiVarString += '&' + key + '=' + commandObj.cgiVar[key];
	        });
	        commandString += cgiVarString;
            }
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

    function defaultErrorCallback(jqXHR, textStatus, errorThrown) {
        console.log('Request failed: ', arguments);
        alert('Request failed: ' + textStatus);
    }

    return {
        setCgi: function(newCgi) {
            cgi = newCgi;
            baseUrl = '../cgi-bin/' + cgi + '?hgsid=' + hgsid;
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
