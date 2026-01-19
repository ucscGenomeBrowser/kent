var assembly1Value = "";
var assembly2Value = "";
var genome1 = "";
var genome2 = "";

/* check if a file exists at the specified URL */
function fileExists(url, callback) {
    $.ajax({
        type: 'HEAD',
        url: url,
        success: function() {
            callback(true);  // file exists
        },
        error: function() {
            callback(false); // file doesn't exist or not accessible
        }
    });
}

// Convert GCA_029289425.2 into the path: GCA/028/289/425/GCA_029289425.2
function gcPath(identifier) {
    var parts = identifier.split('_');
    var prefix = parts[0];
    var numeric = parts[1].split('.')[0];
    var groups = numeric.match(/.{1,3}/g);
    return prefix + "/" + groups.join("/") + "/" + identifier;
}

function liftOverPath(asm1, asm2) {
    var liftPath = "";
    if (/^GC[AF]_/.test(asm1)) {
        liftPath = "https://hgdownload.soe.ucsc.edu/hubs/";
        liftPath += gcPath(asm1);
    } else {
        liftPath = "https://hgdownload.soe.ucsc.edu/goldenPath/";
        liftPath += asm1;
    }
    liftPath += "/liftOver/" + asm1 + "To";
    liftPath += asm2.charAt(0).toUpperCase() + asm2.slice(1);
    liftPath += ".over.chain.gz";
    return(liftPath);
}

function checkAssemblyCompatibility(asm1, asm2) {
    $.ajax({
        url: "/cgi-bin/hubApi/liftOver/listExisting?fromGenome=" + encodeURIComponent(asm1) +
             ";" + "toGenome=" + encodeURIComponent(asm2),
        success: function(response) {
            console.log(JSON.stringify(response, null, 2));
            if (response.itemsReturned === 1) {
               var liftPath1 = liftOverPath(asm1, asm2);
               var liftPath2 = liftOverPath(asm2, asm1);
               var browser1 = "/cgi-bin/hgTracks?db=" + asm1;
               var browser2 = "/cgi-bin/hgTracks?db=" + asm2
               fileExists(liftPath1, function(exists) {
                   if (exists) {
                   document.getElementById("genome1Link").href = browser1;
                   document.getElementById("genome1Link").textContent = assembly1Value;

                   document.getElementById("genome1LiftOver").href = liftPath1;
                   document.getElementById("genome1LiftOver").textContent = asm1 + " to " + asm2;

                   // Make visible
                  document.getElementById("liftExists").style.display = "block";
                   }
               });
               fileExists(liftPath2, function(exists) {
                   if (exists) {
                   document.getElementById("genome2Link").href = browser2;
                   document.getElementById("genome2Link").textContent = assembly2Value;

                   document.getElementById("genome2LiftOver").href = liftPath2;
                   document.getElementById("genome2LiftOver").textContent = asm2 + " to " + asm1;
                   // Make visible
                  document.getElementById("liftExists").style.display = "block";
                   }
               });
            }
        }
    });
}

function checkBothAssembliesSelected() {
    if (genome1 && genome2) { // Both assemblies are now selected
        checkAssemblyCompatibility(genome1, genome2);
    }
}

function assembly1Select(selectEle, item) {
    selectEle.innerHTML = item.label;
    assembly1Value = item.value || item.label;
    genome1 = item.genome;
//    console.log("asm1:", JSON.stringify(item, null, 2));
    document.getElementById("liftExists").style.display = "none";
    checkBothAssembliesSelected();
}

function assembly2Select(selectEle, item) {
    selectEle.innerHTML = item.label;
    assembly2Value = item.value || item.label;
    genome2 = item.genome;
//    console.log("asm2:", JSON.stringify(item, null, 2));
    document.getElementById("liftExists").style.display = "none";
    checkBothAssembliesSelected();
}

function validateEmail(checkEmail) {
    // Require at least one dot in domain
    var validEmail = /^[a-zA-Z0-9.!#$%&'*+/=?^_`{|}~-]+@[a-zA-Z0-9-]+(?:\.[a-zA-Z0-9-]+)+$/;

    if (!validEmail.test(checkEmail)) {
        alert("You have entered an invalid email address !");
        return false;
    }
    return true;
}

function submitForm() {
    var email = document.getElementById("emailInput").value;
    var comments = document.getElementById("commentsInput").value;

    // Hide any previous error message
    document.getElementById("errorMessage").style.display = "none";

    if (!assembly1Value) {
        alert("Please select Assembly 1");
        return;
    }
    if (!assembly2Value) {
        alert("Please select Assembly 2");
        return;
    }
    if (!email) {
        alert("Please enter an email address");
        return;
    }
    if (! validateEmail(email)) {
        return;
    }

    // Build the API URL with query parameters
    var comment = comments.slice(0, 1000); // make sure that URL is not too long
    comment += ", from: " + assembly1Value + ", to: " + assembly2Value;
    var apiUrl = "/cgi-bin/hubApi/liftRequest?" +
        "fromGenome=" + encodeURIComponent(genome1) + ";" +
        "toGenome=" + encodeURIComponent(genome2) + ";" +
        "email=" + encodeURIComponent(email) + ";" +
        "comment=" + encodeURIComponent(comment);

    $.ajax({
        url: apiUrl,
        type: "GET",
        success: function(response) {
            console.log(JSON.stringify(response));
            localStorage.setItem('liftRequestEmail', email);
            document.getElementById("formContainer").style.display = "none";
            document.getElementById("successMessage").style.display = "block";
        },
        error: function(xhr, status, error) {
            var errorMsg = "Error submitting request";
            if (xhr.responseJSON && xhr.responseJSON.error) {
                errorMsg = xhr.responseJSON.error;
            } else if (xhr.responseText) {
                try {
                    var parsed = JSON.parse(xhr.responseText);
                    if (parsed.error) {
                        errorMsg = parsed.error;
                    }
                } catch (e) {
                    errorMsg = error || status || "Unknown error occurred";
                }
            } else if (error) {
                errorMsg = error;
            }
            document.getElementById("errorText").textContent = errorMsg;
            document.getElementById("errorMessage").style.display = "block";
        }
    });
}

document.addEventListener("DOMContentLoaded", () => {
    // Assembly 1 autocomplete
    let selectEle1 = document.getElementById("genomeLabel1");
    let boundSelect1 = assembly1Select.bind(null, selectEle1);
    initSpeciesAutoCompleteDropdown('genomeSearch1', boundSelect1, "/cgi-bin/hubApi/findGenome?browser=mustExist;q=");

    let btn1 = document.getElementById("genomeSearchButton1");
    btn1.addEventListener("click", () => {
        let val = document.getElementById("genomeSearch1").value;
        $("[id='genomeSearch1']").autocompleteCat("search", val);
    });

    // Assembly 2 autocomplete
    let selectEle2 = document.getElementById("genomeLabel2");
    let boundSelect2 = assembly2Select.bind(null, selectEle2);
    initSpeciesAutoCompleteDropdown('genomeSearch2', boundSelect2, "/cgi-bin/hubApi/findGenome?browser=mustExist;q=");

    let btn2 = document.getElementById("genomeSearchButton2");
    btn2.addEventListener("click", () => {
        let val = document.getElementById("genomeSearch2").value;
        $("[id='genomeSearch2']").autocompleteCat("search", val);
    });
    // restore saved email if it exists
    var savedEmail = localStorage.getItem('liftRequestEmail');
    if (savedEmail) {
        document.getElementById("emailInput").value = savedEmail;
    }
});
