
var applications = [];

var loadManifest = function(manifestURL) {
    var assetsFolder = baseURL + "Software/StoreAssets/";

    var xhr = new XMLHttpRequest;
    xhr.onreadystatechange = function() {
        if (xhr.readyState == XMLHttpRequest.DONE) {
            var manifest = JSON.parse(xhr.responseText);

            // Assume only games have asset bundles and include their cover.
            if (manifest["packageType"] == "ASSET_BUNDLE") {
                var cover = assetsFolder + manifest["canonicalName"] + "/cover_square_image.jpg";
                coverModel.append({coverURL: cover, appId: manifest["appId"]});
            }

            // Cache the application manifests with the appId as its key.
            if (manifest["packageType"] == "APP") {
                applications[manifest["appId"]] = manifest;
            }
        }
    }
    xhr.open('GET', manifestURL);
    xhr.send();
}

var launchApp = function(appId) {
    var manifest = applications[appId];
    console.log(manifest["launchFile"]);

    ReviveInjector.CreateProcessAndInject(manifest["launchFile"]);
}
