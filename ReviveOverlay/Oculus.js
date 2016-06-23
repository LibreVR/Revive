
function generateManifest(manifest) {
    var launch = '/' + manifest["launchFile"];

    // Find an executable ending with -Shipping.exe
    var shipping = "-Shipping.exe";
    for (var file in manifest["files"]) {
        if (file.indexOf(shipping, file.length - shipping.length) != -1) {
            launch = '/' + file;
        }
    }

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
                "arguments" : "/base \"Software/Software/" + manifest["canonicalName"] + launch + "\"",

                "image_path" : basePath + "Software/Software/StoreAssets/" + manifest["canonicalName"] + "_assets/cover_landscape_image.jpg",

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

function loadAppManifest(appKey, coverURL) {
    var manifestURL = baseURL + 'Software/Manifests/' + appKey + '.json';
    var xhr = new XMLHttpRequest;
    xhr.onreadystatechange = function() {
        if (xhr.readyState == XMLHttpRequest.DONE) {
            // Check if the application is still installed.
            if (xhr.status == 200)
            {
                var manifest = JSON.parse(xhr.responseText);

                // Add the application manifest to the Revive manifest.
                if (manifest["packageType"] == "APP" && !manifest["isCore"]) {
                    console.log("Found application " + manifest["canonicalName"]);
                    coverModel.append({coverURL: coverURL, appKey: manifest["canonicalName"]});
                    if (!ReviveManifest.isApplicationInstalled(manifest["canonicalName"]))
                        generateManifest(manifest);
                }
            }
            else
            {
                // If the manifest no longer exists, then the application has been removed.
                if (ReviveManifest.isApplicationInstalled(appKey))
                    ReviveManifest.removeManifest(appKey);
            }
        }
    }
    xhr.open('GET', manifestURL);
    xhr.send();
    console.log("Loading application manifest: " + manifestURL);
}

function loadAssetsManifest(manifestURL) {
    var xhr = new XMLHttpRequest;
    xhr.onreadystatechange = function() {
        if (xhr.readyState == XMLHttpRequest.DONE) {
            var manifest = JSON.parse(xhr.responseText);

            // Assume only games have asset bundles and include their cover.
            if (manifest["packageType"] == "ASSET_BUNDLE") {
                console.log("Found assets bundle " + manifest["canonicalName"]);
                var cover = baseURL + "Software/Software/StoreAssets/" + manifest["canonicalName"] + "/cover_square_image.jpg";
                var key = manifest["canonicalName"];
                key = key.substring(0, key.indexOf("_assets"));
                loadAppManifest(key, cover);
            }
        }
    }
    xhr.open('GET', manifestURL);
    xhr.send();
    console.log("Loading assets manifest: " + manifestURL);
}
