/* jshint esnext: true */
var assembly1Value = "";
var assembly2Value = "";
var genome1 = "";
var genome2 = "";

// matches the ottoRequest README.txt and ottoRequestView.cgi STATUS_NAMES
var STATUS_LABELS = {
    0: "received by API",
    1: "acknowledged, email sent",
    2: "galaxy job started",
    3: "galaxy done, download started",
    4: "downloaded, track files made",
    5: "symlinks ready, awaiting push",
    6: "push complete",
    7: "ERROR",
    8: "COMPLETE (final email sent)"
};

function pendingMessageFor(status, requestTime) {
    var label = STATUS_LABELS[status] || ("status " + status);
    if (status === 7) {
        return "A previous request for these assemblies (submitted on " +
            requestTime + ") encountered an error. " +
            "Please contact genome-www@soe.ucsc.edu for help.";
    }
    if (status === 8) {
        return "A request for these assemblies (submitted on " +
            requestTime + ") has already completed. " +
            "If the chain files are not yet available, please contact" +
            " genome-www@soe.ucsc.edu.";
    }
    return "A request for these assemblies was submitted on " +
        requestTime + " and is currently in progress (" + label + ")." +
        " You will receive an email when it completes.";
}

/* check if a file exists at the specified URL */
function fileExists(url, callback) {
  fetch(url, { method: 'HEAD' })
    .then(response => {
      callback(response.ok);
    })
    .catch(() => {
      callback(false);
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
    const url = "/cgi-bin/hubApi/liftOver/listExisting?fromGenome=" + encodeURIComponent(asm1) +
                ";" + "toGenome=" + encodeURIComponent(asm2);

    fetch(url)
      .then(response => response.json())
      .then(response => {
//      console.log(JSON.stringify(response, null, 2));
        if (response.itemsReturned >= 1) {
          const liftPath1 = liftOverPath(asm1, asm2);
          const liftPath2 = liftOverPath(asm2, asm1);
          const browser1 = "/cgi-bin/hgTracks?db=" + asm1;
          const browser2 = "/cgi-bin/hgTracks?db=" + asm2;

          fileExists(liftPath1, function(exists) {
            if (exists) {
              document.getElementById("genome1Link").href = browser1;
              document.getElementById("genome1Link").textContent = assembly1Value;

              document.getElementById("genome1LiftOver").href = liftPath1;
              document.getElementById("genome1LiftOver").textContent = asm1 + " to " + asm2;

              document.getElementById("liftExists").style.display = "block";
              document.getElementById("emailForm").style.display = "none";
              document.getElementById("commentsForm").style.display = "none";
              document.getElementById("submitButton").style.display = "none";
            }
          });

          fileExists(liftPath2, function(exists) {
            if (exists) {
              document.getElementById("genome2Link").href = browser2;
              document.getElementById("genome2Link").textContent = assembly2Value;

              document.getElementById("genome2LiftOver").href = liftPath2;
              document.getElementById("genome2LiftOver").textContent = asm2 + " to " + asm1;

              document.getElementById("liftExists").style.display = "block";
              document.getElementById("emailForm").style.display = "none";
              document.getElementById("commentsForm").style.display = "none";
              document.getElementById("submitButton").style.display = "none";
            }
          });
        } else if (response.pending) {
          showPending(response.pendingStatus, response.pendingRequestTime);
        }
      })
      .catch(error => {
        console.error("Error fetching liftOver list:", error);
      });
}	// end of function checkAssemblyCompatibility(asm1, asm2)

function checkBothAssembliesSelected() {
    if (genome1 && genome2) { // Both assemblies are now selected
        checkAssemblyCompatibility(genome1, genome2);
    }
}

function resetFormVisibility() {
    document.getElementById("liftExists").style.display = "none";
    document.getElementById("pendingRequest").style.display = "none";
    document.getElementById("errorMessage").style.display = "none";
    document.getElementById("emailForm").style.display = "block";
    document.getElementById("commentsForm").style.display = "block";
    document.getElementById("submitButton").style.display = "block";
}

function showError(heading, errorMsg) {
    document.getElementById("errorHeading").textContent = heading;
    document.getElementById("errorText").textContent = errorMsg;
    document.getElementById("errorMessage").style.display = "block";
    document.getElementById("emailForm").style.display = "none";
    document.getElementById("commentsForm").style.display = "none";
    document.getElementById("submitButton").style.display = "none";
}

function showPending(status, requestTime) {
    document.getElementById("pendingMessage").textContent =
        pendingMessageFor(status, requestTime);
    document.getElementById("pendingRequest").style.display = "block";
    document.getElementById("emailForm").style.display = "none";
    document.getElementById("commentsForm").style.display = "none";
    document.getElementById("submitButton").style.display = "none";
}

function assembly1Select(selectEle, item) {
    selectEle.innerHTML = item.label;
    assembly1Value = item.value || item.label;
    genome1 = item.genome;
//  console.log("asm1:", JSON.stringify(item, null, 2));
    resetFormVisibility();
    checkBothAssembliesSelected();
}

function assembly2Select(selectEle, item) {
    selectEle.innerHTML = item.label;
    assembly2Value = item.value || item.label;
    genome2 = item.genome;
//  console.log("asm2:", JSON.stringify(item, null, 2));
    resetFormVisibility();
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
    document.getElementById("errorHeading").textContent = "Error";
    document.getElementById("errorText").textContent = "";

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

    fetch(apiUrl, { method: 'GET' })
      .then((response) => {
        return response.text().then((text) => {
          if (response.ok) {
            localStorage.setItem('liftRequestEmail', email);
            document.getElementById("formContainer").style.display = "none";
            document.getElementById("successMessage").style.display = "block";
          } else {
            // Try to extract error message from JSON or text
            var errorMsg = "Error submitting request";
            var heading = "Error";
            try {
              var parsed = JSON.parse(text);
              if (parsed.error) {
                errorMsg = parsed.error;
              }
            } catch(e) {
              errorMsg = text || response.statusText || errorMsg;
            }
            if (response.status === 429) {
              heading = "Daily limit reached";
              errorMsg = "Thank you for your interest. " + errorMsg +
                  " If you have an urgent need, please contact us at" +
                  " genome-www@soe.ucsc.edu.";
            } else if (response.status === 409) {
              heading = "Already submitted";
              errorMsg = errorMsg +
                  " If you have not seen the completion email, please" +
                  " contact us at genome-www@soe.ucsc.edu.";
            }
            showError(heading, errorMsg);
          }
        });
      })
      .catch((error) => {
        // Network or other fetch errors
        var errorMsg = error.message || "Unknown error occurred";
        showError("Error", errorMsg);
      });
}	// end of function submitForm()

function dismissLiftExists() {
    resetFormVisibility();
    document.getElementById("genomeSearch1").value = "";
    document.getElementById("genomeSearch2").value = "";
    assembly1Value = "";
    assembly2Value = "";
    genome1 = "";
    genome2 = "";
}

function onSearchError(jqXHR, textStatus, errorThrown, term) {
    return [{label: 'No genomes found', value: '', genome: '', disabled: true}];
}

document.addEventListener("DOMContentLoaded", () => {
    // Assembly 1 autocomplete
    let selectEle1 = document.getElementById("genomeLabel1");
    let boundSelect1 = assembly1Select.bind(null, selectEle1);
    initSpeciesAutoCompleteDropdown('genomeSearch1', boundSelect1,
        "/cgi-bin/hubApi/findGenome?browser=mustExist;q=", null, null, onSearchError);

    let btn1 = document.getElementById("genomeSearchButton1");
    btn1.addEventListener("click", () => {
        let val = document.getElementById("genomeSearch1").value;
        $("[id='genomeSearch1']").autocompleteCat("search", val);
    });

    // Assembly 2 autocomplete
    let selectEle2 = document.getElementById("genomeLabel2");
    let boundSelect2 = assembly2Select.bind(null, selectEle2);
    initSpeciesAutoCompleteDropdown('genomeSearch2', boundSelect2,
        "/cgi-bin/hubApi/findGenome?browser=mustExist;q=", null, null, onSearchError);

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

    document.getElementById("dismissLiftExists").addEventListener("click", dismissLiftExists);
    document.getElementById("dismissPending").addEventListener("click", resetFormVisibility);
    document.getElementById("dismissError").addEventListener("click", resetFormVisibility);
});
