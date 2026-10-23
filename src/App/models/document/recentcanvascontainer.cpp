#include "recentcanvascontainer.h"
#include "../../host/structuredvalues.h"
#include <Engine/RecentCanvas.h>
QByteArray encodeRecentCanvasContainer(const RecentCanvasContainer& canvas,QString* error) {
    congregation::RecentCanvas native;
    native.document=canvas.sharedCanvasDocument.toStdString();
    native.objects=congregation::array(congregation::presentation::fromVariant(canvas.drawableObjects));
    native.background=canvas.backgroundLayerPresent;
    for(const auto& a:canvas.embeddedAssets) native.assets.push_back({a.objectId,static_cast<std::uint8_t>(a.kind),a.pngData.toStdString()});
    auto encoded=congregation::encode_canvas(native);
    if(error) *error=encoded?QString{}:QString::fromStdString(encoded.error());
    return encoded?QByteArray::fromStdString(*encoded):QByteArray{};
}
RecentCanvasDecodeResult decodeRecentCanvasContainer(const QByteArray& bytes) {
    RecentCanvasDecodeResult result;
    auto native=congregation::decode_canvas(std::string_view(bytes.constData(),bytes.size()));
    if(!native) { result.error=QString::fromStdString(native.error()); return result; }
    result.container.sharedCanvasDocument=QByteArray::fromStdString(native->document);
    result.container.drawableObjects=congregation::presentation::toVariant(congregation::Value(native->objects)).toList();
    result.container.backgroundLayerPresent=native->background;
    for(const auto& a:native->assets) result.container.embeddedAssets.append({a.objectId,static_cast<RecentCanvasAssetKind>(a.kind),QByteArray::fromStdString(a.png)});
    return result;
}
