/* jshint esversion: 8 */

/* The API key controls on the Hub Development tab of hgHubConnect.
 *
 * A key gets a script past the download CAPTCHA and lets hubtools upload, and the keys
 * live in the central database of the server that handed them out. So this code is kept
 * apart from the hub upload UI in hgMyData.js: a mirror can show the API key section
 * (showHubApiKey) without running hubSpace (storeUserFiles), and then hgMyData.js and
 * everything it pulls in are not on the page at all.
 *
 * For the same reason the request goes to this host's hgHubConnect and not to the login
 * host: a key made on genome-euro belongs in genome-euro's central. */

var hubApiKey = (function() {

    function addSpinner(afterElementId) {
        // put a spinner after the button that was just clicked, unless one is already there
        if (document.getElementById("spinner")) {
            return;
        }
        let spinner = document.createElement("i");
        spinner.id = "spinner";
        spinner.classList.add("fa", "fa-spinner", "fa-spin");
        document.getElementById(afterElementId).after(spinner);
    }

    function removeSpinner() {
        let spinner = document.getElementById("spinner");
        if (spinner) {
            spinner.remove();
        }
    }

    function sendToThisHost(cartData, handleSuccess) {
        // keys are per-central, so always talk to the hgHubConnect of the site being read
        cart.setCgi("hgHubConnect");
        cart.send(cartData, handleSuccess);
        cart.flush();
    }

    function generate() {
        let apiKeyInstr = document.getElementById("apiKeyInstructions");
        let apiKeyDiv = document.getElementById("apiKey");

        addSpinner("generateApiKey");

        let handleSuccess = function(reqObj) {
            apiKeyDiv.textContent = reqObj.apiKey;
            apiKeyInstr.style.display = "block";
            let revokeDiv = document.getElementById("revokeDiv");
            revokeDiv.style.display = "block";
            removeSpinner();

            // remove the word 'already' from the message if we have just re-generated a key
            let refreshSpan = document.getElementById("removeOnGenerate");
            if (refreshSpan) {
                refreshSpan.style.display = "none";
            }
        };

        sendToThisHost({generateApiKey: {}}, handleSuccess);
    }

    function revoke() {
        let apiKeyInstr = document.getElementById("apiKeyInstructions");

        addSpinner("revokeApiKeys");

        let handleSuccess = function(req) {
            apiKeyInstr.style.display = "none";
            removeSpinner();
            let generateDiv = document.getElementById("generateDiv");
            generateDiv.style.display = "block";
            let revokeDiv = document.getElementById("revokeDiv");
            revokeDiv.style.display = "none";
        };

        sendToThisHost({revokeApiKey: {}}, handleSuccess);
    }

    return { generate: generate,
             revoke: revoke,
           };
}());
