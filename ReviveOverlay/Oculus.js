
function generateManifest(manifest) {
    var launch = '/' + manifest["launchFile"];
    // TODO: Find an executable ending with -Shipping.exe in manifest["files"].

    var xhr = new XMLHttpRequest;
    xhr.onreadystatechange = function() {
        if (xhr.readyState == XMLHttpRequest.DONE) {
            var regEx = /<title id=\"pageTitle\">(.*?) \| Oculus<\/title>/i;
            var title = regEx.exec(xhr.responseText)[1];
            if (title == null)
                title = manifest["canonicalName"];

            var revive = {
                "launch_type" : "binary",
                "binary_path_windows" : "Revive/ReviveInjector_x64.exe",
                "arguments" : "Software/" + manifest["canonicalName"] + launch,

                "image_path" : basePath + "Software/StoreAssets/" + manifest["canonicalName"] + "_assets/cover_landscape_image.jpg",

                "strings" : {
                    "en_us" : {
                        "name" : title
                    }
                }
            }

            ReviveManifest.addManifest(manifest["canonicalName"], JSON.stringify(revive));
        }
    }
    xhr.open('GET', "https://www2.oculus.com/experiences/app/" + manifest["appId"]);
    xhr.send();
}

function loadManifest(manifestURL) {
    var xhr = new XMLHttpRequest;
    xhr.onreadystatechange = function() {
        if (xhr.readyState == XMLHttpRequest.DONE) {
            var manifest = JSON.parse(xhr.responseText);

            // Assume only games have asset bundles and include their cover.
            if (manifest["packageType"] == "ASSET_BUNDLE") {
                var cover = baseURL + "Software/StoreAssets/" + manifest["canonicalName"] + "/cover_square_image.jpg";
                var key = manifest["canonicalName"];
                key = key.substring(0, key.indexOf("_assets"));
                coverModel.append({coverURL: cover, appKey: key});
            }

            // Add the application manifest to the Revive manifest.
            if (manifest["packageType"] == "APP" && !manifest["isCore"]) {
                if (!ReviveManifest.isApplicationInstalled(manifest["canonicalName"]))
                    generateManifest(manifest);
            }
        }
    }
    xhr.open('GET', manifestURL);
    xhr.send();
}
