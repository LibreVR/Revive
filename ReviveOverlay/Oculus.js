
function generateManifest(manifest) {
    var english = {}, strings = {};
    // TODO: Query for the actual title and description.
    english["name"] = manifest["canonicalName"];
    strings["en_us"] = english;

    var launch = '/' + manifest["launchFile"];
    // TODO: Find an executable ending with -Shipping.exe in manifest["files"].

    var revive = {};
    revive["launch_type"] = "binary";
    revive["binary_path_windows"] = "Revive/ReviveInjector_x64.exe";
    revive["arguments"] = "Software/" + manifest["canonicalName"] + launch;
    revive["launch_type"] = "binary";
    revive["strings"] = strings;

    ReviveManifest.addManifest(manifest["canonicalName"], JSON.stringify(revive));
}

function loadManifest(manifestURL) {
    var assetsFolder = baseURL + "Software/StoreAssets/";

    var xhr = new XMLHttpRequest;
    xhr.onreadystatechange = function() {
        if (xhr.readyState == XMLHttpRequest.DONE) {
            var manifest = JSON.parse(xhr.responseText);

            // Assume only games have asset bundles and include their cover.
            if (manifest["packageType"] == "ASSET_BUNDLE") {
                var cover = assetsFolder + manifest["canonicalName"] + "/cover_square_image.jpg";
                coverModel.append({coverURL: cover, appKey: manifest["canonicalName"]});
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
