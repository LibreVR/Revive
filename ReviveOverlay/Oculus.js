function decodeHtml(str) {
  return str.replace(/&#(\d+);/g, function(match, dec) {
    return String.fromCharCode(dec);
  });
};

function createObjects() {
    //create folderListModels for each folder
    for (var index in Revive.Libraries) {
        var finishCreation = function() {
            if (component.status == Component.Ready) {
                var library = component.createObject(mainWindow, {
                    library: Revive.Libraries[index],
                    libraryURL: Revive.LibrariesURL[index]
                });
                if (library == null) {
                    console.log("Error creating object");
                }
            } else if (component.status == Component.Error) {
                console.log("Error loading component:", component.errorString());
            }
        }

        var component = Qt.createComponent("Library.qml");
        if (component.status == Component.Ready)
            finishCreation();
        else if (component.status == Component.Loading)
            component.statusChanged.connect(finishCreation);
        else if (component.status == Component.Error)
            console.log("Error loading component:", component.errorString());
    }
}

function generateManifest(manifest, library) {
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

    var parameters = "";
    if (manifest["launchParameters"] != "" && manifest["launchParameters"] != "None" && manifest["launchParameters"] != null)
        parameters = " " + manifest["launchParameters"];

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
            var regEx = /<title id=\"pageTitle\">(.*?) Rift \| Oculus<\/title>/i;
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
                "launch_type": "binary",
                "binary_path_windows": "ReviveInjector.exe",
                "arguments": "/app " + manifest["canonicalName"] + " /library " + library + " \"Software\\" + manifest["canonicalName"] + "\\" + launch + "\"" + parameters,
                "action_manifest_path" : "Input/action_manifest.json",
                "image_path": Revive.BasePath + "CoreData/Software/StoreAssets/" + manifest["canonicalName"] + "_assets/cover_landscape_image_large.png",

                "strings": {
                    "en_us": {
                        "name": title
                    }
                }
            }

            Revive.addManifest(manifest["canonicalName"], JSON.stringify(revive));
        }
    }
    xhr.open('GET', "https://www.oculus.com/experiences/rift/" + manifest["appId"]);
    xhr.send();
}

function heartbeat(appId) {
    if (Platform.AccessToken.length > 0) {
        var xhr = new XMLHttpRequest;
        xhr.onreadystatechange = function () {
            if (xhr.readyState === XMLHttpRequest.DONE) {
                console.log(xhr.responseText);        
            }
        }
        xhr.open('POST', "https://graph.oculus.com/user_heartbeat?access_token=" + Platform.AccessToken);
        xhr.send({'current_status' : 'ONLINE', 'app_id_override' : appId, 'in_vr' : 'true'});
    }
}

function verifyAppManifest(appKey) {
    // See if the application manifest exists in any library.
    for (var index in Revive.LibrariesURL) {
        // Load the smaller mini file since we only want to verify whether it exists.
        var manifestURL = Revive.LibrariesURL[index] + 'Manifests/' + appKey + '.json.mini';
        var xhr = new XMLHttpRequest;
        xhr.open('GET', manifestURL, false);
        xhr.send();
        if (xhr.status == 200) {
            // Found a manifest, this app is still installed.
            return;
        }
    }

    // No manifest found in any library, remove it from our own manifest.
    if (Revive.isApplicationInstalled(appKey))
        Revive.removeManifest(appKey);
}

function loadManifest(manifestURL, library) {
    var xhr = new XMLHttpRequest;
    xhr.onreadystatechange = function () {
        if (xhr.readyState == XMLHttpRequest.DONE) {
            var manifest = JSON.parse(xhr.responseText);

            // Add the application manifest to the Revive manifest and include their cover.
            if (manifest["packageType"] == "APP" && !manifest["isCore"] && !manifest["thirdParty"]) {
                console.log("Found application " + manifest["canonicalName"]);
                var cover = Revive.BaseURL + "CoreData/Software/StoreAssets/" + manifest["canonicalName"] + "_assets/cover_square_image.jpg";
                coverModel.append({coverURL: cover, libraryId: library, appKey: manifest["canonicalName"], appId: manifest["appId"]});
                if (!Revive.isApplicationInstalled(manifest["canonicalName"]))
                    generateManifest(manifest, library);
            }
        }
    }
    xhr.open('GET', manifestURL);
    xhr.send();
    console.log("Loading manifest: " + manifestURL);
}

function removeLibrary(library) {
    // Iterate backwards so we can remove elements while iterating
    var anyRemoved = false;
    for (var i = coverModel.count - 1; i >= 0; i--) {
        if (coverModel.get(i).libraryId == library) {
            coverModel.remove(i);
            anyRemoved = true;
        }
    }
    return anyRemoved;
}
