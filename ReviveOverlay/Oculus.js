
function loadManifest(manifestURL) {
    var assetsFolder = baseURL + "Software/StoreAssets/";

    var xhr = new XMLHttpRequest;
    xhr.onreadystatechange = function() {
        if (xhr.readyState == XMLHttpRequest.DONE) {
            var manifest = JSON.parse(xhr.responseText);
            if (manifest["packageType"] == "ASSET_BUNDLE") {
                var cover = assetsFolder + manifest["canonicalName"] + "/cover_square_image.jpg";
                coverModel.append({coverURL: cover});
            }
        }
    }
    xhr.open('GET', manifestURL);
    xhr.send();
}
