var assembly1Value = "";
var assembly2Value = "";

function assembly1Select(selectEle, item) {
    selectEle.innerHTML = item.label;
    assembly1Value = item.value || item.label;
}

function assembly2Select(selectEle, item) {
    selectEle.innerHTML = item.label;
    assembly2Value = item.value || item.label;
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
    comment = comments.slice(0, 1000); // make sure that URL is not too long
    var apiUrl = "/cgi-bin/hubApi/liftRequest?" +
        "fromGenome=" + encodeURIComponent(assembly1Value) + ";" +
        "toGenome=" + encodeURIComponent(assembly2Value) + ";" +
        "email=" + encodeURIComponent(email) + ";" +
        "comment=" + encodeURIComponent(comments);

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
    initSpeciesAutoCompleteDropdown('genomeSearch1', boundSelect1, "https://hgwdev-demo-chmalee.gi.ucsc.edu/cgi-bin/hubApi/findGenome?browser=mustExist&q=");

    let btn1 = document.getElementById("genomeSearchButton1");
    btn1.addEventListener("click", () => {
        let val = document.getElementById("genomeSearch1").value;
        $("[id='genomeSearch1']").autocompleteCat("search", val);
    });

    // Assembly 2 autocomplete
    let selectEle2 = document.getElementById("genomeLabel2");
    let boundSelect2 = assembly2Select.bind(null, selectEle2);
    initSpeciesAutoCompleteDropdown('genomeSearch2', boundSelect2, "https://hgwdev-demo-chmalee.gi.ucsc.edu/cgi-bin/hubApi/findGenome?browser=mustExist&q=");

    let btn2 = document.getElementById("genomeSearchButton2");
    btn2.addEventListener("click", () => {
        let val = document.getElementById("genomeSearch2").value;
        $("[id='genomeSearch2']").autocompleteCat("search", val);
    });
});
