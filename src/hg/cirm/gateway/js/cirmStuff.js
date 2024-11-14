// cirmStuff - Generally useful functions for CIRM site pages
//
// Copyright (C) 2019 The Regents of the University of California
//
// This file contains a few functions and invocations for adjusting the look of pages on the
// public and private CIRM sites.  Depending on the site, users may be logged in using
// basic auth or using hgLogin.  Either way, login/logout buttons should be presented as
// appropriate (and those buttons should actually work).

var cirmSiteFunctions = (function() {

    var isSecureSite = function() {
        if ((document.domain == "cirmdcm.soe.ucsc.edu") ||
            (document.domain.search(/^cirm-01/) >= 0) ||
            (document.domain.search(/^hgwdev/) >= 0) ||
	    (document.domain.search(/sspsygene.gi.ucsc.edu/) >= 0)) {
            return true;
            }
        return false;
        };

    // copied from http://stackoverflow.com/questions/233507/how-to-log-out-user-from-web-site-using-basic-authentication
    function basicAuthLogout(secUrl, redirUrl) {
        if (bowser.msie) {
            document.execCommand('ClearAuthenticationCache', 'false');
        } else if (bowser.gecko) {
            $.ajax({
                async: false,
                url: secUrl,
                type: 'GET',
                username: 'logout'
            });
        } else if (bowser.webkit || bowser.chrome) {
            var xmlhttp = new XMLHttpRequest();
            xmlhttp.open('GET', secUrl, true);
            xmlhttp.setRequestHeader('Authorization', 'Basic logout');
            xmlhttp.send();
        } else {
            // from http://stackoverflow.com/questions/5957822/how-to-clear-basic-authentication-details-in-chrome
            redirUrl = url.replace('http://', 'http://' + new Date().getTime() + '@');
        }
        setTimeout(function () {
            window.location.href = redirUrl;
        }, 200);
    }

    // This happens at the time the script is being read, so of course it's the last script in the
    // DOM.
    var thisScriptUrl=$("script")[$("script").length-1].src;
    var userUrl = thisScriptUrl.replace(/js\/cirmStuff\.js.*$/i, "cgi-bin/cdwWebBrowse?cdwCommand=userName");
    function userNameUrl() {
        return userUrl;
    }

    return {
        basicAuthLogout: basicAuthLogout,
        isSecureSite: isSecureSite,
        userNameUrl: userNameUrl
    };
}()); // cirmSiteFunctions definition

// Screw with login/logout button
$(document).ready(function() {
    if (cirmSiteFunctions.isSecureSite()) {
	console.log("Secure site detected.");
        // Add returnto to login and logout link URLs
        $("a.login").attr('href', function(i,link){
            return link + "&returnto=" + document.baseURI;
            });
        // Adjust for basic or hgLogin Auth, add username to logout
        var loginRequest = new XMLHttpRequest();
        loginRequest.onload = function() {
	    console.log("Login request completed with stats:", this.status);
            if (this.status == 200) {
                var loginInfo = JSON.parse(this.responseText);
		console.log("Login info received:", loginInfo);
		console.log(loginInfo.username);
                if (typeof(loginInfo.username) !== 'undefined') {
		    console.log("Undefined username detected");
                    $("#logoutLink").text(function(i,oldText){
                        return oldText + " " + loginInfo.username;
                    });
                    $("a.login").toggle();
                }
                if (loginInfo.auth === "basic") {
		    console.log("Basic auth detected. Setting logout handler.");
                    $('#logoutLink').click(function(){
                        cirmSiteFunctions.basicAuthLogout('/', 'https://sspsygene.ucsc.edu/'); return false;
                        });
                }
            } else {
		console.warn("Failed to retrieve login info. Hiding logout link.");
	    }
        };
        var url = cirmSiteFunctions.userNameUrl();
	console.log("Fetching login info from:", url);
        loginRequest.open("GET", url, true);
        loginRequest.send();
    } else {
	console.log("Not a secure site. Hiding login and logout buttons.");
        $("a.login").hide();
    }
});

// Call googleAnalytics
$(document).ready(function() {
    var analyticsKey = "UA-135185279-1";
    if (cirmSiteFunctions.isSecureSite()) {
        analyticsKey = "UA-135185279-2";
    }
    // should keep in sync with kent/src/hg/lib/googleAnalytics.c
    (function(i,s,o,g,r,a,m){i['GoogleAnalyticsObject']=r;i[r]=i[r]||function(){
    (i[r].q=i[r].q||[]).push(arguments)},i[r].l=1*new Date();a=s.createElement(o),
    m=s.getElementsByTagName(o)[0];a.async=1;a.src=g;m.parentNode.insertBefore(a,m)
    })(window,document,'script','//www.google-analytics.com/analytics.js','ga');
    ga('create', analyticsKey, 'auto');
    ga('require', 'displayfeatures');
    ga('send', 'pageview');
});
