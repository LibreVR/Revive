function decodeHtml(str) {
  return str.replace(/&#(\d+);/g, function(match, dec) {
    return String.fromCharCode(dec);
  });
};

function generateManifest(manifest, basePath) {
    console.log("Generating manifest for " + manifest["canonicalName"]);
    var launch = manifest["launchFile"];

    // Find the true executable for Unreal Engine games
    var shipping = /-Shipping.exe$/i;
    var binaries = /Binaries(\\|\/)Win64(\\|\/)(.*)\.exe/i;
    for (var file in manifest["files"]) {
        // Check if the executable is in the binaries folder
        if (binaries.test(file)) {
            launch = file;

            // If we found the shipping executable we can immediately stop looking
            if (shipping.test(file))
                break;
        }
    }

    // Replace the forward slashes with backslashes as used by Windows
    // TODO: Move this to the injector
    launch = launch.replace(/\//g, '\\');

    // Some games need APC injection
    var apc = "";
    if (manifest["canonicalName"] == "oculus-quill")
        apc = "/apc ";

    var parameters = "";
    if (manifest["launchParameters"] != "" && manifest["launchParameters"] != "None" && manifest["launchParameters"] != null)
        parameters = " " + manifest["launchParameters"];

    //add condition ?
    var argBasePath = " /baseLib \"" + basePath.replace(/^.*file\:\/\/\/|\/$/g, '').replace(/\//g, '\\') + "\"";

    // Some games need special arguments, seems like a great idea to hardcode them here
    // TODO: Detect these arguments automatically from the file tree
    if (manifest["canonicalName"] == "epic-games-showdown")
        parameters = " ..\\..\\..\\ShowdownDemo\\ShowdownDemo.uproject";
    if (manifest["canonicalName"] == "hammerhead-vr-abe-vr")
        parameters = " ..\\..\\..\\Abe\\Abe.uproject";
    if (manifest["canonicalName"] == "epic-games-bullet-train-gdc")
        parameters = " ..\\..\\..\\showup\\showup.uproject";

    // Request the human-readable title by parsing the Oculus Store website
    var xhr = new XMLHttpRequest;
    xhr.onreadystatechange = function() {
        if (xhr.readyState == XMLHttpRequest.DONE) {
            var regEx = /<title id=\"pageTitle\">(.*?) on Oculus Rift \| Oculus<\/title>/i;
            var title = manifest["canonicalName"];

            // If the request was successful we can parse the response
            if (xhr.status == 200)
            {
                var result = regEx.exec(xhr.responseText);
                if (result != null)
                    title = decodeHtml(result[1]).replace(/’/g, '\'');
            }

            // Generate the entry and add it to the manifest
            var revive = {
                "launch_type" : "binary",
                "binary_path_windows" : "Revive/x64/ReviveInjector.exe",
                "arguments" : apc + "/app " + manifest["canonicalName"] + argBasePath + " /library \"Software\\" + manifest["canonicalName"] + "\\" + launch + "\"" + parameters,
                "action_manifest_path" : "Input/action_manifest.json",
                "image_path" : Revive.BasePath + "CoreData/Software/StoreAssets/" + manifest["canonicalName"] + "_assets/cover_landscape_image_large.png",

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

function verifyAppManifest(appKey) {
    // Load the smaller mini file since we only want to verify whether it exists.
	 var appToRemove = Revive.LibrariesURL.length;
    for (var index in Revive.LibrariesURL) {
        console.log("parsing lib " + Revive.LibrariesURL[index]);
        var manifestURL = Revive.LibrariesURL[index] + 'Manifests/' + appKey + '.json.mini';
        var xhr = new XMLHttpRequest;
        //remove async access, needed to synchronize appToRemove list
        xhr.open('GET', manifestURL, false);
        xhr.send(null);
        console.log("xhr status " + appKey + " " + xhr.status);
        if (xhr.status != 200) {
            if (!Revive.isApplicationInstalled(appKey)) {
                appToRemove--;
            }
        } else {
            appToRemove--;
        }
    }
    if (!appToRemove > 0) {
        Revive.removeManifest(appKey);
    }
}

function loadManifest(manifestURL, basePath) {
    var xhr = new XMLHttpRequest;
    xhr.onreadystatechange = function () {
        if (xhr.readyState == XMLHttpRequest.DONE) {
            var manifest = JSON.parse(xhr.responseText);

            // Add the application manifest to the Revive manifest and include their cover.
            if (manifest["packageType"] == "APP" && !manifest["isCore"] && !manifest["thirdParty"]) {
                console.log("Found application " + manifest["canonicalName"]);
                var cover = Revive.BaseURL + "CoreData/Software/StoreAssets/" + manifest["canonicalName"] + "_assets/cover_square_image.jpg";
                coverModel.append({coverURL: cover, appKey: manifest["canonicalName"]});
                if (!Revive.isApplicationInstalled(manifest["canonicalName"]))
                    generateManifest(manifest, basePath);
            }
        }
    }
    xhr.open('GET', manifestURL);
    xhr.send();
    console.log("Loading manifest: " + manifestURL);
}
