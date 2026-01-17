var assembly1Value = "";
var assembly2Value = "";
var genome1 = "";
var genome2 = "";

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
//    alert(liftPath);
    liftPath += "/liftOver/" + asm1 + "To";
    liftPath += asm2.charAt(0).toUpperCase() + asm2.slice(1);
    liftPath += ".over.chain.gz";
    return(liftPath);
}

function checkAssemblyCompatibility(asm1, asm2) {
    // Your API call here
    console.log("Checking compatibility for:", asm1, asm2);

    $.ajax({
        url: "/cgi-bin/hubApi/liftOver/listExisting?fromGenome=" + encodeURIComponent(asm1) +
             ";" + "toGenome=" + encodeURIComponent(asm2),
        success: function(response) {
            console.log(JSON.stringify(response, null, 2));
            if (response.itemsReturned === 1) {
               var liftPath = liftOverPath(asm1, asm2);
//               console.log("liftPath: ", liftPath);
               document.getElementById("liftExists").style.display = "block";
               var liftMessage = 'An alignment already exists between these assemblies.<br>' +
              '<a href="' + liftPath + '" download>Click here to download the chain file</a>';
               document.getElementById("liftPath").innerHTML = liftMessage;
            }
            console.log("itemsReturned: ", response.itemsReturned);
            // Handle response
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
    console.log("asm1 genome:", item.genome);
    document.getElementById("liftExists").style.display = "none";
    checkBothAssembliesSelected();
}

function assembly2Select(selectEle, item) {
    selectEle.innerHTML = item.label;
    assembly2Value = item.value || item.label;
    genome2 = item.genome;
//    console.log("asm2:", JSON.stringify(item, null, 2));
    console.log("asm2 genome:", item.genome);
    document.getElementById("liftExists").style.display = "none";
    checkBothAssembliesSelected();
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

    // Build the API URL with query parameters
    var comment = comments.slice(0, 1000); // make sure that URL is not too long
    var apiUrl = "/cgi-bin/hubApi/liftRequest?" +
        "fromGenome=" + encodeURIComponent(assembly1Value) + ";" +
        "toGenome=" + encodeURIComponent(assembly2Value) + ";" +
        "email=" + encodeURIComponent(email) + ";" +
        "comment=" + encodeURIComponent(comment);

    $.ajax({
        url: apiUrl,
        type: "GET",
        success: function(response) {
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
});
