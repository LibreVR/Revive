function decodeHtml(str) {
  return str.replace(/&#(\d+);/g, function(match, dec) {
    return String.fromCharCode(dec);
  });
};

function generateManifest(manifest) {
    console.log("Generating manifest for " + manifest["canonicalName"]);

    var launch = manifest["launchFile"].replace(/\//g, '\\');

    // Find the true executable for Unreal Engine games
    var shipping = /Binaries(\\|\/)Win64(\\|\/)(.*)\.exe/i;
    for (var file in manifest["files"]) {
        if (file.indexOf("CrashReportClient.exe") == -1 && shipping.exec(file) != null) {
            launch = file.replace(/\//g, '\\');
        }
    }

    var parameters = "";
    if (manifest["launchParameters"] != "" && manifest["launchParameters"] != "None")
        parameters = " " + manifest["launchParameters"];

    // Some games need special arguments, seems like a great idea to hardcode them here
    if (manifest["canonicalName"] == "epic-games-showdown")
        parameters = " ..\\..\\..\\ShowdownVRDemo\\ShowdownVRDemo.uproject";
    if (manifest["canonicalName"] == "hammerhead-vr-abe-vr")
        parameters = " ..\\..\\..\\Abe\\Abe.uproject";

    var xhr = new XMLHttpRequest;
    xhr.onreadystatechange = function() {
        if (xhr.readyState == XMLHttpRequest.DONE) {
            var regEx = /<title id=\"pageTitle\">(.*?) \| Oculus<\/title>/i;
            var title = manifest["canonicalName"];
            var result = regEx.exec(xhr.responseText);
            if (result != null)
                title = decodeHtml(result[1]).replace(/’/g, '\'');

            var revive = {
                "launch_type" : "binary",
                "binary_path_windows" : "Revive/ReviveInjector_x64.exe",
                "arguments" : "/library \"Software\\" + manifest["canonicalName"] + "\\" + launch + "\"" + parameters,

                "image_path" : Revive.LibraryPath + "Software/StoreAssets/" + manifest["canonicalName"] + "_assets/cover_landscape_image.jpg",

                "strings" : {
                    "en_us" : {
                        "name" : title
                    }
                }
            }

            Revive.addManifest(manifest["canonicalName"], JSON.stringify(revive));
        }
    }
    xhr.open('GET', "https://www.oculus.com/experiences/rift/" + manifest["appId"]);
    xhr.send();
}

function loadAppManifest(appKey, coverURL) {
    var manifestURL = Revive.LibraryURL + 'Manifests/' + appKey + '.json';
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
                    if (!Revive.isApplicationInstalled(manifest["canonicalName"]))
                        generateManifest(manifest);
                }
            }
            else
            {
                // If the manifest no longer exists, then the application has been removed.
                if (Revive.isApplicationInstalled(appKey))
                    Revive.removeManifest(appKey);
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
                var cover = Revive.LibraryURL + "Software/StoreAssets/" + manifest["canonicalName"] + "/cover_square_image.jpg";
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
