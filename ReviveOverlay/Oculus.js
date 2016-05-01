
var loadManifest = function(manifestURL) {
    var assetsFolder = baseURL + "Software/StoreAssets/";

    var xhr = new XMLHttpRequest;
    xhr.onreadystatechange = function() {
        if (xhr.readyState == XMLHttpRequest.DONE) {
            var manifest = JSON.parse(xhr.responseText);

            // Assume only games have asset bundles and include their cover.
            if (manifest["packageType"] == "ASSET_BUNDLE") {
                var cover = assetsFolder + manifest["canonicalName"] + "/cover_square_image.jpg";
                coverModel.append({coverURL: cover, canonicalName: manifest["canonicalName"]});
            }

            // Add the application manifest to the Revive manifest.
            if (manifest["packageType"] == "APP") {
                // TODO: Check if this application is already installed, if not add a new manifest
            }
        }
    }
    xhr.open('GET', manifestURL);
    xhr.send();
}
