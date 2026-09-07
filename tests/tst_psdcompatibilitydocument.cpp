#include <QtTest>

#include "psdcompatibilitydocument.h"

class tst_PsdCompatibilityDocument : public QObject
{
    Q_OBJECT

private slots:
    void createsRasterBaseLayerForEmptyCanvas();
    void mapsSessionObjectsToPsdLayerRecords();
    void omitsBackgroundLayerWhenDisabled();
    void clampsLayerBoundsAndOpacityToPsdSafeValues();
};

void tst_PsdCompatibilityDocument::createsRasterBaseLayerForEmptyCanvas()
{
    const PsdCompatibilityDocument document = PsdCompatibilityDocument::fromCongregationSession(QSize(640, 480), {});

    QCOMPARE(document.canvasSize(), QSize(640, 480));
    QVERIFY(document.isPsdCanvasSizeCompatible());
    QCOMPARE(document.layers().size(), 1);

    const PsdLayerRecord rasterLayer = document.layers().constFirst();
    QCOMPARE(rasterLayer.kind(), PsdLayerRecord::Kind::Raster);
    QCOMPARE(rasterLayer.kindName(), QStringLiteral("raster"));
    QCOMPARE(rasterLayer.name(), QStringLiteral("Background"));
    QCOMPARE(rasterLayer.bounds(), QRect(0, 0, 640, 480));
    QCOMPARE(rasterLayer.blendModeKey(), QStringLiteral("norm"));
    QCOMPARE(rasterLayer.opacity(), 255);
    QVERIFY(rasterLayer.isVisible());

    const QVariantMap manifest = document.toManifest();
    QCOMPARE(manifest.value(QStringLiteral("formatFamily")).toString(), QStringLiteral("psd"));
    QCOMPARE(manifest.value(QStringLiteral("compatibilityVersion")).toInt(), 1);
    QCOMPARE(manifest.value(QStringLiteral("canvasWidth")).toInt(), 640);
    QCOMPARE(manifest.value(QStringLiteral("canvasHeight")).toInt(), 480);
    QCOMPARE(manifest.value(QStringLiteral("colorMode")).toInt(), PsdCompatibilityDocument::rgbColorMode());
    QCOMPARE(manifest.value(QStringLiteral("bitsPerChannel")).toInt(), PsdCompatibilityDocument::bitsPerChannel());
    QCOMPARE(manifest.value(QStringLiteral("coordinateSystem")).toString(),
             QStringLiteral("psd-top-left-bottom-right-exclusive"));
}

void tst_PsdCompatibilityDocument::mapsSessionObjectsToPsdLayerRecords()
{
    QVariantMap imageLayer;
    imageLayer.insert(QStringLiteral("id"), 7);
    imageLayer.insert(QStringLiteral("type"), QStringLiteral("image"));
    imageLayer.insert(QStringLiteral("x"), 10);
    imageLayer.insert(QStringLiteral("y"), 20);
    imageLayer.insert(QStringLiteral("width"), 120);
    imageLayer.insert(QStringLiteral("height"), 90);
    imageLayer.insert(QStringLiteral("source"), QStringLiteral("file:///tmp/reference.png"));
    imageLayer.insert(QStringLiteral("originalWidth"), 240);
    imageLayer.insert(QStringLiteral("originalHeight"), 180);

    QVariantMap textLayer;
    textLayer.insert(QStringLiteral("id"), 8);
    textLayer.insert(QStringLiteral("type"), QStringLiteral("text"));
    textLayer.insert(QStringLiteral("x"), 50);
    textLayer.insert(QStringLiteral("y"), 60);
    textLayer.insert(QStringLiteral("width"), 180);
    textLayer.insert(QStringLiteral("height"), 44);
    textLayer.insert(QStringLiteral("text"), QStringLiteral("Title\nSecond line"));
    textLayer.insert(QStringLiteral("fontPixelSize"), 22);
    textLayer.insert(QStringLiteral("color"), QStringLiteral("#123456"));

    QVariantMap shapeLayer;
    shapeLayer.insert(QStringLiteral("id"), 9);
    shapeLayer.insert(QStringLiteral("type"), QStringLiteral("shape"));
    shapeLayer.insert(QStringLiteral("x"), 70);
    shapeLayer.insert(QStringLiteral("y"), 80);
    shapeLayer.insert(QStringLiteral("width"), 64);
    shapeLayer.insert(QStringLiteral("height"), 64);
    shapeLayer.insert(QStringLiteral("shapeKind"), QStringLiteral("ellipsebubble"));
    shapeLayer.insert(QStringLiteral("color"), QStringLiteral("#abcdef"));

    const QVariantList objects{imageLayer, textLayer, shapeLayer};
    const PsdCompatibilityDocument document = PsdCompatibilityDocument::fromCongregationSession(QSize(400, 300), objects);

    QCOMPARE(document.layers().size(), 4);
    QCOMPARE(document.layers().at(0).kind(), PsdLayerRecord::Kind::Raster);
    QCOMPARE(document.layers().at(1).kind(), PsdLayerRecord::Kind::Image);
    QCOMPARE(document.layers().at(1).name(), QStringLiteral("reference.png"));
    QCOMPARE(document.layers().at(1).bounds(), QRect(10, 20, 120, 90));
    QCOMPARE(document.layers().at(2).kind(), PsdLayerRecord::Kind::Text);
    QCOMPARE(document.layers().at(2).name(), QStringLiteral("Title"));
    QCOMPARE(document.layers().at(3).kind(), PsdLayerRecord::Kind::Shape);
    QCOMPARE(document.layers().at(3).name(), QStringLiteral("Ellipsebubble"));

    const QVariantList manifestLayers = document.toManifest().value(QStringLiteral("layers")).toList();
    QCOMPARE(manifestLayers.size(), 4);

    const QVariantMap textManifest = manifestLayers.at(2).toMap();
    QCOMPARE(textManifest.value(QStringLiteral("top")).toInt(), 60);
    QCOMPARE(textManifest.value(QStringLiteral("left")).toInt(), 50);
    QCOMPARE(textManifest.value(QStringLiteral("bottom")).toInt(), 104);
    QCOMPARE(textManifest.value(QStringLiteral("right")).toInt(), 230);
    QCOMPARE(textManifest.value(QStringLiteral("blendModeKey")).toString(), QStringLiteral("norm"));
    QCOMPARE(textManifest.value(QStringLiteral("opacity")).toInt(), 255);

    const QVariantMap textPayload = textManifest.value(QStringLiteral("payload")).toMap();
    QCOMPARE(textPayload.value(QStringLiteral("sourceObjectId")).toInt(), 8);
    QCOMPARE(textPayload.value(QStringLiteral("text")).toString(), QStringLiteral("Title\nSecond line"));
    QCOMPARE(textPayload.value(QStringLiteral("fontPixelSize")).toReal(), 22.0);
    QCOMPARE(textPayload.value(QStringLiteral("color")).toString(), QStringLiteral("#123456"));
}

void tst_PsdCompatibilityDocument::omitsBackgroundLayerWhenDisabled()
{
    QVariantMap layer;
    layer.insert(QStringLiteral("id"), 4);
    layer.insert(QStringLiteral("type"), QStringLiteral("layer"));
    layer.insert(QStringLiteral("name"), QStringLiteral("Ink"));
    layer.insert(QStringLiteral("x"), 0);
    layer.insert(QStringLiteral("y"), 0);
    layer.insert(QStringLiteral("width"), 64);
    layer.insert(QStringLiteral("height"), 48);

    const PsdCompatibilityDocument document =
            PsdCompatibilityDocument::fromCongregationSession(QSize(64, 48), {layer}, false);

    QCOMPARE(document.canvasSize(), QSize(64, 48));
    QCOMPARE(document.layers().size(), 1);
    QCOMPARE(document.layers().constFirst().name(), QStringLiteral("Ink"));

    const QVariantList manifestLayers = document.toManifest().value(QStringLiteral("layers")).toList();
    QCOMPARE(manifestLayers.size(), 1);
    QCOMPARE(manifestLayers.constFirst().toMap().value(QStringLiteral("name")).toString(), QStringLiteral("Ink"));
}

void tst_PsdCompatibilityDocument::clampsLayerBoundsAndOpacityToPsdSafeValues()
{
    QVariantMap layer;
    layer.insert(QStringLiteral("id"), 4);
    layer.insert(QStringLiteral("type"), QStringLiteral("shape"));
    layer.insert(QStringLiteral("x"), -12.4);
    layer.insert(QStringLiteral("y"), 8.2);
    layer.insert(QStringLiteral("width"), 500);
    layer.insert(QStringLiteral("height"), 12.2);
    layer.insert(QStringLiteral("opacity"), 0.5);
    layer.insert(QStringLiteral("visible"), false);
    layer.insert(QStringLiteral("blendMode"), QStringLiteral("multiply"));
    layer.insert(QStringLiteral("shapeKind"), QStringLiteral("rectangle"));

    const PsdCompatibilityDocument document = PsdCompatibilityDocument::fromCongregationSession(QSize(128, 64), {layer});
    const PsdLayerRecord psdLayer = document.layers().at(1);

    QCOMPARE(psdLayer.bounds(), QRect(0, 8, 128, 13));
    QCOMPARE(psdLayer.opacity(), 128);
    QVERIFY(!psdLayer.isVisible());
    QCOMPARE(psdLayer.blendModeKey(), QStringLiteral("mul "));

    const PsdCompatibilityDocument psbSizedDocument =
            PsdCompatibilityDocument::fromCongregationSession(QSize(PsdCompatibilityDocument::maximumPsdCanvasEdge() + 1, 64), {});
    QVERIFY(!psbSizedDocument.isPsdCanvasSizeCompatible());
}

QTEST_APPLESS_MAIN(tst_PsdCompatibilityDocument)

#include "tst_psdcompatibilitydocument.moc"
