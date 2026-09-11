#include <QBuffer>
#include <QClipboard>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QHostAddress>
#include <QImage>
#include <QLineF>
#include <QLockFile>
#include <QMap>
#include <QMimeData>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlExpression>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QStringList>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>
#include <QtTest>

#include <iiSharedCanvas.h>
#include <iiFileProvider.h>

#include <cstdint>
#include <vector>

#include "canvasdocumentviewmodel.h"
#include "models/painting/drawingsurfaceitem.h"
#include "paletteutils.h"
#include "psdimagereader.h"

class tst_DrawingSurfaceItem : public QObject
{
    Q_OBJECT

  private slots:
    void initTestCase();
    void createsInitialCanvasInsideWorkspaceMargins();
    void createsNewCanvasAtCurrentWorkspaceSize();
    void createsInfiniteCanvasAndExpandsItWhilePanning();
    void constrainsShapeDragWithShiftModifier();
    void pansCanvasWithHandToolDrag();
    void zoomsCanvasWithHorizontalDrag();
    void zoomsCanvasWithMouseWheelInEveryToolMode();
    void zoomsCanvasWithNativeTemporaryCameraDrag();
    void usesToolAppropriateCanvasCursors();
    void tracksBrushCursorDuringNativePointerInput();
    void pastesSystemClipboardImageAsTransformableObject();
    void importsDraggedImageMimeFilesAndWebImages();
    void movesAndResizesDrawableObjects();
    void constrainsDrawableObjectTransformWithShiftModifier();
    void deletesSelectedDrawableObject();
    void deletesBackgroundLayerLikeRegularLayer();
    void addsBlankLayerRowsWithoutTransformHitTesting();
    void shapeAndTextToolsCreateSeparateLayerRows();
    void savesRasterLayerItemsAsIndependentCanvasLayers();
    void deletingRasterLayerRemovesItsPaintFromQmlComposite();
    void addsManyRasterLayersWithoutSnapshotChurn();
    void renamesLayerRowsAndDrawableObjectMetadata();
    void layersExposeHierarchyRowsAndReorderDrawableObjects();
    void drawsAndSavesStroke();
    void remoteInputPreviewsStrokeWithoutCommittingClientDocument();
    void qmlRemoteParticipantRoutesMutationToHostCanvas();
    void rendersSharedRasterVectorTimelineDocument();
    void rasterToolsPreserveMixedSharedCanvasLayers();
    void rasterToolsRespectSelectedLayerTransform();
    void openingRasterReplacesMixedSharedCanvasDocument();
    void roundTripsNativeSharedCanvasDocument();
    void roundTripsRecentCanvasContainerWithEditableObjects();
    void recentCanvasPreservesVisualCanvasExtentAfterLateResize();
    void roundTripsRecentCanvasThroughQmlSurface();
    void rejectsCorruptRecentCanvasWithoutReplacingTheCurrentDocument();
    void pressureSensitiveManualStrokesPreserveInputPressure();
    void erasesCommittedStrokePixels();
    void commitsTextToRasterCanvas();
    void commitsShapeToRasterCanvas();
    void commitsSpeechBubbleTailsAsIntegratedSolidShapes();
    void fillsContiguousRasterRegion();
    void cachesLayerBitmapThumbnails();
    void savesCompositeDrawableObjectsWithoutFlatteningRaster();
    void savesBlankCanvasAsOpaquePsdBackground();
    void savesPsdLayerRecordsInBottomToTopOrder();
    void savesCompositeDrawableObjectsAsLayeredPsdWithMetadata();
    void opensLayeredPsdThroughPsdSdkReader();
    void createsPsdDrawablePreviewForQmlImage();
    void supportsUndoRedo();
    void opensRasterBackground();
    void opensLargeRasterAtOriginalImageSize();
    void opensRasterImagesAsCanvasAtSourceResolution();
};

namespace
{

constexpr int nativeWindowExposureTimeoutMs = 15000;

class ClipboardContentsGuard
{
  public:
    explicit ClipboardContentsGuard(QClipboard* clipboard) : m_clipboard(clipboard)
    {
        if (!m_clipboard)
        {
            return;
        }

        const QMimeData* mimeData = m_clipboard->mimeData(QClipboard::Clipboard);
        if (!mimeData)
        {
            return;
        }

        for (const QString& format : mimeData->formats())
        {
            m_dataByFormat.insert(format, mimeData->data(format));
        }
        m_hasImage = mimeData->hasImage();
        m_image = m_clipboard->image(QClipboard::Clipboard);
        m_hasText = mimeData->hasText();
        m_text = mimeData->text();
        m_hasUrls = mimeData->hasUrls();
        m_urls = mimeData->urls();
    }

    ~ClipboardContentsGuard()
    {
        if (!m_clipboard)
        {
            return;
        }

        if (m_dataByFormat.isEmpty() && !m_hasImage && !m_hasText && !m_hasUrls)
        {
            m_clipboard->clear(QClipboard::Clipboard);
            return;
        }

        auto* mimeData = new QMimeData();
        for (auto it = m_dataByFormat.cbegin(); it != m_dataByFormat.cend(); ++it)
        {
            mimeData->setData(it.key(), it.value());
        }
        if (m_hasImage && !m_image.isNull())
        {
            mimeData->setImageData(m_image);
        }
        if (m_hasText)
        {
            mimeData->setText(m_text);
        }
        if (m_hasUrls)
        {
            mimeData->setUrls(m_urls);
        }
        m_clipboard->setMimeData(mimeData, QClipboard::Clipboard);
    }

  private:
    QClipboard* m_clipboard = nullptr;
    QMap<QString, QByteArray> m_dataByFormat;
    QImage m_image;
    QString m_text;
    QList<QUrl> m_urls;
    bool m_hasImage = false;
    bool m_hasText = false;
    bool m_hasUrls = false;
};

class CachePathCleanupGuard
{
  public:
    explicit CachePathCleanupGuard(QString path) : m_path(std::move(path)) {}

    ~CachePathCleanupGuard()
    {
        if (m_path.isEmpty())
        {
            return;
        }

        const QFileInfo pathInfo(m_path);
        if (pathInfo.isDir())
        {
            QDir().rmdir(m_path);
        }
        else if (pathInfo.exists())
        {
            QFile::remove(m_path);
        }
    }

    void dismiss() { m_path.clear(); }

  private:
    QString m_path;
};

class FakeDropEvent : public QObject
{
    Q_OBJECT
    Q_PROPERTY(qreal x READ x CONSTANT)
    Q_PROPERTY(qreal y READ y CONSTANT)
    Q_PROPERTY(QStringList formats READ formats CONSTANT)
    Q_PROPERTY(QList<QUrl> urls READ urls CONSTANT)
    Q_PROPERTY(QString html READ html CONSTANT)
    Q_PROPERTY(QString text READ text CONSTANT)

  public:
    qreal x() const { return m_x; }
    qreal y() const { return m_y; }
    QStringList formats() const { return m_dataByFormat.keys(); }
    QList<QUrl> urls() const { return m_urls; }
    QString html() const { return m_html; }
    QString text() const { return m_text; }

    Q_INVOKABLE QByteArray getDataAsArrayBuffer(const QString& format) const
    {
        return m_dataByFormat.value(format);
    }

    qreal m_x = 0;
    qreal m_y = 0;
    QMap<QString, QByteArray> m_dataByFormat;
    QList<QUrl> m_urls;
    QString m_html;
    QString m_text;
};

QString qmlErrorsToString(const QList<QQmlError>& errors)
{
    QStringList messages;
    messages.reserve(errors.size());
    for (const QQmlError& error : errors)
    {
        messages.append(error.toString());
    }
    return messages.join(QLatin1Char('\n'));
}

DrawingSurfaceItem* findDrawingSurfaceItem(QQuickItem* root)
{
    if (!root)
    {
        return nullptr;
    }

    if (auto* surfaceItem = qobject_cast<DrawingSurfaceItem*>(root))
    {
        return surfaceItem;
    }

    const QList<QQuickItem*> children = root->childItems();
    for (QQuickItem* child : children)
    {
        if (auto* surfaceItem = findDrawingSurfaceItem(child))
        {
            return surfaceItem;
        }
    }

    return nullptr;
}

QQuickItem* findItemByObjectName(QQuickItem* root, const QString& objectName)
{
    if (!root)
    {
        return nullptr;
    }

    if (root->objectName() == objectName)
    {
        return root;
    }

    const QList<QQuickItem*> children = root->childItems();
    for (QQuickItem* child : children)
    {
        if (QQuickItem* item = findItemByObjectName(child, objectName))
        {
            return item;
        }
    }

    return nullptr;
}

int workspaceHorizontalInset(qreal width)
{
    return qMax(24, qRound(width * 0.09));
}

int workspaceTopInset(qreal height)
{
    return qMax(24, qRound(height * 0.12));
}

int workspaceBottomInset(qreal height)
{
    return qMax(24, qRound(height * 0.10));
}

QSize workspaceCanvasSize(qreal width, qreal height)
{
    return QSize(
        qMax(1, qRound(width) - workspaceHorizontalInset(width) * 2),
        qMax(1, qRound(height) - workspaceTopInset(height) - workspaceBottomInset(height)));
}

qreal fittedCanvasZoomScale(qreal viewportWidth, qreal viewportHeight, qreal canvasWidth,
                            qreal canvasHeight)
{
    return qMin<qreal>(1.0, qMin(viewportWidth / canvasWidth, viewportHeight / canvasHeight));
}

bool isBlueShapePixel(const QColor& pixel)
{
    return pixel.alpha() > 0 && pixel.blue() > 120 && pixel.red() < 80 && pixel.green() > 70;
}

template <typename Predicate>
int countLogicalAnnulusPixels(const QImage& image, const QSizeF& logicalSize,
                              const QPointF& logicalCenter, qreal innerRadius, qreal outerRadius,
                              Predicate predicate)
{
    if (image.isNull() || logicalSize.isEmpty())
    {
        return 0;
    }

    int matchingPixels = 0;
    for (int pixelY = 0; pixelY < image.height(); ++pixelY)
    {
        const qreal logicalY = (pixelY + 0.5) * logicalSize.height() / image.height();
        for (int pixelX = 0; pixelX < image.width(); ++pixelX)
        {
            const qreal logicalX = (pixelX + 0.5) * logicalSize.width() / image.width();
            const qreal distance = QLineF(logicalCenter, QPointF(logicalX, logicalY)).length();
            if (distance >= innerRadius && distance <= outerRadius &&
                predicate(image.pixelColor(pixelX, pixelY)))
            {
                ++matchingPixels;
            }
        }
    }
    return matchingPixels;
}

quint16 readUInt16(const QByteArray& bytes, int offset)
{
    return static_cast<quint16>((static_cast<uchar>(bytes.at(offset)) << 8) |
                                static_cast<uchar>(bytes.at(offset + 1)));
}

quint32 readUInt32(const QByteArray& bytes, int offset)
{
    return (static_cast<quint32>(static_cast<uchar>(bytes.at(offset))) << 24) |
           (static_cast<quint32>(static_cast<uchar>(bytes.at(offset + 1))) << 16) |
           (static_cast<quint32>(static_cast<uchar>(bytes.at(offset + 2))) << 8) |
           static_cast<quint32>(static_cast<uchar>(bytes.at(offset + 3)));
}

qsizetype imageResourcesLengthOffset(const QByteArray& psd)
{
    return 30 + readUInt32(psd, 26);
}

qsizetype layerMaskLengthOffset(const QByteArray& psd)
{
    const qsizetype resourcesOffset = imageResourcesLengthOffset(psd);
    return resourcesOffset + 4 + readUInt32(psd, static_cast<int>(resourcesOffset));
}

int paddedPascalStringSize(int textLength)
{
    return ((textLength + 1 + 3) / 4) * 4;
}

QStringList psdLayerRecordNames(const QByteArray& psd)
{
    QStringList names;
    const qsizetype layerOffset = layerMaskLengthOffset(psd);
    if (layerOffset + 10 >= psd.size())
    {
        return names;
    }

    const qsizetype layerInfoEnd =
        layerOffset + 8 + readUInt32(psd, static_cast<int>(layerOffset + 4));
    qsizetype cursor = layerOffset + 8;
    if (cursor + 2 > psd.size())
    {
        return names;
    }

    const int layerCount = qAbs(static_cast<qint16>(readUInt16(psd, static_cast<int>(cursor))));
    cursor += 2;
    names.reserve(layerCount);

    for (int layerIndex = 0; layerIndex < layerCount; ++layerIndex)
    {
        if (cursor + 18 > psd.size() || cursor >= layerInfoEnd)
        {
            return {};
        }

        cursor += 16;
        const int channelCount = readUInt16(psd, static_cast<int>(cursor));
        cursor += 2 + channelCount * 6;
        if (cursor + 16 > psd.size())
        {
            return {};
        }

        cursor += 12;
        const quint32 extraDataLength = readUInt32(psd, static_cast<int>(cursor));
        cursor += 4;
        const qsizetype extraDataEnd = cursor + extraDataLength;
        if (extraDataEnd > psd.size() || cursor + 8 > psd.size())
        {
            return {};
        }

        const quint32 layerMaskLength = readUInt32(psd, static_cast<int>(cursor));
        cursor += 4 + layerMaskLength;
        if (cursor + 4 > psd.size())
        {
            return {};
        }

        const quint32 blendingRangesLength = readUInt32(psd, static_cast<int>(cursor));
        cursor += 4 + blendingRangesLength;
        if (cursor >= psd.size())
        {
            return {};
        }

        const int nameLength = static_cast<uchar>(psd.at(cursor));
        ++cursor;
        if (cursor + nameLength > psd.size())
        {
            return {};
        }

        names.append(QString::fromLatin1(psd.mid(cursor, nameLength)));
        cursor += paddedPascalStringSize(nameLength) - 1;
        cursor = extraDataEnd;
    }

    return names;
}

} // namespace

void tst_DrawingSurfaceItem::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    qmlRegisterType<DrawingSurfaceItem>("Congregation", 1, 0, "DrawingSurfaceItem");
}

void tst_DrawingSurfaceItem::createsInitialCanvasInsideWorkspaceMargins()
{
    QQmlEngine engine;

    QQmlComponent component(&engine);
    const QString drawingSurfaceQml = QFINDTESTDATA("../App/qml/painting/DrawingSurface.qml");
    QVERIFY2(!drawingSurfaceQml.isEmpty(), "DrawingSurface.qml test data was not found");
    component.loadUrl(QUrl::fromLocalFile(drawingSurfaceQml));
    QTRY_VERIFY(component.isReady() || component.isError());
    QVERIFY2(component.isReady(), qPrintable(qmlErrorsToString(component.errors())));

    PaletteUtils paletteUtils;
    CanvasDocumentViewModel viewModel(&paletteUtils);
    viewModel.setBrushFlow(0.42);
    viewModel.setBrushOpacity(0.64);
    viewModel.setBrushHardness(0.71);
    viewModel.setBrushSpacing(7.5);
    viewModel.setBrushSpacingRatio(0.33);
    viewModel.setPressureCurveMinimum(0.2);
    viewModel.setPressureCurveMaximum(0.8);
    viewModel.setPressureCurveCenter(0.6);
    viewModel.setBrushPressureControlsOpacity(false);
    viewModel.setStabilizerStrength(0.44);

    QVariantMap initialProperties;
    initialProperties.insert(QStringLiteral("width"), 720);
    initialProperties.insert(QStringLiteral("height"), 480);
    initialProperties.insert(QStringLiteral("documentViewModel"),
                             QVariant::fromValue(static_cast<QObject*>(&viewModel)));
    initialProperties.insert(QStringLiteral("brushFlow"), 0.42);
    initialProperties.insert(QStringLiteral("brushOpacity"), 0.64);
    initialProperties.insert(QStringLiteral("brushHardness"), 0.71);
    initialProperties.insert(QStringLiteral("brushSpacing"), 7.5);
    initialProperties.insert(QStringLiteral("brushSpacingRatio"), 0.33);
    initialProperties.insert(QStringLiteral("pressureCurveMinimum"), 0.2);
    initialProperties.insert(QStringLiteral("pressureCurveCenter"), 0.6);
    initialProperties.insert(QStringLiteral("pressureCurveMaximum"), 0.8);
    initialProperties.insert(QStringLiteral("brushPressureControlsOpacity"), false);
    initialProperties.insert(QStringLiteral("stabilizerStrength"), 0.44);

    QScopedPointer<QObject> object(component.createWithInitialProperties(initialProperties));
    QVERIFY2(!object.isNull(), qPrintable(qmlErrorsToString(component.errors())));
    auto* rootItem = qobject_cast<QQuickItem*>(object.data());
    QVERIFY(rootItem);
    const QColor workspaceColor = rootItem->property("color").value<QColor>();
    QVERIFY(workspaceColor != QColor(Qt::white));

    QQuickItem* canvasPaper = findItemByObjectName(rootItem, QStringLiteral("canvasPaper"));
    QVERIFY(canvasPaper);
    QCOMPARE(canvasPaper->property("color").value<QColor>(), QColor(Qt::white));
    QQuickItem* canvasViewport = findItemByObjectName(rootItem, QStringLiteral("canvasViewport"));
    QVERIFY(canvasViewport);

    DrawingSurfaceItem* canvasItem = findDrawingSurfaceItem(rootItem);
    QVERIFY(canvasItem);
    const QSize expectedCanvasSize = workspaceCanvasSize(720, 480);
    QTRY_COMPARE(canvasViewport->x(), static_cast<qreal>(workspaceHorizontalInset(720)));
    QTRY_COMPARE(canvasViewport->y(), static_cast<qreal>(workspaceTopInset(480)));
    QTRY_COMPARE(canvasViewport->width(), static_cast<qreal>(expectedCanvasSize.width()));
    QTRY_COMPARE(canvasViewport->height(), static_cast<qreal>(expectedCanvasSize.height()));
    QTRY_COMPARE(canvasItem->width(), static_cast<qreal>(expectedCanvasSize.width()));
    QTRY_COMPARE(canvasItem->height(), static_cast<qreal>(expectedCanvasSize.height()));
    QTRY_COMPARE(canvasPaper->width(), static_cast<qreal>(expectedCanvasSize.width()));
    QTRY_COMPARE(canvasPaper->height(), static_cast<qreal>(expectedCanvasSize.height()));
    QTRY_COMPARE(viewModel.canvasWidth(), expectedCanvasSize.width());
    QTRY_COMPARE(viewModel.canvasHeight(), expectedCanvasSize.height());
    QCOMPARE(canvasItem->brushFlow(), 0.42);
    QCOMPARE(canvasItem->brushOpacity(), 0.64);
    QCOMPARE(canvasItem->brushHardness(), 0.71);
    QCOMPARE(canvasItem->brushSpacing(), 7.5);
    QCOMPARE(canvasItem->brushSpacingRatio(), 0.33);
    QCOMPARE(canvasItem->pressureCurveMinimum(), 0.2);
    QCOMPARE(canvasItem->pressureCurveCenter(), 0.6);
    QCOMPARE(canvasItem->pressureCurveMaximum(), 0.8);
    QCOMPARE(canvasItem->brushOpacityEnabled(), false);
    QCOMPARE(canvasItem->stabilizerStrength(), 0.44);

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString outputPath = dir.filePath(QStringLiteral("initial-canvas.png"));
    QVERIFY(canvasItem->saveToFile(outputPath));
    const QImage saved(outputPath);
    QVERIFY(!saved.isNull());
    QCOMPARE(saved.size(), expectedCanvasSize);
}

void tst_DrawingSurfaceItem::createsNewCanvasAtCurrentWorkspaceSize()
{
    qmlRegisterType<DrawingSurfaceItem>("Congregation", 1, 0, "DrawingSurfaceItem");

    QQmlEngine engine;

    QQmlComponent component(&engine);
    const QString drawingSurfaceQml = QFINDTESTDATA("../App/qml/painting/DrawingSurface.qml");
    QVERIFY2(!drawingSurfaceQml.isEmpty(), "DrawingSurface.qml test data was not found");
    component.loadUrl(QUrl::fromLocalFile(drawingSurfaceQml));
    QTRY_VERIFY(component.isReady() || component.isError());
    QVERIFY2(component.isReady(), qPrintable(qmlErrorsToString(component.errors())));

    PaletteUtils paletteUtils;
    CanvasDocumentViewModel viewModel(&paletteUtils);

    QVariantMap initialProperties;
    initialProperties.insert(QStringLiteral("width"), 720);
    initialProperties.insert(QStringLiteral("height"), 480);
    initialProperties.insert(QStringLiteral("documentViewModel"),
                             QVariant::fromValue(static_cast<QObject*>(&viewModel)));

    QScopedPointer<QObject> object(component.createWithInitialProperties(initialProperties));
    QVERIFY2(!object.isNull(), qPrintable(qmlErrorsToString(component.errors())));
    auto* rootItem = qobject_cast<QQuickItem*>(object.data());
    QVERIFY(rootItem);

    DrawingSurfaceItem* canvasItem = findDrawingSurfaceItem(rootItem);
    QVERIFY(canvasItem);
    QCOMPARE(rootItem->property("maximumAntialiasingBrushHardness").toReal(),
             CanvasDocumentViewModel::maximumAntialiasingBrushHardness());
    QQuickItem* canvasPaper = findItemByObjectName(rootItem, QStringLiteral("canvasPaper"));
    QVERIFY(canvasPaper);
    QQuickItem* canvasViewport = findItemByObjectName(rootItem, QStringLiteral("canvasViewport"));
    QVERIFY(canvasViewport);
    const QSize initialCanvasSize = workspaceCanvasSize(720, 480);
    QTRY_COMPARE(canvasViewport->x(), static_cast<qreal>(workspaceHorizontalInset(720)));
    QTRY_COMPARE(canvasViewport->y(), static_cast<qreal>(workspaceTopInset(480)));
    QTRY_COMPARE(canvasItem->width(), static_cast<qreal>(initialCanvasSize.width()));
    QTRY_COMPARE(canvasItem->height(), static_cast<qreal>(initialCanvasSize.height()));
    QTRY_COMPARE(canvasPaper->width(), static_cast<qreal>(initialCanvasSize.width()));
    QTRY_COMPARE(canvasPaper->height(), static_cast<qreal>(initialCanvasSize.height()));
    QTRY_COMPARE(viewModel.canvasWidth(), initialCanvasSize.width());
    QTRY_COMPARE(viewModel.canvasHeight(), initialCanvasSize.height());
    QCOMPARE(canvasItem->brushHardness(),
             CanvasDocumentViewModel::maximumAntialiasingBrushHardness());
    QVERIFY(rootItem->property("color").value<QColor>() !=
            canvasPaper->property("color").value<QColor>());
    QCOMPARE(rootItem->property("selectedDrawableObjectId").toInt(), 1);
    QVariantList initialObjects = rootItem->property("drawableObjects").toList();
    QCOMPARE(initialObjects.size(), 1);
    QCOMPARE(initialObjects.first().toMap().value(QStringLiteral("type")).toString(),
             QStringLiteral("layer"));
    QCOMPARE(initialObjects.first().toMap().value(QStringLiteral("name")).toString(),
             QStringLiteral("Layer 1"));
    QVariantList initialRows;
    QTRY_VERIFY(
        [&]()
        {
            initialRows = rootItem->property("layerHierarchyRows").toList();
            return initialRows.size() == 2;
        }());
    QCOMPARE(initialRows.at(0).toMap().value(QStringLiteral("key")).toString(),
             QStringLiteral("object-1"));
    QCOMPARE(initialRows.at(0).toMap().value(QStringLiteral("selected")).toBool(), true);
    QCOMPARE(initialRows.at(1).toMap().value(QStringLiteral("key")).toString(),
             QStringLiteral("raster-canvas"));
    QCOMPARE(initialRows.at(1).toMap().value(QStringLiteral("selected")).toBool(), false);
    QVERIFY(rootItem->setProperty("canvasWidth", 720));
    QVERIFY(rootItem->setProperty("canvasHeight", 480));

    QVERIFY(rootItem->setProperty("canvasWidth", 300));
    QVERIFY(rootItem->setProperty("canvasHeight", 200));
    QCoreApplication::processEvents();
    QCOMPARE(canvasItem->width(), static_cast<qreal>(initialCanvasSize.width()));
    QCOMPARE(canvasItem->height(), static_cast<qreal>(initialCanvasSize.height()));
    QVERIFY(rootItem->setProperty("canvasWidth", 720));
    QVERIFY(rootItem->setProperty("canvasHeight", 480));

    rootItem->setWidth(960);
    rootItem->setHeight(540);
    QCoreApplication::processEvents();
    const QSize expandedCanvasSize = workspaceCanvasSize(960, 540);
    QCOMPARE(canvasViewport->x(), static_cast<qreal>(workspaceHorizontalInset(960)));
    QCOMPARE(canvasViewport->y(), static_cast<qreal>(workspaceTopInset(540)));
    QCOMPARE(canvasViewport->width(), static_cast<qreal>(expandedCanvasSize.width()));
    QCOMPARE(canvasViewport->height(), static_cast<qreal>(expandedCanvasSize.height()));
    QCOMPARE(canvasItem->width(), static_cast<qreal>(initialCanvasSize.width()));
    QCOMPARE(canvasItem->height(), static_cast<qreal>(initialCanvasSize.height()));
    QCOMPARE(canvasPaper->width(), static_cast<qreal>(initialCanvasSize.width()));
    QCOMPARE(canvasPaper->height(), static_cast<qreal>(initialCanvasSize.height()));

    QQmlExpression createWorkspaceCanvas(engine.rootContext(), object.data(),
                                         QStringLiteral("newCanvas();"));
    createWorkspaceCanvas.evaluate();
    QVERIFY2(!createWorkspaceCanvas.hasError(),
             qPrintable(createWorkspaceCanvas.error().toString()));
    QTRY_COMPARE(canvasItem->width(), static_cast<qreal>(expandedCanvasSize.width()));
    QTRY_COMPARE(canvasItem->height(), static_cast<qreal>(expandedCanvasSize.height()));
    QTRY_COMPARE(canvasPaper->width(), static_cast<qreal>(expandedCanvasSize.width()));
    QTRY_COMPARE(canvasPaper->height(), static_cast<qreal>(expandedCanvasSize.height()));
    QTRY_COMPARE(viewModel.canvasWidth(), expandedCanvasSize.width());
    QTRY_COMPARE(viewModel.canvasHeight(), expandedCanvasSize.height());
    QVariantList workspaceCanvasObjects = rootItem->property("drawableObjects").toList();
    QCOMPARE(workspaceCanvasObjects.size(), 1);
    const QVariantMap workspaceDefaultLayer = workspaceCanvasObjects.first().toMap();
    QCOMPARE(workspaceDefaultLayer.value(QStringLiteral("type")).toString(),
             QStringLiteral("layer"));
    QCOMPARE(workspaceDefaultLayer.value(QStringLiteral("name")).toString(),
             QStringLiteral("Layer 1"));
    QCOMPARE(rootItem->property("selectedDrawableObjectId").toInt(),
             workspaceDefaultLayer.value(QStringLiteral("id")).toInt());

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString outputPath = dir.filePath(QStringLiteral("new-workspace-sized-canvas.png"));
    QVERIFY(canvasItem->saveToFile(outputPath));
    const QImage saved(outputPath);
    QVERIFY(!saved.isNull());
    QCOMPARE(saved.size(), expandedCanvasSize);

    const QSize explicitCanvasSize(333, 222);
    QQmlExpression createExplicitCanvas(engine.rootContext(), object.data(),
                                        QStringLiteral("newCanvas(333, 222);"));
    createExplicitCanvas.evaluate();
    QVERIFY2(!createExplicitCanvas.hasError(), qPrintable(createExplicitCanvas.error().toString()));
    QTRY_COMPARE(canvasItem->width(), static_cast<qreal>(explicitCanvasSize.width()));
    QTRY_COMPARE(canvasItem->height(), static_cast<qreal>(explicitCanvasSize.height()));
    QTRY_COMPARE(canvasPaper->width(), static_cast<qreal>(explicitCanvasSize.width()));
    QTRY_COMPARE(canvasPaper->height(), static_cast<qreal>(explicitCanvasSize.height()));
    QTRY_COMPARE(viewModel.canvasWidth(), explicitCanvasSize.width());
    QTRY_COMPARE(viewModel.canvasHeight(), explicitCanvasSize.height());
    QCOMPARE(rootItem->property("canvasZoomScale").toReal(), 1.0);

    const QString explicitOutputPath =
        dir.filePath(QStringLiteral("new-explicit-sized-canvas.png"));
    QVERIFY(canvasItem->saveToFile(explicitOutputPath));
    const QImage explicitSaved(explicitOutputPath);
    QVERIFY(!explicitSaved.isNull());
    QCOMPARE(explicitSaved.size(), explicitCanvasSize);

    QVERIFY(rootItem->setProperty("canvasWidth", explicitCanvasSize.width()));
    QVERIFY(rootItem->setProperty("canvasHeight", explicitCanvasSize.height()));
    rootItem->setWidth(800);
    rootItem->setHeight(600);
    QCoreApplication::processEvents();
    const QSize compactCanvasSize = workspaceCanvasSize(800, 600);
    QCOMPARE(canvasViewport->x(), static_cast<qreal>(workspaceHorizontalInset(800)));
    QCOMPARE(canvasViewport->y(), static_cast<qreal>(workspaceTopInset(600)));
    QCOMPARE(canvasViewport->width(), static_cast<qreal>(compactCanvasSize.width()));
    QCOMPARE(canvasViewport->height(), static_cast<qreal>(compactCanvasSize.height()));
    QCOMPARE(canvasItem->width(), static_cast<qreal>(explicitCanvasSize.width()));
    QCOMPARE(canvasItem->height(), static_cast<qreal>(explicitCanvasSize.height()));
    QCOMPARE(canvasPaper->width(), static_cast<qreal>(explicitCanvasSize.width()));
    QCOMPARE(canvasPaper->height(), static_cast<qreal>(explicitCanvasSize.height()));

    const QSize highResolutionCanvasSize(4096, 3072);
    QQmlExpression createHighResolutionCanvas(engine.rootContext(), object.data(),
                                              QStringLiteral("newCanvas(4096, 3072);"));
    createHighResolutionCanvas.evaluate();
    QVERIFY2(!createHighResolutionCanvas.hasError(),
             qPrintable(createHighResolutionCanvas.error().toString()));
    QTRY_COMPARE(canvasItem->width(), static_cast<qreal>(highResolutionCanvasSize.width()));
    QTRY_COMPARE(canvasItem->height(), static_cast<qreal>(highResolutionCanvasSize.height()));
    QTRY_COMPARE(canvasPaper->width(), static_cast<qreal>(highResolutionCanvasSize.width()));
    QTRY_COMPARE(canvasPaper->height(), static_cast<qreal>(highResolutionCanvasSize.height()));
    QTRY_COMPARE(viewModel.canvasWidth(), highResolutionCanvasSize.width());
    QTRY_COMPARE(viewModel.canvasHeight(), highResolutionCanvasSize.height());

    const qreal expectedHighResolutionZoom =
        fittedCanvasZoomScale(canvasViewport->width(), canvasViewport->height(),
                              highResolutionCanvasSize.width(), highResolutionCanvasSize.height());
    QVERIFY(expectedHighResolutionZoom < 1.0);
    QVERIFY(qAbs(rootItem->property("canvasZoomScale").toReal() - expectedHighResolutionZoom) <
            0.0001);
    QCOMPARE(canvasItem->scale(), rootItem->property("canvasZoomScale").toReal());
    QCOMPARE(canvasPaper->scale(), rootItem->property("canvasZoomScale").toReal());
    QVERIFY(canvasItem->width() * canvasItem->scale() <= canvasViewport->width() + 0.5);
    QVERIFY(canvasItem->height() * canvasItem->scale() <= canvasViewport->height() + 0.5);

    QVERIFY(QMetaObject::invokeMethod(rootItem, "clearCanvas", Qt::DirectConnection));
    QTRY_COMPARE(canvasItem->width(), static_cast<qreal>(compactCanvasSize.width()));
    QTRY_COMPARE(canvasItem->height(), static_cast<qreal>(compactCanvasSize.height()));
    QTRY_COMPARE(canvasPaper->width(), static_cast<qreal>(compactCanvasSize.width()));
    QTRY_COMPARE(canvasPaper->height(), static_cast<qreal>(compactCanvasSize.height()));
    QTRY_COMPARE(viewModel.canvasWidth(), compactCanvasSize.width());
    QTRY_COMPARE(viewModel.canvasHeight(), compactCanvasSize.height());
    QCOMPARE(rootItem->property("canvasZoomScale").toReal(), 1.0);
}

void tst_DrawingSurfaceItem::createsInfiniteCanvasAndExpandsItWhilePanning()
{
    qmlRegisterType<DrawingSurfaceItem>("Congregation", 1, 0, "DrawingSurfaceItem");

    QQmlEngine engine;
    QQmlComponent component(&engine);
    const QString drawingSurfaceQml = QFINDTESTDATA("../App/qml/painting/DrawingSurface.qml");
    QVERIFY2(!drawingSurfaceQml.isEmpty(), "DrawingSurface.qml test data was not found");
    component.loadUrl(QUrl::fromLocalFile(drawingSurfaceQml));
    QTRY_VERIFY(component.isReady() || component.isError());
    QVERIFY2(component.isReady(), qPrintable(qmlErrorsToString(component.errors())));

    PaletteUtils paletteUtils;
    CanvasDocumentViewModel viewModel(&paletteUtils);
    QVariantMap initialProperties;
    initialProperties.insert(QStringLiteral("width"), 720);
    initialProperties.insert(QStringLiteral("height"), 480);
    initialProperties.insert(QStringLiteral("documentViewModel"),
                             QVariant::fromValue(static_cast<QObject*>(&viewModel)));
    QScopedPointer<QObject> object(component.createWithInitialProperties(initialProperties));
    QVERIFY2(!object.isNull(), qPrintable(qmlErrorsToString(component.errors())));
    auto* rootItem = qobject_cast<QQuickItem*>(object.data());
    QVERIFY(rootItem);

    QQuickWindow window;
    window.setGeometry(100, 100, 720, 480);
    rootItem->setParentItem(window.contentItem());
    window.show();
    QVERIFY2(QTest::qWaitForWindowExposed(&window, nativeWindowExposureTimeoutMs),
             "The infinite-canvas input test window was not exposed before the compositor timeout");

    QQmlExpression createInfinite(engine.rootContext(), object.data(),
                                  QStringLiteral("newCanvas(320, 240, true);"));
    createInfinite.evaluate();
    QVERIFY2(!createInfinite.hasError(), qPrintable(createInfinite.error().toString()));

    DrawingSurfaceItem* canvasItem = findDrawingSurfaceItem(rootItem);
    QVERIFY(canvasItem);
    QQuickItem* canvasPaper = findItemByObjectName(rootItem, QStringLiteral("canvasPaper"));
    QQuickItem* canvasViewport = findItemByObjectName(rootItem, QStringLiteral("canvasViewport"));
    QVERIFY(canvasPaper);
    QVERIFY(canvasViewport);
    QTRY_VERIFY(canvasItem->infiniteCanvas());
    QTRY_COMPARE(canvasItem->width(), 320.0);
    QTRY_COMPARE(canvasItem->height(), 240.0);
    QCOMPARE(canvasItem->canvasOriginX(), 0);
    QCOMPARE(canvasItem->canvasOriginY(), 0);
    QCOMPARE(canvasItem->canvasChunkSize(), 256);
    QVERIFY(canvasItem->document());
    QVERIFY(iiSharedCanvas::findChunkedRasterAsset(*canvasItem->document(), "canvas.raster.0"));
    const QVariantList createdObjects = rootItem->property("drawableObjects").toList();
    QCOMPARE(createdObjects.size(), 1);
    const int defaultLayerId = createdObjects.first().toMap().value(QStringLiteral("id")).toInt();
    QVERIFY(defaultLayerId > 0);
    QTRY_VERIFY(
        [&]()
        {
            QQmlExpression layerReady(engine.rootContext(), object.data(),
                                      QStringLiteral("const item = rasterLayerItemById(%1); "
                                                     "item !== null && item.infiniteCanvas;")
                                          .arg(defaultLayerId));
            const QVariant result = layerReady.evaluate();
            return !layerReady.hasError() && result.toBool();
        }());

    const QPointF worldZeroBefore = canvasItem->mapToItem(canvasViewport, QPointF(0.0, 0.0));
    QVERIFY(rootItem->setProperty("toolMode", QStringLiteral("pan")));
    QQmlExpression pan(
        engine.rootContext(), object.data(),
        QStringLiteral("beginPanDrag(100, 100); updatePanDrag(500, 100); commitPanDrag();"));
    pan.evaluate();
    QVERIFY2(!pan.hasError(), qPrintable(pan.error().toString()));

    QTRY_VERIFY(canvasItem->canvasOriginX() < 0);
    QTRY_VERIFY(canvasItem->canvasWidth() > 320);
    QTRY_COMPARE(canvasItem->width(), static_cast<qreal>(canvasItem->canvasWidth()));
    QTRY_COMPARE(canvasItem->height(), static_cast<qreal>(canvasItem->canvasHeight()));
    QTRY_COMPARE(canvasPaper->width(), canvasItem->width());
    QTRY_COMPARE(canvasPaper->height(), canvasItem->height());

    const QPointF worldZeroAfter = canvasItem->mapToItem(
        canvasViewport, QPointF(-canvasItem->canvasOriginX(), -canvasItem->canvasOriginY()));
    QVERIFY(qAbs(worldZeroAfter.x() - (worldZeroBefore.x() + 400.0)) <= 1.5);
    QVERIFY(qAbs(worldZeroAfter.y() - worldZeroBefore.y()) <= 1.5);

    QQmlExpression layerObject(engine.rootContext(), object.data(),
                               QStringLiteral("rasterLayerItemById(%1);").arg(defaultLayerId));
    const QVariant layerObjectResult = layerObject.evaluate();
    QVERIFY2(!layerObject.hasError(), qPrintable(layerObject.error().toString()));
    auto* layerItem = qobject_cast<DrawingSurfaceItem*>(layerObjectResult.value<QObject*>());
    QVERIFY(layerItem);
    QQmlExpression staleLayerGeometry(
        engine.rootContext(), object.data(),
        QStringLiteral("(function () { const layer = cloneDrawableObject(drawableObjects[0]); "
                       "layer.width = 320; layer.height = 240; "
                       "return layer.id === %1 && replaceDrawableObjectById(%1, layer); })();")
            .arg(defaultLayerId));
    const QVariant staleLayerGeometryResult = staleLayerGeometry.evaluate();
    QVERIFY2(!staleLayerGeometry.hasError(), qPrintable(staleLayerGeometry.error().toString()));
    QCOMPARE(staleLayerGeometryResult.toBool(), true);
    QCoreApplication::processEvents();
    QCOMPARE(layerItem->width(), canvasItem->width());
    QCOMPARE(layerItem->height(), canvasItem->height());
    QVERIFY(rootItem->setProperty("toolMode", QStringLiteral("brush")));
    QCoreApplication::processEvents();
    QTRY_VERIFY(layerItem->isEnabled());

    const QPointF expandedLayerPoint(layerItem->width() - 10.0, layerItem->height() - 10.0);
    QVERIFY(layerItem->contains(expandedLayerPoint));
    layerItem->beginStroke(expandedLayerPoint.x(), expandedLayerPoint.y(), 1.0, false);
    layerItem->endStroke(expandedLayerPoint.x(), expandedLayerPoint.y(), 1.0, false);
    const auto* layerPaintAsset =
        iiSharedCanvas::findChunkedRasterAsset(*layerItem->document(), "canvas.raster.0");
    QVERIFY(layerPaintAsset);
    const int layerChunkColumn = qFloor((layerItem->canvasOriginX() + expandedLayerPoint.x()) /
                                        layerItem->canvasChunkSize());
    const int layerChunkRow = qFloor((layerItem->canvasOriginY() + expandedLayerPoint.y()) /
                                     layerItem->canvasChunkSize());
    QVERIFY(iiSharedCanvas::findRasterChunk(*layerPaintAsset, layerChunkColumn, layerChunkRow));

    QQmlExpression exposeZoomedWorld(engine.rootContext(), object.data(),
                                     QStringLiteral("canvasZoomScale = 0.5; "
                                                    "ensureInfiniteCanvasForViewport();"));
    exposeZoomedWorld.evaluate();
    QVERIFY2(!exposeZoomedWorld.hasError(), qPrintable(exposeZoomedWorld.error().toString()));
    QTRY_COMPARE(layerItem->width(), canvasItem->width());
    QTRY_COMPARE(layerItem->height(), canvasItem->height());

    const QPointF visibleViewportPoint(canvasViewport->width() / 2.0,
                                       canvasViewport->height() - 12.0);
    const QPointF visibleLayerPoint = layerItem->mapFromItem(canvasViewport, visibleViewportPoint);
    QVERIFY(layerItem->contains(visibleLayerPoint));
    const qreal visibleWorldX = layerItem->canvasOriginX() + visibleLayerPoint.x();
    const qreal visibleWorldY = layerItem->canvasOriginY() + visibleLayerPoint.y();
    QVERIFY(visibleWorldX < 0.0 || visibleWorldX >= 320.0 || visibleWorldY < 0.0 ||
            visibleWorldY >= 240.0);
    const int priorLayerStrokeCount = layerItem->strokeCount();
    const QPoint visibleWindowPoint = canvasViewport->mapToScene(visibleViewportPoint).toPoint();
    QTest::mousePress(&window, Qt::LeftButton, Qt::NoModifier, visibleWindowPoint);
    QTest::mouseRelease(&window, Qt::LeftButton, Qt::NoModifier, visibleWindowPoint);
    QTRY_COMPARE(layerItem->strokeCount(), priorLayerStrokeCount + 1);
    layerPaintAsset =
        iiSharedCanvas::findChunkedRasterAsset(*layerItem->document(), "canvas.raster.0");
    QVERIFY(layerPaintAsset);
    QVERIFY(iiSharedCanvas::findRasterChunk(*layerPaintAsset,
                                            qFloor(visibleWorldX / layerItem->canvasChunkSize()),
                                            qFloor(visibleWorldY / layerItem->canvasChunkSize())));

    QQmlExpression activateBackground(engine.rootContext(), object.data(),
                                      QStringLiteral("activateLayerByKey(\"raster-canvas\");"));
    QCOMPARE(activateBackground.evaluate().toBool(), true);
    QVERIFY2(!activateBackground.hasError(), qPrintable(activateBackground.error().toString()));
    QVERIFY(rootItem->setProperty("toolMode", QStringLiteral("brush")));
    QCoreApplication::processEvents();
    canvasItem->beginStroke(10, 10, 1.0, false);
    canvasItem->endStroke(10, 10, 1.0, false);
    auto* paintAsset =
        iiSharedCanvas::findChunkedRasterAsset(*canvasItem->document(), "canvas.raster.0");
    QVERIFY(paintAsset);
    QVERIFY(iiSharedCanvas::findRasterChunk(
        *paintAsset, canvasItem->canvasOriginX() / canvasItem->canvasChunkSize(),
        canvasItem->canvasOriginY() / canvasItem->canvasChunkSize()));

    QTemporaryDir nativeCanvasDirectory;
    QVERIFY(nativeCanvasDirectory.isValid());
    const QString nativeCanvasPath =
        nativeCanvasDirectory.filePath(QStringLiteral("infinite.iisc"));
    QVERIFY(canvasItem->saveToFile(nativeCanvasPath));
    CanvasDocumentViewModel reopenedViewModel(&paletteUtils);
    DrawingSurfaceItem reopened;
    reopened.setWidth(1);
    reopened.setHeight(1);
    reopened.setDocumentViewModel(&reopenedViewModel);
    QVERIFY(reopened.openRaster(nativeCanvasPath));
    QVERIFY(reopened.infiniteCanvas());
    QCOMPARE(reopened.canvasOriginX(), canvasItem->canvasOriginX());
    QCOMPARE(reopened.canvasOriginY(), canvasItem->canvasOriginY());
    QCOMPARE(reopened.canvasWidth(), canvasItem->canvasWidth());
    QCOMPARE(reopened.canvasHeight(), canvasItem->canvasHeight());
    QVERIFY(iiSharedCanvas::findChunkedRasterAsset(*reopened.document(), "canvas.raster.0"));

    const QString recentCanvasPath = nativeCanvasDirectory.filePath(QStringLiteral("infinite.vrc"));
    QVERIFY(canvasItem->saveRecentCanvas(recentCanvasPath, {}, {}, true));
    CanvasDocumentViewModel recentViewModel(&paletteUtils);
    DrawingSurfaceItem recent;
    recent.setWidth(1);
    recent.setHeight(1);
    recent.setDocumentViewModel(&recentViewModel);
    const QVariantMap recentSession = recent.openRecentCanvas(recentCanvasPath);
    QVERIFY(recentSession.value(QStringLiteral("valid")).toBool());
    QVERIFY(recent.infiniteCanvas());
    QCOMPARE(recent.canvasOriginX(), canvasItem->canvasOriginX());
    QCOMPARE(recent.canvasOriginY(), canvasItem->canvasOriginY());
    QCOMPARE(recent.canvasWidth(), canvasItem->canvasWidth());
    QCOMPARE(recent.canvasHeight(), canvasItem->canvasHeight());

    QVariantList objects = rootItem->property("drawableObjects").toList();
    QCOMPARE(objects.size(), 1);
    const QVariantMap rasterLayer = objects.first().toMap();
    QCOMPARE(rasterLayer.value(QStringLiteral("x")).toReal(), 0.0);
    QCOMPARE(rasterLayer.value(QStringLiteral("y")).toReal(), 0.0);
    QCOMPARE(rasterLayer.value(QStringLiteral("width")).toReal(), canvasItem->width());
    QCOMPARE(rasterLayer.value(QStringLiteral("height")).toReal(), canvasItem->height());

    QQmlExpression layerState(
        engine.rootContext(), object.data(),
        QStringLiteral("const item = rasterLayerItemById(%5); "
                       "item !== null && item.infiniteCanvas "
                       "&& item.canvasOriginX === %1 && item.canvasOriginY === %2 "
                       "&& item.canvasWidth === %3 && item.canvasHeight === %4;")
            .arg(canvasItem->canvasOriginX())
            .arg(canvasItem->canvasOriginY())
            .arg(canvasItem->canvasWidth())
            .arg(canvasItem->canvasHeight())
            .arg(defaultLayerId));
    QTRY_VERIFY(!layerState.evaluate().isNull());
    QQmlExpression layerDebug(
        engine.rootContext(), object.data(),
        QStringLiteral("const item = rasterLayerItemById(%1); item === null ? \"null\" : "
                       "[item.infiniteCanvas, item.canvasOriginX, item.canvasOriginY, "
                       "item.canvasWidth, item.canvasHeight].join(\",\");")
            .arg(defaultLayerId));
    const QVariant layerStateResult = layerState.evaluate();
    QVERIFY2(!layerState.hasError() && layerStateResult.toBool(),
             qPrintable(layerDebug.evaluate().toString()));
}

void tst_DrawingSurfaceItem::constrainsShapeDragWithShiftModifier()
{
    qmlRegisterType<DrawingSurfaceItem>("Congregation", 1, 0, "DrawingSurfaceItem");

    QQmlEngine engine;

    QQmlComponent component(&engine);
    const QString drawingSurfaceQml = QFINDTESTDATA("../App/qml/painting/DrawingSurface.qml");
    QVERIFY2(!drawingSurfaceQml.isEmpty(), "DrawingSurface.qml test data was not found");
    component.loadUrl(QUrl::fromLocalFile(drawingSurfaceQml));
    QTRY_VERIFY(component.isReady() || component.isError());
    QVERIFY2(component.isReady(), qPrintable(qmlErrorsToString(component.errors())));

    PaletteUtils paletteUtils;
    CanvasDocumentViewModel viewModel(&paletteUtils);

    QVariantMap initialProperties;
    initialProperties.insert(QStringLiteral("width"), 500);
    initialProperties.insert(QStringLiteral("height"), 360);
    initialProperties.insert(QStringLiteral("documentViewModel"),
                             QVariant::fromValue(static_cast<QObject*>(&viewModel)));
    initialProperties.insert(QStringLiteral("toolMode"), QStringLiteral("shape"));

    QScopedPointer<QObject> object(component.createWithInitialProperties(initialProperties));
    QVERIFY2(!object.isNull(), qPrintable(qmlErrorsToString(component.errors())));
    auto* rootItem = qobject_cast<QQuickItem*>(object.data());
    QVERIFY(rootItem);

    DrawingSurfaceItem* canvasItem = findDrawingSurfaceItem(rootItem);
    QVERIFY(canvasItem);
    QTRY_VERIFY(canvasItem->width() > 160);
    QTRY_VERIFY(canvasItem->height() > 160);

    QQmlExpression constrainedDrag(
        engine.rootContext(), object.data(),
        QStringLiteral("beginShapeDrag(20, 20, false); updateShapeDrag(140, 70, true);"));
    constrainedDrag.evaluate();
    QVERIFY2(!constrainedDrag.hasError(), qPrintable(constrainedDrag.error().toString()));

    QCOMPARE(rootItem->property("shapeAspectLocked").toBool(), true);
    const qreal constrainedWidth = qAbs(rootItem->property("shapeCurrentX").toReal() -
                                        rootItem->property("shapeStartX").toReal());
    const qreal constrainedHeight = qAbs(rootItem->property("shapeCurrentY").toReal() -
                                         rootItem->property("shapeStartY").toReal());
    QCOMPARE(constrainedWidth, constrainedHeight);
    QCOMPARE(constrainedWidth, 120.0);

    QQmlExpression freeDrag(engine.rootContext(), object.data(),
                            QStringLiteral("updateShapeDrag(140, 70, false);"));
    freeDrag.evaluate();
    QVERIFY2(!freeDrag.hasError(), qPrintable(freeDrag.error().toString()));

    QCOMPARE(rootItem->property("shapeAspectLocked").toBool(), false);
    const qreal freeWidth = qAbs(rootItem->property("shapeCurrentX").toReal() -
                                 rootItem->property("shapeStartX").toReal());
    const qreal freeHeight = qAbs(rootItem->property("shapeCurrentY").toReal() -
                                  rootItem->property("shapeStartY").toReal());
    QCOMPARE(freeWidth, 120.0);
    QCOMPARE(freeHeight, 50.0);
}

void tst_DrawingSurfaceItem::pansCanvasWithHandToolDrag()
{
    qmlRegisterType<DrawingSurfaceItem>("Congregation", 1, 0, "DrawingSurfaceItem");

    QQmlEngine engine;

    QQmlComponent component(&engine);
    const QString drawingSurfaceQml = QFINDTESTDATA("../App/qml/painting/DrawingSurface.qml");
    QVERIFY2(!drawingSurfaceQml.isEmpty(), "DrawingSurface.qml test data was not found");
    component.loadUrl(QUrl::fromLocalFile(drawingSurfaceQml));
    QTRY_VERIFY(component.isReady() || component.isError());
    QVERIFY2(component.isReady(), qPrintable(qmlErrorsToString(component.errors())));

    PaletteUtils paletteUtils;
    CanvasDocumentViewModel viewModel(&paletteUtils);

    QVariantMap initialProperties;
    initialProperties.insert(QStringLiteral("width"), 720);
    initialProperties.insert(QStringLiteral("height"), 480);
    initialProperties.insert(QStringLiteral("documentViewModel"),
                             QVariant::fromValue(static_cast<QObject*>(&viewModel)));
    initialProperties.insert(QStringLiteral("toolMode"), QStringLiteral("pan"));

    QScopedPointer<QObject> object(component.createWithInitialProperties(initialProperties));
    QVERIFY2(!object.isNull(), qPrintable(qmlErrorsToString(component.errors())));
    auto* rootItem = qobject_cast<QQuickItem*>(object.data());
    QVERIFY(rootItem);

    DrawingSurfaceItem* canvasItem = findDrawingSurfaceItem(rootItem);
    QVERIFY(canvasItem);
    QQuickItem* canvasPaper = findItemByObjectName(rootItem, QStringLiteral("canvasPaper"));
    QVERIFY(canvasPaper);
    QTRY_COMPARE(rootItem->property("canvasPanOffsetX").toReal(), 0.0);
    QTRY_COMPARE(rootItem->property("canvasPanOffsetY").toReal(), 0.0);

    const qreal initialCanvasX = canvasItem->x();
    const qreal initialCanvasY = canvasItem->y();
    const qreal initialPaperX = canvasPaper->x();
    const qreal initialPaperY = canvasPaper->y();

    QQmlExpression panRightUp(
        engine.rootContext(), object.data(),
        QStringLiteral("beginPanDrag(100, 120); updatePanDrag(145, 85); commitPanDrag();"));
    panRightUp.evaluate();
    QVERIFY2(!panRightUp.hasError(), qPrintable(panRightUp.error().toString()));

    QTRY_COMPARE(rootItem->property("canvasPanOffsetX").toReal(), 45.0);
    QTRY_COMPARE(rootItem->property("canvasPanOffsetY").toReal(), -35.0);
    QTRY_COMPARE(canvasItem->x(), initialCanvasX + 45.0);
    QTRY_COMPARE(canvasItem->y(), initialCanvasY - 35.0);
    QTRY_COMPARE(canvasPaper->x(), initialPaperX + 45.0);
    QTRY_COMPARE(canvasPaper->y(), initialPaperY - 35.0);

    QQmlExpression resetPan(engine.rootContext(), object.data(),
                            QStringLiteral("resetCanvasPan();"));
    resetPan.evaluate();
    QVERIFY2(!resetPan.hasError(), qPrintable(resetPan.error().toString()));

    QTRY_COMPARE(rootItem->property("canvasPanOffsetX").toReal(), 0.0);
    QTRY_COMPARE(rootItem->property("canvasPanOffsetY").toReal(), 0.0);
    QTRY_COMPARE(canvasItem->x(), initialCanvasX);
    QTRY_COMPARE(canvasItem->y(), initialCanvasY);
    QTRY_COMPARE(canvasPaper->x(), initialPaperX);
    QTRY_COMPARE(canvasPaper->y(), initialPaperY);

    QQmlExpression setBrushTool(engine.rootContext(), object.data(),
                                QStringLiteral("toolMode = \"brush\";"));
    setBrushTool.evaluate();
    QVERIFY2(!setBrushTool.hasError(), qPrintable(setBrushTool.error().toString()));
    QCOMPARE(rootItem->property("toolMode").toString(), QStringLiteral("brush"));

    QQmlExpression temporaryPan(
        engine.rootContext(), object.data(),
        QStringLiteral(
            "beginSpacePanMode();"
            "beginPanDrag(40, 60);"
            "updatePanDrag(75, 84);"
            "commitPanDrag();"
            "[effectiveToolMode(), toolMode, canvasPanOffsetX, canvasPanOffsetY].join(\",\");"));
    const QVariant temporaryPanResult = temporaryPan.evaluate();
    QVERIFY2(!temporaryPan.hasError(), qPrintable(temporaryPan.error().toString()));
    QCOMPARE(temporaryPanResult.toString(), QStringLiteral("pan,brush,35,24"));
    QTRY_COMPARE(canvasItem->x(), initialCanvasX + 35.0);
    QTRY_COMPARE(canvasItem->y(), initialCanvasY + 24.0);
    QTRY_COMPARE(canvasPaper->x(), initialPaperX + 35.0);
    QTRY_COMPARE(canvasPaper->y(), initialPaperY + 24.0);

    QQmlExpression releaseTemporaryPan(
        engine.rootContext(), object.data(),
        QStringLiteral(
            "endSpacePanMode();"
            "[effectiveToolMode(), toolMode, canvasPanOffsetX, canvasPanOffsetY].join(\",\");"));
    const QVariant releaseTemporaryPanResult = releaseTemporaryPan.evaluate();
    QVERIFY2(!releaseTemporaryPan.hasError(), qPrintable(releaseTemporaryPan.error().toString()));
    QCOMPARE(releaseTemporaryPanResult.toString(), QStringLiteral("brush,brush,35,24"));

    QQmlExpression temporaryZoom(
        engine.rootContext(), object.data(),
        QStringLiteral("const zoomBefore = canvasZoomScale;"
                       "beginSpaceZoomMode();"
                       "beginZoomDrag(100);"
                       "updateZoomDrag(220);"
                       "commitZoomDrag();"
                       "[effectiveToolMode(), toolMode, canvasZoomScale > zoomBefore, "
                       "temporaryCameraMode].join(\",\");"));
    const QVariant temporaryZoomResult = temporaryZoom.evaluate();
    QVERIFY2(!temporaryZoom.hasError(), qPrintable(temporaryZoom.error().toString()));
    QCOMPARE(temporaryZoomResult.toString(), QStringLiteral("zoom,brush,true,zoom"));

    QQmlExpression modifierReleaseToPan(
        engine.rootContext(), object.data(),
        QStringLiteral("beginSpacePanMode();"
                       "[effectiveToolMode(), toolMode, temporaryCameraMode, "
                       "zoomDraggingActive].join(\",\");"));
    const QVariant modifierReleaseToPanResult = modifierReleaseToPan.evaluate();
    QVERIFY2(!modifierReleaseToPan.hasError(), qPrintable(modifierReleaseToPan.error().toString()));
    QCOMPARE(modifierReleaseToPanResult.toString(), QStringLiteral("pan,brush,pan,false"));

    QQmlExpression releaseTemporaryCamera(
        engine.rootContext(), object.data(),
        QStringLiteral("endTemporaryCameraMode();"
                       "[effectiveToolMode(), toolMode, temporaryCameraMode].join(\",\");"));
    const QVariant releaseTemporaryCameraResult = releaseTemporaryCamera.evaluate();
    QVERIFY2(!releaseTemporaryCamera.hasError(),
             qPrintable(releaseTemporaryCamera.error().toString()));
    QCOMPARE(releaseTemporaryCameraResult.toString(), QStringLiteral("brush,brush,"));

    QQmlExpression disabledShortcutPan(
        engine.rootContext(), object.data(),
        QStringLiteral("toolShortcutsEnabled = false;"
                       "[beginSpacePanMode(), beginSpaceZoomMode(), effectiveToolMode(), "
                       "temporaryCameraMode].join(\",\");"));
    const QVariant disabledShortcutPanResult = disabledShortcutPan.evaluate();
    QVERIFY2(!disabledShortcutPan.hasError(), qPrintable(disabledShortcutPan.error().toString()));
    QCOMPARE(disabledShortcutPanResult.toString(), QStringLiteral("false,false,brush,"));

    QQmlExpression textEditingSpace(
        engine.rootContext(), object.data(),
        QStringLiteral("toolShortcutsEnabled = true;"
                       "toolMode = \"text\";"
                       "beginTextPlacement(20, 24);"
                       "[beginSpacePanMode(), beginSpaceZoomMode(), effectiveToolMode(), "
                       "temporaryCameraMode, "
                       "textEditingActive].join(\",\");"));
    const QVariant textEditingSpaceResult = textEditingSpace.evaluate();
    QVERIFY2(!textEditingSpace.hasError(), qPrintable(textEditingSpace.error().toString()));
    QCOMPARE(textEditingSpaceResult.toString(), QStringLiteral("false,false,text,,true"));

    QQmlExpression cancelTextEditing(engine.rootContext(), object.data(),
                                     QStringLiteral("cancelActiveText();"));
    cancelTextEditing.evaluate();
    QVERIFY2(!cancelTextEditing.hasError(), qPrintable(cancelTextEditing.error().toString()));
}

void tst_DrawingSurfaceItem::zoomsCanvasWithHorizontalDrag()
{
    qmlRegisterType<DrawingSurfaceItem>("Congregation", 1, 0, "DrawingSurfaceItem");

    QQmlEngine engine;

    QQmlComponent component(&engine);
    const QString drawingSurfaceQml = QFINDTESTDATA("../App/qml/painting/DrawingSurface.qml");
    QVERIFY2(!drawingSurfaceQml.isEmpty(), "DrawingSurface.qml test data was not found");
    component.loadUrl(QUrl::fromLocalFile(drawingSurfaceQml));
    QTRY_VERIFY(component.isReady() || component.isError());
    QVERIFY2(component.isReady(), qPrintable(qmlErrorsToString(component.errors())));

    PaletteUtils paletteUtils;
    CanvasDocumentViewModel viewModel(&paletteUtils);

    QVariantMap initialProperties;
    initialProperties.insert(QStringLiteral("width"), 720);
    initialProperties.insert(QStringLiteral("height"), 480);
    initialProperties.insert(QStringLiteral("documentViewModel"),
                             QVariant::fromValue(static_cast<QObject*>(&viewModel)));
    initialProperties.insert(QStringLiteral("toolMode"), QStringLiteral("zoom"));

    QScopedPointer<QObject> object(component.createWithInitialProperties(initialProperties));
    QVERIFY2(!object.isNull(), qPrintable(qmlErrorsToString(component.errors())));
    auto* rootItem = qobject_cast<QQuickItem*>(object.data());
    QVERIFY(rootItem);

    DrawingSurfaceItem* canvasItem = findDrawingSurfaceItem(rootItem);
    QVERIFY(canvasItem);
    QQuickItem* canvasPaper = findItemByObjectName(rootItem, QStringLiteral("canvasPaper"));
    QVERIFY(canvasPaper);
    QQuickItem* canvasViewport = findItemByObjectName(rootItem, QStringLiteral("canvasViewport"));
    QVERIFY(canvasViewport);
    QQuickItem* canvasZoomMouseArea =
        findItemByObjectName(rootItem, QStringLiteral("canvasZoomMouseArea"));
    QVERIFY(canvasZoomMouseArea);
    QCOMPARE(canvasZoomMouseArea->parentItem(), canvasViewport);
    QTRY_COMPARE(canvasZoomMouseArea->width(), canvasViewport->width());
    QTRY_COMPARE(canvasZoomMouseArea->height(), canvasViewport->height());
    QVERIFY(canvasZoomMouseArea->isEnabled());
    QTRY_COMPARE(rootItem->property("canvasZoomScale").toReal(), 1.0);
    QCOMPARE(canvasItem->scale(), 1.0);
    QCOMPARE(canvasPaper->scale(), 1.0);

    QQmlExpression zoomIn(
        engine.rootContext(), object.data(),
        QStringLiteral("beginZoomDrag(100); updateZoomDrag(220); commitZoomDrag();"));
    zoomIn.evaluate();
    QVERIFY2(!zoomIn.hasError(), qPrintable(zoomIn.error().toString()));

    const qreal zoomedInScale = rootItem->property("canvasZoomScale").toReal();
    QVERIFY(zoomedInScale > 1.0);
    QCOMPARE(canvasItem->scale(), zoomedInScale);
    QCOMPARE(canvasPaper->scale(), zoomedInScale);

    QQmlExpression zoomOut(
        engine.rootContext(), object.data(),
        QStringLiteral("beginZoomDrag(220); updateZoomDrag(80); commitZoomDrag();"));
    zoomOut.evaluate();
    QVERIFY2(!zoomOut.hasError(), qPrintable(zoomOut.error().toString()));

    const qreal zoomedOutScale = rootItem->property("canvasZoomScale").toReal();
    QVERIFY(zoomedOutScale < zoomedInScale);
    QCOMPARE(canvasItem->scale(), zoomedOutScale);
    QCOMPARE(canvasPaper->scale(), zoomedOutScale);

    QQmlExpression createSmallCanvas(engine.rootContext(), object.data(),
                                     QStringLiteral("newCanvas(200, 120);"));
    createSmallCanvas.evaluate();
    QVERIFY2(!createSmallCanvas.hasError(), qPrintable(createSmallCanvas.error().toString()));
    QTRY_COMPARE(canvasItem->width(), 200.0);
    QTRY_COMPARE(canvasItem->height(), 120.0);
    QVERIFY(canvasItem->x() + canvasItem->width() < canvasViewport->width());
    QCOMPARE(canvasZoomMouseArea->width(), canvasViewport->width());
    QCOMPARE(canvasZoomMouseArea->height(), canvasViewport->height());

    const qreal emptyWorkspaceStartX = canvasItem->x() + canvasItem->width() + 20.0;
    QVERIFY(emptyWorkspaceStartX < canvasViewport->width());
    engine.rootContext()->setContextProperty(QStringLiteral("testEmptyWorkspaceZoomStartX"),
                                             emptyWorkspaceStartX);
    QQmlExpression emptyWorkspaceZoom(
        engine.rootContext(), object.data(),
        QStringLiteral("beginZoomDrag(testEmptyWorkspaceZoomStartX);"
                       "updateZoomDrag(testEmptyWorkspaceZoomStartX + 120);"
                       "commitZoomDrag();"));
    emptyWorkspaceZoom.evaluate();
    QVERIFY2(!emptyWorkspaceZoom.hasError(), qPrintable(emptyWorkspaceZoom.error().toString()));
    const qreal emptyWorkspaceZoomedScale = rootItem->property("canvasZoomScale").toReal();
    QVERIFY(emptyWorkspaceZoomedScale > zoomedOutScale);
    QCOMPARE(canvasItem->scale(), emptyWorkspaceZoomedScale);
    QCOMPARE(canvasPaper->scale(), emptyWorkspaceZoomedScale);
}

void tst_DrawingSurfaceItem::zoomsCanvasWithMouseWheelInEveryToolMode()
{
    qmlRegisterType<DrawingSurfaceItem>("Congregation", 1, 0, "DrawingSurfaceItem");

    QQmlEngine engine;
    QQmlComponent component(&engine);
    const QString drawingSurfaceQml = QFINDTESTDATA("../App/qml/painting/DrawingSurface.qml");
    QVERIFY2(!drawingSurfaceQml.isEmpty(), "DrawingSurface.qml test data was not found");
    component.loadUrl(QUrl::fromLocalFile(drawingSurfaceQml));
    QTRY_VERIFY(component.isReady() || component.isError());
    QVERIFY2(component.isReady(), qPrintable(qmlErrorsToString(component.errors())));

    PaletteUtils paletteUtils;
    CanvasDocumentViewModel viewModel(&paletteUtils);
    QVariantMap initialProperties;
    initialProperties.insert(QStringLiteral("width"), 720);
    initialProperties.insert(QStringLiteral("height"), 480);
    initialProperties.insert(QStringLiteral("toolMode"), QStringLiteral("brush"));
    initialProperties.insert(QStringLiteral("brushSize"), 12);
    initialProperties.insert(QStringLiteral("documentViewModel"),
                             QVariant::fromValue(static_cast<QObject*>(&viewModel)));

    QScopedPointer<QObject> object(component.createWithInitialProperties(initialProperties));
    QVERIFY2(!object.isNull(), qPrintable(qmlErrorsToString(component.errors())));
    auto* rootItem = qobject_cast<QQuickItem*>(object.data());
    QVERIFY(rootItem);

    QQuickWindow window;
    window.setGeometry(100, 100, 720, 480);
    rootItem->setParentItem(window.contentItem());
    window.show();
    QVERIFY2(QTest::qWaitForWindowExposed(&window, nativeWindowExposureTimeoutMs),
             "The wheel-zoom test window was not exposed before the compositor timeout");

    QQuickItem* canvasViewport = findItemByObjectName(rootItem, QStringLiteral("canvasViewport"));
    QVERIFY(canvasViewport);
    const QPoint wheelPoint =
        canvasViewport
            ->mapToScene(QPointF(canvasViewport->width() / 2, canvasViewport->height() / 2))
            .toPoint();
    const QStringList toolModes = {QStringLiteral("brush"), QStringLiteral("eraser"),
                                   QStringLiteral("pan"),   QStringLiteral("move"),
                                   QStringLiteral("zoom"),  QStringLiteral("shape"),
                                   QStringLiteral("fill"),  QStringLiteral("text")};

    for (const QString& toolMode : toolModes)
    {
        QVERIFY(rootItem->setProperty("toolMode", toolMode));
        QVERIFY(rootItem->setProperty("canvasZoomScale", 1.0));
        QTest::wheelEvent(&window, wheelPoint, QPoint(0, 120));
        QTRY_VERIFY2(rootItem->property("canvasZoomScale").toReal() > 1.0,
                     qPrintable(QStringLiteral("Wheel zoom did not activate in %1 mode; scale=%2")
                                    .arg(toolMode)
                                    .arg(rootItem->property("canvasZoomScale").toReal())));
    }

    QCOMPARE(rootItem->property("brushSize").toReal(), 12.0);
    const qreal zoomedInScale = rootItem->property("canvasZoomScale").toReal();
    QTest::wheelEvent(&window, wheelPoint, QPoint(0, -120));
    QTRY_VERIFY(rootItem->property("canvasZoomScale").toReal() < zoomedInScale);
}

void tst_DrawingSurfaceItem::zoomsCanvasWithNativeTemporaryCameraDrag()
{
    qmlRegisterType<DrawingSurfaceItem>("Congregation", 1, 0, "DrawingSurfaceItem");

    QQmlEngine engine;
    QQmlComponent component(&engine);
    const QString drawingSurfaceQml = QFINDTESTDATA("../App/qml/painting/DrawingSurface.qml");
    QVERIFY2(!drawingSurfaceQml.isEmpty(), "DrawingSurface.qml test data was not found");
    component.loadUrl(QUrl::fromLocalFile(drawingSurfaceQml));
    QTRY_VERIFY(component.isReady() || component.isError());
    QVERIFY2(component.isReady(), qPrintable(qmlErrorsToString(component.errors())));

    PaletteUtils paletteUtils;
    CanvasDocumentViewModel viewModel(&paletteUtils);
    QVariantMap initialProperties;
    initialProperties.insert(QStringLiteral("width"), 720);
    initialProperties.insert(QStringLiteral("height"), 480);
    initialProperties.insert(QStringLiteral("toolMode"), QStringLiteral("brush"));
    initialProperties.insert(QStringLiteral("documentViewModel"),
                             QVariant::fromValue(static_cast<QObject*>(&viewModel)));

    QScopedPointer<QObject> object(component.createWithInitialProperties(initialProperties));
    QVERIFY2(!object.isNull(), qPrintable(qmlErrorsToString(component.errors())));
    auto* rootItem = qobject_cast<QQuickItem*>(object.data());
    QVERIFY(rootItem);

    QQuickWindow window;
    window.setGeometry(100, 100, 720, 480);
    rootItem->setParentItem(window.contentItem());
    window.show();
    QVERIFY2(
        QTest::qWaitForWindowExposed(&window, nativeWindowExposureTimeoutMs),
        "The native temporary-camera test window was not exposed before the compositor timeout");

    QQuickItem* canvasViewport = findItemByObjectName(rootItem, QStringLiteral("canvasViewport"));
    QVERIFY(canvasViewport);
    QQuickItem* canvasZoomMouseArea =
        findItemByObjectName(rootItem, QStringLiteral("canvasZoomMouseArea"));
    QVERIFY(canvasZoomMouseArea);
    QObject* canvasWheelZoomHandler =
        rootItem->findChild<QObject*>(QStringLiteral("canvasWheelZoomHandler"));
    QVERIFY(canvasWheelZoomHandler);
    QCOMPARE(rootItem->property("toolMode").toString(), QStringLiteral("brush"));
    QCOMPARE(rootItem->property("temporaryCameraMode").toString(), QString{});
    QCOMPARE(canvasZoomMouseArea->property("enabled").toBool(), false);
    QTRY_COMPARE(rootItem->property("canvasZoomScale").toReal(), 1.0);

    QQmlExpression beginTemporaryZoom(engine.rootContext(), object.data(),
                                      QStringLiteral("beginSpaceZoomMode();"));
    beginTemporaryZoom.evaluate();
    QVERIFY2(!beginTemporaryZoom.hasError(), qPrintable(beginTemporaryZoom.error().toString()));
    QTRY_COMPARE(canvasZoomMouseArea->property("enabled").toBool(), true);
    QTRY_COMPARE(canvasWheelZoomHandler->property("cursorShape").toInt(),
                 static_cast<int>(Qt::CrossCursor));
    QCOMPARE(rootItem->property("temporaryCameraMode").toString(), QStringLiteral("zoom"));
    QCOMPARE(rootItem->property("toolMode").toString(), QStringLiteral("brush"));

    const QPoint startPoint =
        canvasViewport
            ->mapToScene(QPointF(canvasViewport->width() / 2, canvasViewport->height() / 2))
            .toPoint();
    const QPoint finishPoint = startPoint + QPoint(120, 0);
    QTest::mouseMove(&window, startPoint + QPoint(-1, 0));
    QTest::mouseMove(&window, startPoint);
    QTRY_COMPARE(window.cursor().shape(), Qt::CrossCursor);
    QTest::mousePress(&window, Qt::LeftButton, Qt::NoModifier, startPoint);
    QTRY_VERIFY(rootItem->property("zoomDraggingActive").toBool());
    QTest::mouseMove(&window, finishPoint, 10);
    QTRY_VERIFY(rootItem->property("canvasZoomScale").toReal() > 1.0);
    QTest::mouseRelease(&window, Qt::LeftButton, Qt::NoModifier, finishPoint);
    QTRY_VERIFY(!rootItem->property("zoomDraggingActive").toBool());

    const qreal zoomedInScale = rootItem->property("canvasZoomScale").toReal();
    QVERIFY(zoomedInScale > 1.0);

    QTest::mousePress(&window, Qt::LeftButton, Qt::NoModifier, finishPoint);
    QTRY_VERIFY(rootItem->property("zoomDraggingActive").toBool());
    QTest::mouseMove(&window, startPoint, 10);
    QTRY_VERIFY(rootItem->property("canvasZoomScale").toReal() < zoomedInScale);
    QTest::mouseRelease(&window, Qt::LeftButton, Qt::NoModifier, startPoint);
    QTRY_VERIFY(!rootItem->property("zoomDraggingActive").toBool());

    const qreal zoomedOutScale = rootItem->property("canvasZoomScale").toReal();
    QVERIFY(zoomedOutScale < zoomedInScale);

    QQmlExpression endTemporaryCamera(engine.rootContext(), object.data(),
                                      QStringLiteral("endTemporaryCameraMode();"));
    endTemporaryCamera.evaluate();
    QVERIFY2(!endTemporaryCamera.hasError(), qPrintable(endTemporaryCamera.error().toString()));
    QTRY_COMPARE(canvasZoomMouseArea->property("enabled").toBool(), false);
    QCOMPARE(rootItem->property("temporaryCameraMode").toString(), QString{});
    QCOMPARE(rootItem->property("toolMode").toString(), QStringLiteral("brush"));
    QCOMPARE(rootItem->property("canvasZoomScale").toReal(), zoomedOutScale);
}

void tst_DrawingSurfaceItem::usesToolAppropriateCanvasCursors()
{
    qmlRegisterType<DrawingSurfaceItem>("Congregation", 1, 0, "DrawingSurfaceItem");

    QQmlEngine engine;

    QQmlComponent component(&engine);
    const QString drawingSurfaceQml = QFINDTESTDATA("../App/qml/painting/DrawingSurface.qml");
    QVERIFY2(!drawingSurfaceQml.isEmpty(), "DrawingSurface.qml test data was not found");
    component.loadUrl(QUrl::fromLocalFile(drawingSurfaceQml));
    QTRY_VERIFY(component.isReady() || component.isError());
    QVERIFY2(component.isReady(), qPrintable(qmlErrorsToString(component.errors())));

    PaletteUtils paletteUtils;
    CanvasDocumentViewModel viewModel(&paletteUtils);

    QVariantMap initialProperties;
    initialProperties.insert(QStringLiteral("width"), 500);
    initialProperties.insert(QStringLiteral("height"), 360);
    initialProperties.insert(QStringLiteral("documentViewModel"),
                             QVariant::fromValue(static_cast<QObject*>(&viewModel)));
    initialProperties.insert(QStringLiteral("toolMode"), QStringLiteral("brush"));

    QScopedPointer<QObject> object(component.createWithInitialProperties(initialProperties));
    QVERIFY2(!object.isNull(), qPrintable(qmlErrorsToString(component.errors())));
    auto* rootItem = qobject_cast<QQuickItem*>(object.data());
    QVERIFY(rootItem);

    QQmlExpression cursorContract(
        engine.rootContext(), object.data(),
        QStringLiteral("var matches = [];"
                       "function matchesCursor(mode, cursor) { toolMode = mode; return "
                       "canvasCursorShape(20, 20) === cursor; }"
                       "matches.push(matchesCursor(\"brush\", Qt.BlankCursor));"
                       "matches.push(matchesCursor(\"eraser\", Qt.BlankCursor));"
                       "matches.push(matchesCursor(\"zoom\", Qt.CrossCursor));"
                       "matches.push(matchesCursor(\"shape\", Qt.CrossCursor));"
                       "matches.push(matchesCursor(\"fill\", Qt.PointingHandCursor));"
                       "matches.push(matchesCursor(\"text\", Qt.IBeamCursor));"
                       "toolMode = \"pan\"; panDraggingActive = false; "
                       "matches.push(canvasCursorShape(20, 20) === Qt.OpenHandCursor);"
                       "panDraggingActive = true; matches.push(canvasCursorShape(20, 20) === "
                       "Qt.ClosedHandCursor);"
                       "panDraggingActive = false;"
                       "appendDrawableObject({ id: 2, type: \"shape\", x: 100, y: 80, width: 100, "
                       "height: 80, shapeKind: \"rectangle\", color: \"#1976d2\" });"
                       "toolMode = \"move\";"
                       "matches.push(canvasCursorShape(20, 20) === Qt.ArrowCursor);"
                       "matches.push(canvasCursorShape(150, 120) === Qt.SizeAllCursor);"
                       "matches.push(canvasCursorShape(200, 120) === Qt.SizeHorCursor);"
                       "drawableObjectHoverHandleMode = \"resize-e\";"
                       "matches.push(canvasCursorShape(20, 20) === Qt.ArrowCursor);"
                       "beginDrawableObjectTransform(150, 120);"
                       "drawableObjectHoverHandleMode = \"resize-e\";"
                       "matches.push(canvasCursorShape(160, 130) === Qt.SizeAllCursor);"
                       "cancelActiveDrawableObjectTransform();"
                       "matches.push(drawableObjectHoverHandleMode === \"\");"
                       "toolMode = \"brush\"; beginSpacePanMode();"
                       "matches.push(canvasCursorShape(20, 20) === Qt.OpenHandCursor);"
                       "panDraggingActive = true;"
                       "matches.push(canvasCursorShape(20, 20) === Qt.ClosedHandCursor);"
                       "panDraggingActive = false;"
                       "endSpacePanMode();"
                       "matches.push(canvasCursorShape(20, 20) === Qt.BlankCursor);"
                       "beginSpaceZoomMode();"
                       "matches.push(canvasCursorShape(20, 20) === Qt.CrossCursor);"
                       "endTemporaryCameraMode();"
                       "matches.push(canvasCursorShape(20, 20) === Qt.BlankCursor);"
                       "matches.join(\",\");"));
    const QVariant cursorContractResult = cursorContract.evaluate();
    QVERIFY2(!cursorContract.hasError(), qPrintable(cursorContract.error().toString()));
    QCOMPARE(cursorContractResult.toString(),
             QStringLiteral("true,true,true,true,true,true,true,true,true,true,true,true,true,true,"
                            "true,true,true,true,true"));

    DrawingSurfaceItem* canvasItem = findDrawingSurfaceItem(rootItem);
    QVERIFY(canvasItem);
    QTRY_VERIFY(canvasItem->width() > 100);
    QTRY_VERIFY(canvasItem->height() > 100);

    QQuickItem* brushCursorGlassPane =
        findItemByObjectName(rootItem, QStringLiteral("brushCursorGlassPane"));
    QVERIFY(brushCursorGlassPane);
    QQuickItem* brushCursorOutline =
        findItemByObjectName(rootItem, QStringLiteral("brushCursorOutline"));
    QVERIFY(brushCursorOutline);
    QQuickItem* canvasViewport = findItemByObjectName(rootItem, QStringLiteral("canvasViewport"));
    QVERIFY(canvasViewport);
    QCOMPARE(brushCursorOutline->parentItem(), canvasViewport);
    QObject* brushCursorHoverHandler =
        rootItem->findChild<QObject*>(QStringLiteral("brushCursorHoverHandler"));
    QVERIFY(brushCursorHoverHandler);
    QObject* brushCursorPointHandler =
        rootItem->findChild<QObject*>(QStringLiteral("brushCursorPointHandler"));
    QVERIFY(brushCursorPointHandler);
    QQuickItem* brushCursorRingShape =
        findItemByObjectName(rootItem, QStringLiteral("brushCursorRingShape"));
    QVERIFY(brushCursorRingShape);
    QObject* brushCursorOuterRing =
        rootItem->findChild<QObject*>(QStringLiteral("brushCursorOuterRing"));
    QVERIFY(brushCursorOuterRing);
    QObject* brushCursorInnerRing =
        rootItem->findChild<QObject*>(QStringLiteral("brushCursorInnerRing"));
    QVERIFY(brushCursorInnerRing);

    QQmlExpression brushCursorContract(
        engine.rootContext(), object.data(),
        QStringLiteral("toolMode = \"brush\"; brushSize = 12; canvasZoomScale = 2.5;"
                       "[brushCursorToolActive,"
                       " brushCursorScreenDiameter,"
                       " !brushCursorUsesSystemFallback,"
                       " canvasCursorShape(20, 20) === Qt.BlankCursor].join(\",\");"));
    const QVariant brushCursorContractResult = brushCursorContract.evaluate();
    QVERIFY2(!brushCursorContract.hasError(), qPrintable(brushCursorContract.error().toString()));
    QCOMPARE(brushCursorContractResult.toString(), QStringLiteral("true,30,true,true"));
    QCOMPARE(brushCursorHoverHandler->property("blocking").toBool(), false);
    QTRY_COMPARE(brushCursorHoverHandler->property("cursorShape").toInt(),
                 static_cast<int>(Qt::BlankCursor));
    QTRY_COMPARE(brushCursorPointHandler->property("enabled").toBool(), true);
    QCOMPARE(brushCursorPointHandler->property("acceptedButtons").toInt(),
             static_cast<int>(Qt::NoButton));
    QCOMPARE(brushCursorPointHandler->property("cursorShape").toInt(),
             static_cast<int>(Qt::BlankCursor));
    QTRY_VERIFY(qAbs(brushCursorGlassPane->width() - canvasItem->width()) < 0.01);
    QTRY_VERIFY(qAbs(brushCursorGlassPane->height() - canvasItem->height()) < 0.01);
    QCOMPARE(brushCursorOutline->property("cursorDiameter").toReal(), 30.0);
    QCOMPARE(brushCursorOutline->width(), 30.0);
    QCOMPARE(brushCursorOutline->height(), 30.0);
    QVERIFY(brushCursorOutline->z() > canvasItem->z());
    QCOMPARE(brushCursorRingShape->width(), 33.0);
    QCOMPARE(brushCursorRingShape->height(), 33.0);
    QCOMPARE(brushCursorOuterRing->property("strokeWidth").toReal(), 3.0);
    QCOMPARE(brushCursorInnerRing->property("strokeWidth").toReal(), 1.0);
    const QRectF brushCursorScreenRect =
        brushCursorOutline->mapRectToItem(rootItem, brushCursorOutline->boundingRect());
    QVERIFY(qAbs(brushCursorScreenRect.width() - 30.0) < 0.01);
    QVERIFY(qAbs(brushCursorScreenRect.height() - 30.0) < 0.01);

    QQmlExpression smallBrushCursorContract(
        engine.rootContext(), object.data(),
        QStringLiteral("brushSize = 2; canvasZoomScale = 1;"
                       "[brushCursorScreenDiameter,"
                       " brushCursorEffectiveOuterStrokeWidth,"
                       " brushCursorEffectiveInnerStrokeWidth].join(\",\");"));
    const QVariant smallBrushCursorContractResult = smallBrushCursorContract.evaluate();
    QVERIFY2(!smallBrushCursorContract.hasError(),
             qPrintable(smallBrushCursorContract.error().toString()));
    QCOMPARE(smallBrushCursorContractResult.toString(), QStringLiteral("2,1,0.3333333333333333"));
    QTRY_COMPARE(brushCursorOutline->width(), 2.0);
    QTRY_COMPARE(brushCursorRingShape->width(), 3.0);
    QTRY_COMPARE(brushCursorOuterRing->property("strokeWidth").toReal(), 1.0);
    QTRY_VERIFY(qAbs(brushCursorInnerRing->property("strokeWidth").toReal() - 1.0 / 3.0) < 0.0001);

    QQmlExpression subpixelBrushCursorContract(
        engine.rootContext(), object.data(),
        QStringLiteral("brushSize = 1; canvasZoomScale = 0.5;"
                       "[brushCursorScreenDiameter,"
                       " brushCursorUsesSystemFallback,"
                       " canvasCursorShape(20, 20) === Qt.CrossCursor].join(\",\");"));
    const QVariant subpixelBrushCursorContractResult = subpixelBrushCursorContract.evaluate();
    QVERIFY2(!subpixelBrushCursorContract.hasError(),
             qPrintable(subpixelBrushCursorContract.error().toString()));
    QCOMPARE(subpixelBrushCursorContractResult.toString(), QStringLiteral("0.5,true,true"));
    QTRY_COMPARE(brushCursorPointHandler->property("cursorShape").toInt(),
                 static_cast<int>(Qt::CrossCursor));

    QQmlExpression eraserCursorContract(
        engine.rootContext(), object.data(),
        QStringLiteral(
            "toolMode = \"eraser\"; brushSize = 2; canvasZoomScale = 1;"
            "[brushCursorToolActive, canvasCursorShape(20, 20) === Qt.BlankCursor].join(\",\");"));
    const QVariant eraserCursorContractResult = eraserCursorContract.evaluate();
    QVERIFY2(!eraserCursorContract.hasError(), qPrintable(eraserCursorContract.error().toString()));
    QCOMPARE(eraserCursorContractResult.toString(), QStringLiteral("true,true"));

    QQmlExpression nonBrushCursorContract(
        engine.rootContext(), object.data(),
        QStringLiteral("toolMode = \"zoom\";"
                       "[!brushCursorToolActive,"
                       " canvasCursorShape(20, 20) === Qt.CrossCursor].join(\",\");"));
    const QVariant nonBrushCursorContractResult = nonBrushCursorContract.evaluate();
    QVERIFY2(!nonBrushCursorContract.hasError(),
             qPrintable(nonBrushCursorContract.error().toString()));
    QCOMPARE(nonBrushCursorContractResult.toString(), QStringLiteral("true,true"));
    QTRY_COMPARE(brushCursorPointHandler->property("enabled").toBool(), false);
    QTRY_COMPARE(brushCursorHoverHandler->property("cursorShape").toInt(),
                 static_cast<int>(Qt::CrossCursor));
}

void tst_DrawingSurfaceItem::tracksBrushCursorDuringNativePointerInput()
{
    qmlRegisterType<DrawingSurfaceItem>("Congregation", 1, 0, "DrawingSurfaceItem");

    QQmlEngine engine;
    QQmlComponent component(&engine);
    const QString drawingSurfaceQml = QFINDTESTDATA("../App/qml/painting/DrawingSurface.qml");
    QVERIFY2(!drawingSurfaceQml.isEmpty(), "DrawingSurface.qml test data was not found");
    component.loadUrl(QUrl::fromLocalFile(drawingSurfaceQml));
    QTRY_VERIFY(component.isReady() || component.isError());
    QVERIFY2(component.isReady(), qPrintable(qmlErrorsToString(component.errors())));

    PaletteUtils paletteUtils;
    CanvasDocumentViewModel viewModel(&paletteUtils);
    QVariantMap initialProperties;
    initialProperties.insert(QStringLiteral("width"), 500);
    initialProperties.insert(QStringLiteral("height"), 360);
    initialProperties.insert(QStringLiteral("brushSize"), 24);
    initialProperties.insert(QStringLiteral("toolMode"), QStringLiteral("brush"));
    initialProperties.insert(QStringLiteral("documentViewModel"),
                             QVariant::fromValue(static_cast<QObject*>(&viewModel)));

    QScopedPointer<QObject> object(component.createWithInitialProperties(initialProperties));
    QVERIFY2(!object.isNull(), qPrintable(qmlErrorsToString(component.errors())));
    auto* rootItem = qobject_cast<QQuickItem*>(object.data());
    QVERIFY(rootItem);

    QQuickWindow window;
    window.setGeometry(100, 100, 500, 360);
    rootItem->setParentItem(window.contentItem());
    window.show();
    QVERIFY2(QTest::qWaitForWindowExposed(&window, nativeWindowExposureTimeoutMs),
             "The native pointer test window was not exposed before the compositor timeout");

    DrawingSurfaceItem* canvasItem = findDrawingSurfaceItem(rootItem);
    QVERIFY(canvasItem);
    QQuickItem* brushCursorOutline =
        findItemByObjectName(rootItem, QStringLiteral("brushCursorOutline"));
    QVERIFY(brushCursorOutline);
    QQuickItem* canvasViewport = findItemByObjectName(rootItem, QStringLiteral("canvasViewport"));
    QVERIFY(canvasViewport);
    QCOMPARE(brushCursorOutline->parentItem(), canvasViewport);
    QObject* brushCursorHoverHandler =
        rootItem->findChild<QObject*>(QStringLiteral("brushCursorHoverHandler"));
    QVERIFY(brushCursorHoverHandler);
    QObject* brushCursorPointHandler =
        rootItem->findChild<QObject*>(QStringLiteral("brushCursorPointHandler"));
    QVERIFY(brushCursorPointHandler);
    QCOMPARE(brushCursorPointHandler->property("acceptedButtons").toInt(),
             static_cast<int>(Qt::NoButton));
    QVERIFY(brushCursorOutline->z() > canvasItem->z());
    QTRY_VERIFY(canvasItem->width() > 100);
    QTRY_VERIFY(canvasItem->height() > 100);

    QVERIFY(rootItem->setProperty("canvasZoomScale", 1.5));
    QTRY_COMPARE(canvasItem->scale(), 1.5);

    const QPointF hoverScenePoint =
        canvasItem->mapToScene(QPointF(canvasItem->width() / 2, canvasItem->height() / 2));
    const QPoint hoverWindowPoint(qRound(hoverScenePoint.x()), qRound(hoverScenePoint.y()));
    QTest::mouseMove(&window, hoverWindowPoint);
    QTRY_VERIFY(brushCursorHoverHandler->property("hovered").toBool());
    QTRY_VERIFY(brushCursorOutline->isVisible());
    QTRY_COMPARE(window.cursor().shape(), Qt::BlankCursor);

    QRectF outlineSceneRect =
        brushCursorOutline->mapRectToScene(brushCursorOutline->boundingRect());
    QVERIFY(qAbs(outlineSceneRect.width() - 36.0) < 0.1);
    QVERIFY(qAbs(outlineSceneRect.height() - 36.0) < 0.1);
    QVERIFY(QLineF(outlineSceneRect.center(), hoverScenePoint).length() < 1.0);

    QTest::qWait(50);
    const QImage hoverFrame = window.grabWindow();
    QVERIFY(!hoverFrame.isNull());
    const int darkRingPixels =
        countLogicalAnnulusPixels(hoverFrame, window.size(), outlineSceneRect.center(), 16.0, 20.0,
                                  [](const QColor& pixel)
                                  {
                                      return pixel.alpha() > 200 && pixel.red() < 80 &&
                                             pixel.green() < 80 && pixel.blue() < 80;
                                  });
    QVERIFY2(darkRingPixels >= 12,
             "The vector brush outline did not render above the blank raster surface");

    QVERIFY(rootItem->setProperty("canvasZoomScale", 1.0));
    QTRY_COMPARE(canvasItem->scale(), 1.0);
    const QPointF edgeScenePoint = canvasItem->mapToScene(QPointF(4, canvasItem->height() / 2));
    const QPoint edgeWindowPoint(qRound(edgeScenePoint.x()), qRound(edgeScenePoint.y()));
    QTest::mouseMove(&window, edgeWindowPoint);
    QTRY_VERIFY(
        QLineF(brushCursorOutline->mapRectToScene(brushCursorOutline->boundingRect()).center(),
               QPointF(edgeWindowPoint))
            .length() < 1.0);
    QVERIFY(brushCursorOutline->mapRectToScene(brushCursorOutline->boundingRect()).left() <
            canvasItem->mapRectToScene(canvasItem->boundingRect()).left());
    QVERIFY(rootItem->setProperty("canvasZoomScale", 1.5));
    QTRY_COMPARE(canvasItem->scale(), 1.5);
    QTest::mouseMove(&window, hoverWindowPoint);

    QTest::mousePress(&window, Qt::LeftButton, Qt::NoModifier, hoverWindowPoint);
    QTRY_VERIFY(brushCursorPointHandler->property("active").toBool());
    const QPoint dragWindowPoint = hoverWindowPoint + QPoint(18, 12);
    QTest::mouseMove(&window, dragWindowPoint, 10);
    QTRY_VERIFY(brushCursorPointHandler->property("active").toBool());
    QTRY_VERIFY(
        QLineF(brushCursorOutline->mapRectToScene(brushCursorOutline->boundingRect()).center(),
               QPointF(dragWindowPoint))
            .length() < 1.0);
    QTest::mouseRelease(&window, Qt::LeftButton, Qt::NoModifier, dragWindowPoint);
    QTRY_VERIFY(!brushCursorPointHandler->property("active").toBool());
    QTRY_VERIFY(
        QLineF(brushCursorOutline->mapRectToScene(brushCursorOutline->boundingRect()).center(),
               QPointF(dragWindowPoint))
            .length() < 1.0);

    QVERIFY(rootItem->setProperty("toolMode", QStringLiteral("eraser")));
    QTest::mouseMove(&window, dragWindowPoint + QPoint(1, 0));
    QTRY_VERIFY(brushCursorOutline->isVisible());
    QTRY_COMPARE(window.cursor().shape(), Qt::BlankCursor);

    QVERIFY(rootItem->setProperty("toolMode", QStringLiteral("pan")));
    QTest::mouseMove(&window, dragWindowPoint + QPoint(2, 0));
    QTRY_VERIFY(!brushCursorOutline->isVisible());
    QTRY_COMPARE(window.cursor().shape(), Qt::OpenHandCursor);
}

void tst_DrawingSurfaceItem::pastesSystemClipboardImageAsTransformableObject()
{
    qmlRegisterType<DrawingSurfaceItem>("Congregation", 1, 0, "DrawingSurfaceItem");

    QClipboard* clipboard = QGuiApplication::clipboard();
    QVERIFY(clipboard);
    ClipboardContentsGuard clipboardGuard(clipboard);
    clipboard->setText(QStringLiteral("not an image"), QClipboard::Clipboard);

    QQmlEngine engine;
    QQmlComponent component(&engine);
    const QString drawingSurfaceQml = QFINDTESTDATA("../App/qml/painting/DrawingSurface.qml");
    QVERIFY2(!drawingSurfaceQml.isEmpty(), "DrawingSurface.qml test data was not found");
    component.loadUrl(QUrl::fromLocalFile(drawingSurfaceQml));
    QTRY_VERIFY(component.isReady() || component.isError());
    QVERIFY2(component.isReady(), qPrintable(qmlErrorsToString(component.errors())));

    PaletteUtils paletteUtils;
    CanvasDocumentViewModel viewModel(&paletteUtils);
    QVariantMap initialProperties;
    initialProperties.insert(QStringLiteral("width"), 500);
    initialProperties.insert(QStringLiteral("height"), 360);
    initialProperties.insert(QStringLiteral("documentViewModel"),
                             QVariant::fromValue(static_cast<QObject*>(&viewModel)));
    initialProperties.insert(QStringLiteral("toolMode"), QStringLiteral("move"));

    QScopedPointer<QObject> object(component.createWithInitialProperties(initialProperties));
    QVERIFY2(!object.isNull(), qPrintable(qmlErrorsToString(component.errors())));
    auto* rootItem = qobject_cast<QQuickItem*>(object.data());
    QVERIFY(rootItem);
    DrawingSurfaceItem* canvasItem = findDrawingSurfaceItem(rootItem);
    QVERIFY(canvasItem);
    QTRY_VERIFY(canvasItem->width() > 100);
    QTRY_VERIFY(canvasItem->height() > 100);

    QQmlExpression beginDraftText(
        engine.rootContext(), object.data(),
        QStringLiteral("toolMode = \"text\"; beginTextPlacement(24, 24);"));
    beginDraftText.evaluate();
    QVERIFY2(!beginDraftText.hasError(), qPrintable(beginDraftText.error().toString()));
    QQuickItem* textToolEditor = findItemByObjectName(rootItem, QStringLiteral("textToolEditor"));

    QVERIFY(textToolEditor);
    QVERIFY(textToolEditor->setProperty("text", QStringLiteral("Uncommitted draft")));
    QVERIFY(rootItem->property("textEditingActive").toBool());

    const QVariantList objectsBeforeFailure = rootItem->property("drawableObjects").toList();
    const int selectedObjectBeforeFailure = rootItem->property("selectedDrawableObjectId").toInt();
    const QVariantMap textClipboardResult =
        canvasItem->clipboardImageObject(canvasItem->width(), canvasItem->height());
    QCOMPARE(textClipboardResult.value(QStringLiteral("status")).toString(),
             QStringLiteral("no-image"));
    QVERIFY(!textClipboardResult.contains(QStringLiteral("source")));
    QQmlExpression pasteTextOnlyClipboard(engine.rootContext(), object.data(),
                                          QStringLiteral("pasteClipboardImage();"));
    QCOMPARE(pasteTextOnlyClipboard.evaluate().toBool(), false);
    QVERIFY2(!pasteTextOnlyClipboard.hasError(),
             qPrintable(pasteTextOnlyClipboard.error().toString()));
    QCOMPARE(rootItem->property("clipboardImagePasteErrorCode").toString(),
             QStringLiteral("no-image"));
    QCOMPARE(rootItem->property("drawableObjects").toList(), objectsBeforeFailure);
    QCOMPARE(rootItem->property("selectedDrawableObjectId").toInt(), selectedObjectBeforeFailure);
    QCOMPARE(rootItem->property("toolMode").toString(), QStringLiteral("text"));
    QVERIFY(rootItem->property("textEditingActive").toBool());
    QCOMPARE(textToolEditor->property("text").toString(), QStringLiteral("Uncommitted draft"));

    QQmlExpression cancelDraftText(engine.rootContext(), object.data(),
                                   QStringLiteral("cancelActiveText(); toolMode = \"move\";"));
    cancelDraftText.evaluate();
    QVERIFY2(!cancelDraftText.hasError(), qPrintable(cancelDraftText.error().toString()));

    auto* invalidImageMimeData = new QMimeData();
    invalidImageMimeData->setData(QStringLiteral("application/x-qt-image"),
                                  QByteArrayLiteral("not an encoded image"));
    clipboard->setMimeData(invalidImageMimeData, QClipboard::Clipboard);
    const QVariantMap invalidClipboardResult =
        canvasItem->clipboardImageObject(canvasItem->width(), canvasItem->height());
    QCOMPARE(invalidClipboardResult.value(QStringLiteral("status")).toString(),
             QStringLiteral("decode-failed"));
    QQmlExpression pasteInvalidClipboard(engine.rootContext(), object.data(),
                                         QStringLiteral("pasteClipboardImage();"));
    QCOMPARE(pasteInvalidClipboard.evaluate().toBool(), false);
    QVERIFY2(!pasteInvalidClipboard.hasError(),
             qPrintable(pasteInvalidClipboard.error().toString()));
    QCOMPARE(rootItem->property("clipboardImagePasteErrorCode").toString(),
             QStringLiteral("decode-failed"));
    QCOMPARE(rootItem->property("drawableObjects").toList(), objectsBeforeFailure);
    QCOMPARE(rootItem->property("selectedDrawableObjectId").toInt(), selectedObjectBeforeFailure);

    QImage oversizedClipboardImage(32769, 1, QImage::Format_ARGB32_Premultiplied);
    oversizedClipboardImage.fill(Qt::black);
    clipboard->setImage(oversizedClipboardImage, QClipboard::Clipboard);
    const QVariantMap oversizedClipboardResult =
        canvasItem->clipboardImageObject(canvasItem->width(), canvasItem->height());
    QCOMPARE(oversizedClipboardResult.value(QStringLiteral("status")).toString(),
             QStringLiteral("image-too-large"));
    QQmlExpression pasteOversizedClipboard(engine.rootContext(), object.data(),
                                           QStringLiteral("pasteClipboardImage();"));
    QCOMPARE(pasteOversizedClipboard.evaluate().toBool(), false);
    QVERIFY2(!pasteOversizedClipboard.hasError(),
             qPrintable(pasteOversizedClipboard.error().toString()));
    QCOMPARE(rootItem->property("clipboardImagePasteErrorCode").toString(),
             QStringLiteral("image-too-large"));
    QCOMPARE(rootItem->property("drawableObjects").toList(), objectsBeforeFailure);
    QCOMPARE(rootItem->property("selectedDrawableObjectId").toInt(), selectedObjectBeforeFailure);

    QImage clipboardImage(640, 320, QImage::Format_ARGB32_Premultiplied);
    clipboardImage.fill(QColor(QStringLiteral("#5e35b1")));
    clipboard->setImage(clipboardImage, QClipboard::Clipboard);

    QVERIFY(rootItem->setProperty("canvasItemReady", false));
    QQmlExpression pasteBeforeCanvasReady(engine.rootContext(), object.data(),
                                          QStringLiteral("pasteClipboardImage();"));
    QCOMPARE(pasteBeforeCanvasReady.evaluate().toBool(), false);
    QVERIFY2(!pasteBeforeCanvasReady.hasError(),
             qPrintable(pasteBeforeCanvasReady.error().toString()));
    QCOMPARE(rootItem->property("clipboardImagePasteErrorCode").toString(),
             QStringLiteral("canvas-unavailable"));
    QCOMPARE(rootItem->property("drawableObjects").toList(), objectsBeforeFailure);
    QCOMPARE(rootItem->property("selectedDrawableObjectId").toInt(), selectedObjectBeforeFailure);
    QVERIFY(rootItem->setProperty("canvasItemReady", true));

    const QVariantMap clipboardObject =
        canvasItem->clipboardImageObject(canvasItem->width() * 0.8, canvasItem->height() * 0.8);
    QVERIFY(!clipboardObject.isEmpty());
    QCOMPARE(clipboardObject.value(QStringLiteral("status")).toString(), QStringLiteral("ready"));
    QCOMPARE(clipboardObject.value(QStringLiteral("originalWidth")).toInt(),
             clipboardImage.width());
    QCOMPARE(clipboardObject.value(QStringLiteral("originalHeight")).toInt(),
             clipboardImage.height());
    const QUrl cachedSource(clipboardObject.value(QStringLiteral("source")).toString());
    QVERIFY(cachedSource.isLocalFile());
    QVERIFY(QFileInfo::exists(cachedSource.toLocalFile()));
    const QImage cachedImage(cachedSource.toLocalFile());
    QCOMPARE(cachedImage.size(), clipboardImage.size());
    QCOMPARE(cachedImage.pixelColor(0, 0), clipboardImage.pixelColor(0, 0));

    QQmlExpression pasteClipboardImage(engine.rootContext(), object.data(),
                                       QStringLiteral("pasteClipboardImage();"));
    QCOMPARE(pasteClipboardImage.evaluate().toBool(), true);
    QVERIFY2(!pasteClipboardImage.hasError(), qPrintable(pasteClipboardImage.error().toString()));

    const QVariantList objects = rootItem->property("drawableObjects").toList();
    QCOMPARE(objects.size(), 2);
    const QVariantMap pastedObject = objects.constLast().toMap();
    QCOMPARE(pastedObject.value(QStringLiteral("type")).toString(), QStringLiteral("image"));
    QCOMPARE(pastedObject.value(QStringLiteral("name")).toString(), QStringLiteral("Pasted Image"));
    QCOMPARE(pastedObject.value(QStringLiteral("source")).toString(), cachedSource.toString());
    QCOMPARE(pastedObject.value(QStringLiteral("originalWidth")).toInt(), clipboardImage.width());
    QCOMPARE(pastedObject.value(QStringLiteral("originalHeight")).toInt(), clipboardImage.height());
    QCOMPARE(rootItem->property("selectedDrawableObjectId").toInt(),
             pastedObject.value(QStringLiteral("id")).toInt());

    const QSize maximumObjectSize(qMax(1, qRound(canvasItem->width() * 0.8)),
                                  qMax(1, qRound(canvasItem->height() * 0.8)));
    const QSize expectedObjectSize =
        clipboardImage.size().scaled(maximumObjectSize, Qt::KeepAspectRatio);
    QCOMPARE(pastedObject.value(QStringLiteral("width")).toInt(), expectedObjectSize.width());
    QCOMPARE(pastedObject.value(QStringLiteral("height")).toInt(), expectedObjectSize.height());
    QCOMPARE(pastedObject.value(QStringLiteral("x")).toReal(),
             (canvasItem->width() - expectedObjectSize.width()) / 2.0);
    QCOMPARE(pastedObject.value(QStringLiteral("y")).toReal(),
             (canvasItem->height() - expectedObjectSize.height()) / 2.0);

    QQmlExpression transformPastedImage(
        engine.rootContext(), object.data(),
        QStringLiteral("var pasted = selectedDrawableObject();"
                       "var centerX = pasted.x + pasted.width / 2;"
                       "var centerY = pasted.y + pasted.height / 2;"
                       "var transformable = drawableObjectIsTransformable(pasted);"
                       "beginDrawableObjectTransform(centerX, centerY);"
                       "updateDrawableObjectTransform(centerX + 24, centerY + 16);"
                       "commitDrawableObjectTransform();"
                       "transformable;"));
    QCOMPARE(transformPastedImage.evaluate().toBool(), true);
    QVERIFY2(!transformPastedImage.hasError(), qPrintable(transformPastedImage.error().toString()));

    const QVariantMap movedObject =
        rootItem->property("drawableObjects").toList().constLast().toMap();
    QCOMPARE(movedObject.value(QStringLiteral("x")).toReal(),
             pastedObject.value(QStringLiteral("x")).toReal() + 24.0);
    QCOMPARE(movedObject.value(QStringLiteral("y")).toReal(),
             pastedObject.value(QStringLiteral("y")).toReal() + 16.0);

    const int objectCountBeforeDuplicatePaste =
        rootItem->property("drawableObjects").toList().size();
    QQmlExpression pasteDuplicateClipboardImage(engine.rootContext(), object.data(),
                                                QStringLiteral("pasteClipboardImage();"));
    QCOMPARE(pasteDuplicateClipboardImage.evaluate().toBool(), true);
    QVERIFY2(!pasteDuplicateClipboardImage.hasError(),
             qPrintable(pasteDuplicateClipboardImage.error().toString()));
    const QVariantList objectsAfterDuplicatePaste = rootItem->property("drawableObjects").toList();
    QCOMPARE(objectsAfterDuplicatePaste.size(), objectCountBeforeDuplicatePaste + 1);
    const QVariantMap duplicatePastedObject = objectsAfterDuplicatePaste.constLast().toMap();
    QCOMPARE(duplicatePastedObject.value(QStringLiteral("source")).toString(),
             cachedSource.toString());
    QCOMPARE(duplicatePastedObject.value(QStringLiteral("x")).toReal(),
             qMin(canvasItem->width() - expectedObjectSize.width(),
                  (canvasItem->width() - expectedObjectSize.width()) / 2.0 + 16.0));
    QCOMPARE(duplicatePastedObject.value(QStringLiteral("y")).toReal(),
             qMin(canvasItem->height() - expectedObjectSize.height(),
                  (canvasItem->height() - expectedObjectSize.height()) / 2.0 + 16.0));
    QVERIFY(duplicatePastedObject.value(QStringLiteral("id")).toInt() !=
            pastedObject.value(QStringLiteral("id")).toInt());

    const QString cachedImagePath = cachedSource.toLocalFile();
    QVERIFY(QFile::remove(cachedImagePath));
    QVERIFY(QDir().mkpath(cachedImagePath));
    CachePathCleanupGuard obstructedCachePath(cachedImagePath);
    const QVariantList objectsBeforeCacheFailure = rootItem->property("drawableObjects").toList();
    const int selectedObjectBeforeCacheFailure =
        rootItem->property("selectedDrawableObjectId").toInt();
    const QVariantMap obstructedCacheResult =
        canvasItem->clipboardImageObject(canvasItem->width(), canvasItem->height());
    QCOMPARE(obstructedCacheResult.value(QStringLiteral("status")).toString(),
             QStringLiteral("cache-write-failed"));
    QQmlExpression pasteWithObstructedCache(engine.rootContext(), object.data(),
                                            QStringLiteral("pasteClipboardImage();"));
    QCOMPARE(pasteWithObstructedCache.evaluate().toBool(), false);
    QVERIFY2(!pasteWithObstructedCache.hasError(),
             qPrintable(pasteWithObstructedCache.error().toString()));
    QCOMPARE(rootItem->property("clipboardImagePasteErrorCode").toString(),
             QStringLiteral("cache-write-failed"));
    QCOMPARE(rootItem->property("drawableObjects").toList(), objectsBeforeCacheFailure);
    QCOMPARE(rootItem->property("selectedDrawableObjectId").toInt(),
             selectedObjectBeforeCacheFailure);
    QVERIFY(QDir().rmdir(cachedImagePath));
    obstructedCachePath.dismiss();

    const QVariantMap recoveredCacheResult =
        canvasItem->clipboardImageObject(canvasItem->width(), canvasItem->height());
    QCOMPARE(recoveredCacheResult.value(QStringLiteral("status")).toString(),
             QStringLiteral("ready"));
    QVERIFY(QFileInfo::exists(cachedImagePath));

    QFile corruptedCacheFile(cachedImagePath);
    QVERIFY(corruptedCacheFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QCOMPARE(corruptedCacheFile.write("corrupted PNG cache entry"), 25);
    corruptedCacheFile.close();
    const QVariantMap repairedCacheResult =
        canvasItem->clipboardImageObject(canvasItem->width(), canvasItem->height());
    QCOMPARE(repairedCacheResult.value(QStringLiteral("status")).toString(),
             QStringLiteral("ready"));
    const QImage repairedCachedImage(cachedImagePath);
    QCOMPARE(repairedCachedImage.size(), clipboardImage.size());
    QCOMPARE(repairedCachedImage.pixelColor(0, 0), clipboardImage.pixelColor(0, 0));

    QTemporaryDir copiedFileDirectory;
    QVERIFY(copiedFileDirectory.isValid());
    QImage copiedFileImage(72, 48, QImage::Format_ARGB32_Premultiplied);
    copiedFileImage.fill(QColor(QStringLiteral("#00897b")));
    const QString copiedFilePath = copiedFileDirectory.filePath(QStringLiteral("copied-image.png"));
    QVERIFY(copiedFileImage.save(copiedFilePath, "PNG"));
    auto* copiedFileMimeData = new QMimeData();
    copiedFileMimeData->setUrls({QUrl::fromLocalFile(copiedFilePath)});
    clipboard->setMimeData(copiedFileMimeData, QClipboard::Clipboard);

    const QVariantMap copiedFileResult =
        canvasItem->clipboardImageObject(canvasItem->width(), canvasItem->height());
    QCOMPARE(copiedFileResult.value(QStringLiteral("status")).toString(), QStringLiteral("ready"));
    QCOMPARE(copiedFileResult.value(QStringLiteral("originalWidth")).toInt(),
             copiedFileImage.width());
    QCOMPARE(copiedFileResult.value(QStringLiteral("originalHeight")).toInt(),
             copiedFileImage.height());

    const int objectCountBeforeFilePaste = rootItem->property("drawableObjects").toList().size();
    QQmlExpression pasteCopiedFile(engine.rootContext(), object.data(),
                                   QStringLiteral("pasteClipboardImage();"));
    QCOMPARE(pasteCopiedFile.evaluate().toBool(), true);
    QVERIFY2(!pasteCopiedFile.hasError(), qPrintable(pasteCopiedFile.error().toString()));
    QCOMPARE(rootItem->property("drawableObjects").toList().size(), objectCountBeforeFilePaste + 1);
    QCOMPARE(rootItem->property("clipboardImagePasteErrorCode").toString(), QString());

    const QUrl copiedFileCachedSource(copiedFileResult.value(QStringLiteral("source")).toString());
    QVERIFY(copiedFileCachedSource.isLocalFile());
    clipboard->clear(QClipboard::Clipboard);
    const QImage imageAfterClipboardOwnershipEnded(copiedFileCachedSource.toLocalFile());
    QCOMPARE(imageAfterClipboardOwnershipEnded.size(), copiedFileImage.size());
    QCOMPARE(imageAfterClipboardOwnershipEnded.pixelColor(0, 0), copiedFileImage.pixelColor(0, 0));
}

void tst_DrawingSurfaceItem::importsDraggedImageMimeFilesAndWebImages()
{
    DrawingSurfaceItem canvasItem;
    QSignalSpy readySpy(&canvasItem, &DrawingSurfaceItem::droppedImageReady);
    QSignalSpy failedSpy(&canvasItem, &DrawingSurfaceItem::droppedImageFailed);

    QImage sourceImage(96, 64, QImage::Format_ARGB32_Premultiplied);
    sourceImage.fill(QColor(QStringLiteral("#3949ab")));
    QByteArray encodedPng;
    QBuffer encodedPngBuffer(&encodedPng);
    QVERIFY(encodedPngBuffer.open(QIODevice::WriteOnly));
    QVERIFY(sourceImage.save(&encodedPngBuffer, "PNG"));

    FakeDropEvent mimeDrop;
    mimeDrop.m_x = 135.5;
    mimeDrop.m_y = 82.25;
    mimeDrop.m_dataByFormat.insert(QStringLiteral("image/png"), encodedPng);
    QVERIFY(canvasItem.canImportDroppedImage(&mimeDrop));
    canvasItem.importDroppedImage(&mimeDrop, 320, 240);
    QCOMPARE(readySpy.count(), 1);
    QCOMPARE(failedSpy.count(), 0);

    const QList<QVariant> mimeReadyArguments = readySpy.takeFirst();
    const QVariantMap mimeObject = mimeReadyArguments.at(0).toMap();
    QCOMPARE(mimeObject.value(QStringLiteral("status")).toString(), QStringLiteral("ready"));
    QCOMPARE(mimeObject.value(QStringLiteral("originalWidth")).toInt(), sourceImage.width());
    QCOMPARE(mimeObject.value(QStringLiteral("originalHeight")).toInt(), sourceImage.height());
    QCOMPARE(mimeReadyArguments.at(1).toReal(), mimeDrop.m_x);
    QCOMPARE(mimeReadyArguments.at(2).toReal(), mimeDrop.m_y);
    const QUrl mimeCachedSource(mimeObject.value(QStringLiteral("source")).toString());
    QVERIFY(mimeCachedSource.isLocalFile());
    QCOMPARE(QImage(mimeCachedSource.toLocalFile()).size(), sourceImage.size());

    QTemporaryDir localImageDirectory;
    QVERIFY(localImageDirectory.isValid());
    const QString localImagePath = localImageDirectory.filePath(QStringLiteral("finder-image.png"));
    QVERIFY(sourceImage.save(localImagePath, "PNG"));

    FakeDropEvent localFileDrop;
    localFileDrop.m_x = 48;
    localFileDrop.m_y = 52;
    localFileDrop.m_urls = {QUrl::fromLocalFile(localImagePath)};
    QVERIFY(canvasItem.canImportDroppedImage(&localFileDrop));
    canvasItem.importDroppedImage(&localFileDrop, 320, 240);
    QCOMPARE(readySpy.count(), 1);
    QCOMPARE(failedSpy.count(), 0);

    const QVariantMap localFileObject = readySpy.takeFirst().at(0).toMap();
    QCOMPARE(localFileObject.value(QStringLiteral("status")).toString(), QStringLiteral("ready"));
    QCOMPARE(localFileObject.value(QStringLiteral("originalSource")).toString(),
             QUrl::fromLocalFile(localImagePath).toString());
    QCOMPARE(localFileObject.value(QStringLiteral("suggestedName")).toString(),
             QStringLiteral("finder-image.png"));
    const QUrl localFileCachedSource(localFileObject.value(QStringLiteral("source")).toString());
    QVERIFY(localFileCachedSource.isLocalFile());
    QVERIFY(QFile::remove(localImagePath));
    QCOMPARE(QImage(localFileCachedSource.toLocalFile()).size(), sourceImage.size());

    QTcpServer imageServer;
    QVERIFY(imageServer.listen(QHostAddress::LocalHost));
    connect(&imageServer, &QTcpServer::newConnection, &imageServer,
            [&imageServer, encodedPng]()
            {
                while (QTcpSocket* socket = imageServer.nextPendingConnection())
                {
                    connect(
                        socket, &QTcpSocket::readyRead, socket,
                        [socket, encodedPng]()
                        {
                            socket->readAll();
                            QByteArray response = QByteArrayLiteral(
                                "HTTP/1.1 200 OK\r\nContent-Type: image/png\r\nContent-Length: ");
                            response.append(QByteArray::number(encodedPng.size()));
                            response.append(QByteArrayLiteral("\r\nConnection: close\r\n\r\n"));
                            response.append(encodedPng);
                            socket->write(response);
                            socket->disconnectFromHost();
                        });
                }
            });

    const QUrl remoteImageUrl(
        QStringLiteral("http://127.0.0.1:%1/site-image.png?token=secret#drag-fragment")
            .arg(imageServer.serverPort()));
    QUrl storedRemoteImageUrl = remoteImageUrl;
    storedRemoteImageUrl.setQuery(QString());
    storedRemoteImageUrl.setFragment(QString());
    FakeDropEvent webImageDrop;
    webImageDrop.m_x = 210;
    webImageDrop.m_y = 160;
    webImageDrop.m_html = QStringLiteral("<a href=\"https://example.test\"><img src=\"%1\"></a>")
                              .arg(remoteImageUrl.toString());
    QVERIFY(canvasItem.canImportDroppedImage(&webImageDrop));
    canvasItem.importDroppedImage(&webImageDrop, 320, 240);
    QTRY_COMPARE_WITH_TIMEOUT(readySpy.count(), 1, 5000);
    QCOMPARE(failedSpy.count(), 0);

    const QList<QVariant> webReadyArguments = readySpy.takeFirst();
    const QVariantMap webObject = webReadyArguments.at(0).toMap();
    QCOMPARE(webObject.value(QStringLiteral("status")).toString(), QStringLiteral("ready"));
    QCOMPARE(webObject.value(QStringLiteral("originalSource")).toString(),
             storedRemoteImageUrl.toString());
    QCOMPARE(webObject.value(QStringLiteral("suggestedName")).toString(),
             QStringLiteral("site-image.png"));
    QCOMPARE(webReadyArguments.at(1).toReal(), webImageDrop.m_x);
    QCOMPARE(webReadyArguments.at(2).toReal(), webImageDrop.m_y);
    QCOMPARE(
        QImage(QUrl(webObject.value(QStringLiteral("source")).toString()).toLocalFile()).size(),
        sourceImage.size());

    FakeDropEvent nonImageDrop;
    nonImageDrop.m_text = QStringLiteral("plain text");
    QVERIFY(!canvasItem.canImportDroppedImage(&nonImageDrop));

    qmlRegisterType<DrawingSurfaceItem>("Congregation", 1, 0, "DrawingSurfaceItem");
    QQmlEngine engine;
    QQmlComponent component(&engine);
    const QString drawingSurfaceQml = QFINDTESTDATA("../App/qml/painting/DrawingSurface.qml");
    QVERIFY2(!drawingSurfaceQml.isEmpty(), "DrawingSurface.qml test data was not found");
    component.loadUrl(QUrl::fromLocalFile(drawingSurfaceQml));
    QTRY_VERIFY(component.isReady() || component.isError());
    QVERIFY2(component.isReady(), qPrintable(qmlErrorsToString(component.errors())));

    PaletteUtils paletteUtils;
    CanvasDocumentViewModel viewModel(&paletteUtils);
    QVariantMap initialProperties;
    initialProperties.insert(QStringLiteral("width"), 500);
    initialProperties.insert(QStringLiteral("height"), 360);
    initialProperties.insert(QStringLiteral("documentViewModel"),
                             QVariant::fromValue(static_cast<QObject*>(&viewModel)));
    initialProperties.insert(QStringLiteral("toolMode"), QStringLiteral("brush"));
    QScopedPointer<QObject> surfaceObject(component.createWithInitialProperties(initialProperties));
    QVERIFY2(surfaceObject, qPrintable(qmlErrorsToString(component.errors())));
    auto* rootItem = qobject_cast<QQuickItem*>(surfaceObject.data());
    QVERIFY(rootItem);
    QQuickWindow dropWindow;
    dropWindow.resize(500, 360);
    rootItem->setParentItem(dropWindow.contentItem());
    dropWindow.show();
    QTRY_VERIFY_WITH_TIMEOUT(dropWindow.isExposed(), nativeWindowExposureTimeoutMs);
    DrawingSurfaceItem* qmlCanvasItem = findDrawingSurfaceItem(rootItem);
    QVERIFY(qmlCanvasItem);
    QTRY_VERIFY(qmlCanvasItem->width() > 100);
    QTRY_VERIFY(qmlCanvasItem->height() > 100);

    engine.rootContext()->setContextProperty(QStringLiteral("testDroppedImageObject"), webObject);
    QSignalSpy insertedSpy(rootItem, SIGNAL(imageDropSucceeded()));
    QSignalSpy insertionFailedSpy(rootItem, SIGNAL(imageDropFailed(QString)));
    QQmlExpression insertAtDropPoint(
        engine.rootContext(), surfaceObject.data(),
        QStringLiteral("insertDroppedImageObject(testDroppedImageObject, 210, 160);"));
    QCOMPARE(insertAtDropPoint.evaluate().toBool(), true);
    QVERIFY2(!insertAtDropPoint.hasError(), qPrintable(insertAtDropPoint.error().toString()));
    QCOMPARE(insertedSpy.count(), 1);
    QCOMPARE(insertionFailedSpy.count(), 0);

    const QVariantList drawableObjects = rootItem->property("drawableObjects").toList();
    QCOMPARE(drawableObjects.size(), 2);
    const QVariantMap droppedObject = drawableObjects.constLast().toMap();
    QCOMPARE(droppedObject.value(QStringLiteral("type")).toString(), QStringLiteral("image"));
    QCOMPARE(droppedObject.value(QStringLiteral("name")).toString(),
             QStringLiteral("site-image.png"));
    QCOMPARE(droppedObject.value(QStringLiteral("originalSource")).toString(),
             storedRemoteImageUrl.toString());
    QCOMPARE(droppedObject.value(QStringLiteral("x")).toReal(), 210.0 - sourceImage.width() / 2.0);
    QCOMPARE(droppedObject.value(QStringLiteral("y")).toReal(), 160.0 - sourceImage.height() / 2.0);
    QCOMPARE(rootItem->property("selectedDrawableObjectId").toInt(),
             droppedObject.value(QStringLiteral("id")).toInt());

    QTemporaryDir nativeDropDirectory;
    QVERIFY(nativeDropDirectory.isValid());
    QImage nativeDropImage(44, 28, QImage::Format_ARGB32_Premultiplied);
    nativeDropImage.fill(QColor(QStringLiteral("#f4511e")));
    const QString nativeDropPath =
        nativeDropDirectory.filePath(QStringLiteral("native-finder-drop.png"));
    QVERIFY(nativeDropImage.save(nativeDropPath, "PNG"));

    QMimeData nativeDropMimeData;
    nativeDropMimeData.setUrls({QUrl::fromLocalFile(nativeDropPath)});
    const QPointF canvasDropPoint = qmlCanvasItem->mapToScene(
        QPointF(qmlCanvasItem->width() * 0.7, qmlCanvasItem->height() * 0.6));
    QDragEnterEvent dragEnterEvent(canvasDropPoint.toPoint(), Qt::CopyAction, &nativeDropMimeData,
                                   Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(&dropWindow, &dragEnterEvent);
    QVERIFY(dragEnterEvent.isAccepted());

    QDropEvent dropEvent(canvasDropPoint, Qt::CopyAction, &nativeDropMimeData, Qt::LeftButton,
                         Qt::NoModifier);
    QCoreApplication::sendEvent(&dropWindow, &dropEvent);
    QVERIFY(dropEvent.isAccepted());
    QTRY_COMPARE(insertedSpy.count(), 2);
    QCOMPARE(insertionFailedSpy.count(), 0);

    const QVariantList objectsAfterNativeDrop = rootItem->property("drawableObjects").toList();
    QCOMPARE(objectsAfterNativeDrop.size(), 3);
    const QVariantMap nativeDroppedObject = objectsAfterNativeDrop.constLast().toMap();
    QCOMPARE(nativeDroppedObject.value(QStringLiteral("name")).toString(),
             QStringLiteral("native-finder-drop.png"));
    QCOMPARE(nativeDroppedObject.value(QStringLiteral("originalSource")).toString(),
             QUrl::fromLocalFile(nativeDropPath).toString());

    QMimeData nativeWebDropMimeData;
    nativeWebDropMimeData.setHtml(webImageDrop.m_html);
    const QPointF webCanvasDropPoint = qmlCanvasItem->mapToScene(
        QPointF(qmlCanvasItem->width() * 0.35, qmlCanvasItem->height() * 0.4));
    QDragEnterEvent webDragEnterEvent(webCanvasDropPoint.toPoint(), Qt::CopyAction,
                                      &nativeWebDropMimeData, Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(&dropWindow, &webDragEnterEvent);
    QVERIFY(webDragEnterEvent.isAccepted());

    QDropEvent webDropEvent(webCanvasDropPoint, Qt::CopyAction, &nativeWebDropMimeData,
                            Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(&dropWindow, &webDropEvent);
    QVERIFY(webDropEvent.isAccepted());
    QTRY_COMPARE_WITH_TIMEOUT(insertedSpy.count(), 3, 5000);
    QCOMPARE(insertionFailedSpy.count(), 0);

    const QVariantList objectsAfterWebDrop = rootItem->property("drawableObjects").toList();
    QCOMPARE(objectsAfterWebDrop.size(), 4);
    const QVariantMap webDroppedObject = objectsAfterWebDrop.constLast().toMap();
    QCOMPARE(webDroppedObject.value(QStringLiteral("name")).toString(),
             QStringLiteral("site-image.png"));
    QCOMPARE(webDroppedObject.value(QStringLiteral("originalSource")).toString(),
             storedRemoteImageUrl.toString());

    rootItem->setParentItem(nullptr);
    dropWindow.hide();
}

void tst_DrawingSurfaceItem::movesAndResizesDrawableObjects()
{
    qmlRegisterType<DrawingSurfaceItem>("Congregation", 1, 0, "DrawingSurfaceItem");

    QQmlEngine engine;

    QQmlComponent component(&engine);
    const QString drawingSurfaceQml = QFINDTESTDATA("../App/qml/painting/DrawingSurface.qml");
    QVERIFY2(!drawingSurfaceQml.isEmpty(), "DrawingSurface.qml test data was not found");
    component.loadUrl(QUrl::fromLocalFile(drawingSurfaceQml));
    QTRY_VERIFY(component.isReady() || component.isError());
    QVERIFY2(component.isReady(), qPrintable(qmlErrorsToString(component.errors())));

    PaletteUtils paletteUtils;
    CanvasDocumentViewModel viewModel(&paletteUtils);

    QVariantMap initialProperties;
    initialProperties.insert(QStringLiteral("width"), 500);
    initialProperties.insert(QStringLiteral("height"), 360);
    initialProperties.insert(QStringLiteral("documentViewModel"),
                             QVariant::fromValue(static_cast<QObject*>(&viewModel)));
    initialProperties.insert(QStringLiteral("toolMode"), QStringLiteral("move"));

    QScopedPointer<QObject> object(component.createWithInitialProperties(initialProperties));
    QVERIFY2(!object.isNull(), qPrintable(qmlErrorsToString(component.errors())));
    auto* rootItem = qobject_cast<QQuickItem*>(object.data());
    QVERIFY(rootItem);
    DrawingSurfaceItem* canvasItem = findDrawingSurfaceItem(rootItem);
    QVERIFY(canvasItem);
    QCOMPARE(canvasItem->objectName(), QStringLiteral("canvasSurface"));
    QVERIFY(canvasItem->clip());
    QTRY_VERIFY(canvasItem->width() > 100);
    QTRY_VERIFY(canvasItem->height() > 100);
    engine.rootContext()->setContextProperty(QStringLiteral("testCanvasWidth"),
                                             canvasItem->width());
    engine.rootContext()->setContextProperty(QStringLiteral("testCanvasHeight"),
                                             canvasItem->height());

    QQmlExpression handleTargetContract(
        engine.rootContext(), object.data(),
        QStringLiteral(
            "[drawableObjectHandleSize, drawableObjectHandleHitSize, drawableObjectHandles.length, "
            "drawableObjectResizeCursor(\"resize-n\") === Qt.SizeVerCursor, "
            "drawableObjectResizeCursor(\"resize-e\") === Qt.SizeHorCursor].join(\",\");"));
    const QVariant handleTargetContractResult = handleTargetContract.evaluate();
    QVERIFY2(!handleTargetContract.hasError(), qPrintable(handleTargetContract.error().toString()));
    QCOMPARE(handleTargetContractResult.toString(), QStringLiteral("10,32,8,true,true"));

    QQmlExpression moveObject(
        engine.rootContext(), object.data(),
        QStringLiteral("appendDrawableObject({ id: 2, type: \"shape\", x: 10, y: 20, width: 30, "
                       "height: 28, shapeKind: \"rectangle\", color: \"#1976d2\" });"
                       "beginDrawableObjectTransform(20, 30); updateDrawableObjectTransform(40, "
                       "60); commitDrawableObjectTransform();"));
    moveObject.evaluate();
    QVERIFY2(!moveObject.hasError(), qPrintable(moveObject.error().toString()));

    QVariantList objects = rootItem->property("drawableObjects").toList();
    QCOMPARE(objects.size(), 2);
    QVariantMap movedObject = objects.last().toMap();
    QCOMPARE(movedObject.value(QStringLiteral("x")).toReal(), 30.0);
    QCOMPARE(movedObject.value(QStringLiteral("y")).toReal(), 50.0);
    QCOMPARE(movedObject.value(QStringLiteral("width")).toReal(), 30.0);
    QCOMPARE(movedObject.value(QStringLiteral("height")).toReal(), 28.0);

    QQmlExpression resizeObject(
        engine.rootContext(), object.data(),
        QStringLiteral("beginDrawableObjectTransform(60, 78); updateDrawableObjectTransform(90, "
                       "100); commitDrawableObjectTransform();"));
    resizeObject.evaluate();
    QVERIFY2(!resizeObject.hasError(), qPrintable(resizeObject.error().toString()));

    objects = rootItem->property("drawableObjects").toList();
    QCOMPARE(objects.size(), 2);
    const QVariantMap resizedObject = objects.last().toMap();
    QCOMPARE(resizedObject.value(QStringLiteral("x")).toReal(), 30.0);
    QCOMPARE(resizedObject.value(QStringLiteral("y")).toReal(), 50.0);
    QCOMPARE(resizedObject.value(QStringLiteral("width")).toReal(), 60.0);
    QCOMPARE(resizedObject.value(QStringLiteral("height")).toReal(), 50.0);

    QQmlExpression moveOutsideCanvas(
        engine.rootContext(), object.data(),
        QStringLiteral("beginDrawableObjectTransform(40, 60); updateDrawableObjectTransform(-90, "
                       "-80); commitDrawableObjectTransform();"));
    moveOutsideCanvas.evaluate();
    QVERIFY2(!moveOutsideCanvas.hasError(), qPrintable(moveOutsideCanvas.error().toString()));

    objects = rootItem->property("drawableObjects").toList();
    QCOMPARE(objects.size(), 2);
    const QVariantMap movedOutsideObject = objects.last().toMap();
    QCOMPARE(movedOutsideObject.value(QStringLiteral("x")).toReal(), -100.0);
    QCOMPARE(movedOutsideObject.value(QStringLiteral("y")).toReal(), -90.0);
    QCOMPARE(movedOutsideObject.value(QStringLiteral("width")).toReal(), 60.0);
    QCOMPARE(movedOutsideObject.value(QStringLiteral("height")).toReal(), 50.0);

    QQmlExpression resizeOutsideCanvas(
        engine.rootContext(), object.data(),
        QStringLiteral("beginDrawableObjectTransform(-40, -40);"
                       "updateDrawableObjectTransform(testCanvasWidth + 80, testCanvasHeight + 90);"
                       "commitDrawableObjectTransform();"));
    resizeOutsideCanvas.evaluate();
    QVERIFY2(!resizeOutsideCanvas.hasError(), qPrintable(resizeOutsideCanvas.error().toString()));

    objects = rootItem->property("drawableObjects").toList();
    QCOMPARE(objects.size(), 2);
    const QVariantMap resizedOutsideObject = objects.last().toMap();
    QCOMPARE(resizedOutsideObject.value(QStringLiteral("x")).toReal(), -100.0);
    QCOMPARE(resizedOutsideObject.value(QStringLiteral("y")).toReal(), -90.0);
    QVERIFY(resizedOutsideObject.value(QStringLiteral("x")).toReal() +
                resizedOutsideObject.value(QStringLiteral("width")).toReal() >
            canvasItem->width());
    QVERIFY(resizedOutsideObject.value(QStringLiteral("y")).toReal() +
                resizedOutsideObject.value(QStringLiteral("height")).toReal() >
            canvasItem->height());

    QQmlExpression forgivingHandleHitTargets(
        engine.rootContext(), object.data(),
        QStringLiteral("var objectX = testCanvasWidth + 200;"
                       "var objectY = testCanvasHeight + 200;"
                       "appendDrawableObject({ id: 3, type: \"shape\", x: objectX, y: objectY, "
                       "width: 40, height: 30, shapeKind: \"rectangle\", color: \"#ef6c00\" });"
                       "var cornerMode = drawableObjectHandleAt(objectX + 55, objectY + 45);"
                       "beginDrawableObjectTransform(objectX + 55, objectY + 45);"
                       "updateDrawableObjectTransform(objectX + 75, objectY + 65);"
                       "commitDrawableObjectTransform();"
                       "var afterCorner = selectedDrawableObject();"
                       "var edgeMode = drawableObjectHandleAt(objectX + 30, objectY - 15);"
                       "beginDrawableObjectTransform(objectX + 30, objectY - 15);"
                       "updateDrawableObjectTransform(objectX + 30, objectY - 25);"
                       "commitDrawableObjectTransform();"
                       "var afterEdge = selectedDrawableObject();"
                       "[cornerMode, edgeMode, afterCorner.width, afterCorner.height, afterEdge.y "
                       "=== objectY - 10, afterEdge.height].join(\",\");"));
    const QVariant forgivingHandleHitTargetsResult = forgivingHandleHitTargets.evaluate();
    QVERIFY2(!forgivingHandleHitTargets.hasError(),
             qPrintable(forgivingHandleHitTargets.error().toString()));
    QCOMPARE(forgivingHandleHitTargetsResult.toString(),
             QStringLiteral("resize-se,resize-n,60,50,true,60"));
}

void tst_DrawingSurfaceItem::constrainsDrawableObjectTransformWithShiftModifier()
{
    qmlRegisterType<DrawingSurfaceItem>("Congregation", 1, 0, "DrawingSurfaceItem");

    QQmlEngine engine;

    QQmlComponent component(&engine);
    const QString drawingSurfaceQml = QFINDTESTDATA("../App/qml/painting/DrawingSurface.qml");
    QVERIFY2(!drawingSurfaceQml.isEmpty(), "DrawingSurface.qml test data was not found");
    component.loadUrl(QUrl::fromLocalFile(drawingSurfaceQml));
    QTRY_VERIFY(component.isReady() || component.isError());
    QVERIFY2(component.isReady(), qPrintable(qmlErrorsToString(component.errors())));

    PaletteUtils paletteUtils;
    CanvasDocumentViewModel viewModel(&paletteUtils);

    QVariantMap initialProperties;
    initialProperties.insert(QStringLiteral("width"), 500);
    initialProperties.insert(QStringLiteral("height"), 360);
    initialProperties.insert(QStringLiteral("documentViewModel"),
                             QVariant::fromValue(static_cast<QObject*>(&viewModel)));
    initialProperties.insert(QStringLiteral("toolMode"), QStringLiteral("move"));

    QScopedPointer<QObject> object(component.createWithInitialProperties(initialProperties));
    QVERIFY2(!object.isNull(), qPrintable(qmlErrorsToString(component.errors())));
    auto* rootItem = qobject_cast<QQuickItem*>(object.data());
    QVERIFY(rootItem);

    QQmlExpression constrainedMove(
        engine.rootContext(), object.data(),
        QStringLiteral("appendDrawableObject({ id: 2, type: \"shape\", x: 10, y: 20, width: 40, "
                       "height: 20, shapeKind: \"rectangle\", color: \"#1976d2\" });"
                       "beginDrawableObjectTransform(20, 30);"
                       "updateDrawableObjectTransform(80, 50, true);"
                       "commitDrawableObjectTransform();"
                       "beginDrawableObjectTransform(80, 30);"
                       "updateDrawableObjectTransform(90, 120, true);"
                       "commitDrawableObjectTransform();"));
    constrainedMove.evaluate();
    QVERIFY2(!constrainedMove.hasError(), qPrintable(constrainedMove.error().toString()));

    QVariantList objects = rootItem->property("drawableObjects").toList();
    QCOMPARE(objects.size(), 2);
    QVariantMap transformedObject = objects.last().toMap();
    QCOMPARE(transformedObject.value(QStringLiteral("x")).toReal(), 70.0);
    QCOMPARE(transformedObject.value(QStringLiteral("y")).toReal(), 110.0);
    QCOMPARE(transformedObject.value(QStringLiteral("width")).toReal(), 40.0);
    QCOMPARE(transformedObject.value(QStringLiteral("height")).toReal(), 20.0);

    QQmlExpression constrainedCornerResize(
        engine.rootContext(), object.data(),
        QStringLiteral("beginDrawableObjectTransform(110, 130);"
                       "updateDrawableObjectTransform(170, 150, true);"
                       "commitDrawableObjectTransform();"));
    constrainedCornerResize.evaluate();
    QVERIFY2(!constrainedCornerResize.hasError(),
             qPrintable(constrainedCornerResize.error().toString()));

    objects = rootItem->property("drawableObjects").toList();
    transformedObject = objects.last().toMap();
    QCOMPARE(transformedObject.value(QStringLiteral("x")).toReal(), 70.0);
    QCOMPARE(transformedObject.value(QStringLiteral("y")).toReal(), 110.0);
    QCOMPARE(transformedObject.value(QStringLiteral("width")).toReal(), 100.0);
    QCOMPARE(transformedObject.value(QStringLiteral("height")).toReal(), 50.0);

    QQmlExpression constrainedEdgeResize(
        engine.rootContext(), object.data(),
        QStringLiteral("beginDrawableObjectTransform(170, 135);"
                       "updateDrawableObjectTransform(220, 135, true);"
                       "commitDrawableObjectTransform();"));
    constrainedEdgeResize.evaluate();
    QVERIFY2(!constrainedEdgeResize.hasError(),
             qPrintable(constrainedEdgeResize.error().toString()));

    objects = rootItem->property("drawableObjects").toList();
    transformedObject = objects.last().toMap();
    QCOMPARE(transformedObject.value(QStringLiteral("x")).toReal(), 70.0);
    QCOMPARE(transformedObject.value(QStringLiteral("y")).toReal(), 97.5);
    QCOMPARE(transformedObject.value(QStringLiteral("width")).toReal(), 150.0);
    QCOMPARE(transformedObject.value(QStringLiteral("height")).toReal(), 75.0);
}

void tst_DrawingSurfaceItem::deletesSelectedDrawableObject()
{
    qmlRegisterType<DrawingSurfaceItem>("Congregation", 1, 0, "DrawingSurfaceItem");

    QQmlEngine engine;

    QQmlComponent component(&engine);
    const QString drawingSurfaceQml = QFINDTESTDATA("../App/qml/painting/DrawingSurface.qml");
    QVERIFY2(!drawingSurfaceQml.isEmpty(), "DrawingSurface.qml test data was not found");
    component.loadUrl(QUrl::fromLocalFile(drawingSurfaceQml));
    QTRY_VERIFY(component.isReady() || component.isError());
    QVERIFY2(component.isReady(), qPrintable(qmlErrorsToString(component.errors())));

    PaletteUtils paletteUtils;
    CanvasDocumentViewModel viewModel(&paletteUtils);

    QVariantMap initialProperties;
    initialProperties.insert(QStringLiteral("width"), 500);
    initialProperties.insert(QStringLiteral("height"), 360);
    initialProperties.insert(QStringLiteral("documentViewModel"),
                             QVariant::fromValue(static_cast<QObject*>(&viewModel)));
    initialProperties.insert(QStringLiteral("toolMode"), QStringLiteral("move"));

    QScopedPointer<QObject> object(component.createWithInitialProperties(initialProperties));
    QVERIFY2(!object.isNull(), qPrintable(qmlErrorsToString(component.errors())));
    auto* rootItem = qobject_cast<QQuickItem*>(object.data());
    QVERIFY(rootItem);

    QQmlExpression deleteObject(
        engine.rootContext(), object.data(),
        QStringLiteral("appendDrawableObject({ id: 2, type: \"shape\", x: 10, y: 20, width: 30, "
                       "height: 28, shapeKind: \"rectangle\", color: \"#1976d2\" });"
                       "appendDrawableObject({ id: 3, type: \"text\", x: 40, y: 50, width: 120, "
                       "height: 32, text: \"Label\", fontPixelSize: 18, color: \"#111111\" });"
                       "deleteSelectedDrawableObject();"));
    const QVariant deleteResult = deleteObject.evaluate();
    QVERIFY2(!deleteObject.hasError(), qPrintable(deleteObject.error().toString()));
    QCOMPARE(deleteResult.toBool(), true);

    QVariantList objects = rootItem->property("drawableObjects").toList();
    QCOMPARE(objects.size(), 2);
    const QVariantMap remainingObject = objects.last().toMap();
    QCOMPARE(remainingObject.value(QStringLiteral("id")).toInt(), 2);
    QCOMPARE(rootItem->property("selectedDrawableObjectId").toInt(), -1);

    QQmlExpression deleteWithoutSelection(engine.rootContext(), object.data(),
                                          QStringLiteral("deleteSelectedDrawableObject();"));
    const QVariant deleteWithoutSelectionResult = deleteWithoutSelection.evaluate();
    QVERIFY2(!deleteWithoutSelection.hasError(),
             qPrintable(deleteWithoutSelection.error().toString()));
    QCOMPARE(deleteWithoutSelectionResult.toBool(), false);

    objects = rootItem->property("drawableObjects").toList();
    QCOMPARE(objects.size(), 2);
}

void tst_DrawingSurfaceItem::deletesBackgroundLayerLikeRegularLayer()
{
    qmlRegisterType<DrawingSurfaceItem>("Congregation", 1, 0, "DrawingSurfaceItem");

    QQmlEngine engine;

    QQmlComponent component(&engine);
    const QString drawingSurfaceQml = QFINDTESTDATA("../App/qml/painting/DrawingSurface.qml");
    QVERIFY2(!drawingSurfaceQml.isEmpty(), "DrawingSurface.qml test data was not found");
    component.loadUrl(QUrl::fromLocalFile(drawingSurfaceQml));
    QTRY_VERIFY(component.isReady() || component.isError());
    QVERIFY2(component.isReady(), qPrintable(qmlErrorsToString(component.errors())));

    PaletteUtils paletteUtils;
    CanvasDocumentViewModel viewModel(&paletteUtils);

    QVariantMap initialProperties;
    initialProperties.insert(QStringLiteral("width"), 320);
    initialProperties.insert(QStringLiteral("height"), 240);
    initialProperties.insert(QStringLiteral("documentViewModel"),
                             QVariant::fromValue(static_cast<QObject*>(&viewModel)));

    QScopedPointer<QObject> object(component.createWithInitialProperties(initialProperties));
    QVERIFY2(!object.isNull(), qPrintable(qmlErrorsToString(component.errors())));
    auto* rootItem = qobject_cast<QQuickItem*>(object.data());
    QVERIFY(rootItem);

    QQuickItem* canvasPaper = findItemByObjectName(rootItem, QStringLiteral("canvasPaper"));
    QVERIFY(canvasPaper);
    QCOMPARE(canvasPaper->property("color").value<QColor>(), QColor(Qt::white));
    QQuickItem* transparencyGridBackground =
        findItemByObjectName(rootItem, QStringLiteral("transparencyGridBackground"));
    QVERIFY(transparencyGridBackground);
    QCOMPARE(transparencyGridBackground->property("visible").toBool(), false);
    QCOMPARE(transparencyGridBackground->property("source").toUrl().scheme(),
             QStringLiteral("data"));

    QVariantList rows = rootItem->property("layerHierarchyRows").toList();
    QCOMPARE(rows.size(), 2);
    QCOMPARE(rows.at(0).toMap().value(QStringLiteral("key")).toString(),
             QStringLiteral("object-1"));
    QCOMPARE(rows.at(0).toMap().value(QStringLiteral("label")).toString(),
             QStringLiteral("Layer 1"));
    QCOMPARE(rows.at(0).toMap().value(QStringLiteral("selected")).toBool(), true);
    QCOMPARE(rows.at(1).toMap().value(QStringLiteral("key")).toString(),
             QStringLiteral("raster-canvas"));
    QCOMPARE(rows.at(1).toMap().value(QStringLiteral("selected")).toBool(), false);
    QTRY_VERIFY(
        [&]()
        {
            QQmlExpression layerReady(engine.rootContext(), object.data(),
                                      QStringLiteral("rasterLayerItemById(1) !== null"));
            const QVariant result = layerReady.evaluate();
            return !layerReady.hasError() && result.toBool();
        }());

    QQmlExpression canDeleteBackground(engine.rootContext(), object.data(),
                                       QStringLiteral("canDeleteCurrentLayer();"));
    const QVariant canDeleteResult = canDeleteBackground.evaluate();
    QVERIFY2(!canDeleteBackground.hasError(), qPrintable(canDeleteBackground.error().toString()));
    QCOMPARE(canDeleteResult.toBool(), true);

    QQmlExpression deleteBackground(engine.rootContext(), object.data(),
                                    QStringLiteral("deleteLayerByKey(\"raster-canvas\");"));
    const QVariant deleteResult = deleteBackground.evaluate();
    QVERIFY2(!deleteBackground.hasError(), qPrintable(deleteBackground.error().toString()));
    QCOMPARE(deleteResult.toBool(), true);
    QCOMPARE(rootItem->property("backgroundLayerPresent").toBool(), false);
    QCOMPARE(canvasPaper->property("color").value<QColor>(), QColor(Qt::transparent));
    QCOMPARE(transparencyGridBackground->property("visible").toBool(), true);

    rows = rootItem->property("layerHierarchyRows").toList();
    QCOMPARE(rows.size(), 1);
    QCOMPARE(rows.first().toMap().value(QStringLiteral("key")).toString(),
             QStringLiteral("object-1"));
    QCOMPARE(rows.first().toMap().value(QStringLiteral("selected")).toBool(), true);

    QQmlExpression currentLayerKey(engine.rootContext(), object.data(),
                                   QStringLiteral("currentLayerKey();"));
    const QVariant currentLayerKeyResult = currentLayerKey.evaluate();
    QVERIFY2(!currentLayerKey.hasError(), qPrintable(currentLayerKey.error().toString()));
    QCOMPARE(currentLayerKeyResult.toString(), QStringLiteral("object-1"));

    QQmlExpression canDeleteDefaultLayer(engine.rootContext(), object.data(),
                                         QStringLiteral("canDeleteCurrentLayer();"));
    const QVariant canDeleteDefaultLayerResult = canDeleteDefaultLayer.evaluate();
    QVERIFY2(!canDeleteDefaultLayer.hasError(),
             qPrintable(canDeleteDefaultLayer.error().toString()));
    QCOMPARE(canDeleteDefaultLayerResult.toBool(), true);

    QQmlExpression hasActiveRasterSurface(engine.rootContext(), object.data(),
                                          QStringLiteral("hasActiveRasterSurface();"));
    const QVariant hasActiveRasterResult = hasActiveRasterSurface.evaluate();
    QVERIFY2(!hasActiveRasterSurface.hasError(),
             qPrintable(hasActiveRasterSurface.error().toString()));
    QCOMPARE(hasActiveRasterResult.toBool(), true);

    QQmlExpression activeRasterSurface(engine.rootContext(), object.data(),
                                       QStringLiteral("activeRasterSurface();"));
    const QVariant activeRasterResult = activeRasterSurface.evaluate();
    QVERIFY2(!activeRasterSurface.hasError(), qPrintable(activeRasterSurface.error().toString()));
    QVERIFY(!activeRasterResult.isNull());
    QVERIFY(activeRasterResult.value<QObject*>() != nullptr);

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString psdPath = dir.filePath(QStringLiteral("without-background.psd"));
    QQmlExpression savePsd(
        engine.rootContext(), object.data(),
        QStringLiteral("saveToFile(\"%1\");").arg(QUrl::fromLocalFile(psdPath).toString()));
    const QVariant saveResult = savePsd.evaluate();
    QVERIFY2(!savePsd.hasError(), qPrintable(savePsd.error().toString()));
    QCOMPARE(saveResult.toBool(), true);

    QFile psdFile(psdPath);
    QVERIFY(psdFile.open(QIODevice::ReadOnly));
    QCOMPARE(psdLayerRecordNames(psdFile.readAll()), QStringList({QStringLiteral("Layer 1")}));

    const PsdImportedDocument importedDocument = PsdImageReader::readDocument(psdPath);
    QVERIFY(importedDocument.isValid());
    QCOMPARE(importedDocument.layers.size(), 1);
    QCOMPARE(importedDocument.layers.first().name, QStringLiteral("Layer 1"));
    const QVariantList manifestLayers =
        importedDocument.congregationManifest.value(QStringLiteral("layers")).toList();
    QCOMPARE(manifestLayers.size(), 1);
    QCOMPARE(manifestLayers.first().toMap().value(QStringLiteral("name")).toString(),
             QStringLiteral("Layer 1"));
}

void tst_DrawingSurfaceItem::addsBlankLayerRowsWithoutTransformHitTesting()
{
    qmlRegisterType<DrawingSurfaceItem>("Congregation", 1, 0, "DrawingSurfaceItem");

    QQmlEngine engine;

    QQmlComponent component(&engine);
    const QString drawingSurfaceQml = QFINDTESTDATA("../App/qml/painting/DrawingSurface.qml");
    QVERIFY2(!drawingSurfaceQml.isEmpty(), "DrawingSurface.qml test data was not found");
    component.loadUrl(QUrl::fromLocalFile(drawingSurfaceQml));
    QTRY_VERIFY(component.isReady() || component.isError());
    QVERIFY2(component.isReady(), qPrintable(qmlErrorsToString(component.errors())));

    PaletteUtils paletteUtils;
    CanvasDocumentViewModel viewModel(&paletteUtils);

    QVariantMap initialProperties;
    initialProperties.insert(QStringLiteral("width"), 500);
    initialProperties.insert(QStringLiteral("height"), 360);
    initialProperties.insert(QStringLiteral("documentViewModel"),
                             QVariant::fromValue(static_cast<QObject*>(&viewModel)));
    initialProperties.insert(QStringLiteral("toolMode"), QStringLiteral("move"));

    QScopedPointer<QObject> object(component.createWithInitialProperties(initialProperties));
    QVERIFY2(!object.isNull(), qPrintable(qmlErrorsToString(component.errors())));
    auto* rootItem = qobject_cast<QQuickItem*>(object.data());
    QVERIFY(rootItem);

    DrawingSurfaceItem* canvasItem = findDrawingSurfaceItem(rootItem);
    QVERIFY(canvasItem);
    QTRY_VERIFY(canvasItem->width() > 1);
    QTRY_VERIFY(canvasItem->height() > 1);

    QQmlExpression addLayer(engine.rootContext(), object.data(),
                            QStringLiteral("addEmptyLayer();"));
    const QVariant addedLayerId = addLayer.evaluate();
    QVERIFY2(!addLayer.hasError(), qPrintable(addLayer.error().toString()));
    QCOMPARE(addedLayerId.toInt(), 2);
    QCOMPARE(rootItem->property("selectedDrawableObjectId").toInt(), 2);

    QVariantList objects = rootItem->property("drawableObjects").toList();
    QCOMPARE(objects.size(), 2);
    const QVariantMap layerObject = objects.last().toMap();
    QCOMPARE(layerObject.value(QStringLiteral("type")).toString(), QStringLiteral("layer"));
    QCOMPARE(layerObject.value(QStringLiteral("name")).toString(), QStringLiteral("Layer 2"));
    QCOMPARE(layerObject.value(QStringLiteral("x")).toReal(), 0.0);
    QCOMPARE(layerObject.value(QStringLiteral("y")).toReal(), 0.0);
    QCOMPARE(layerObject.value(QStringLiteral("width")).toReal(), canvasItem->width());
    QCOMPARE(layerObject.value(QStringLiteral("height")).toReal(), canvasItem->height());

    QVariantList rows;
    QTRY_VERIFY(
        [&]()
        {
            rows = rootItem->property("layerHierarchyRows").toList();
            return rows.size() == 3 &&
                   !rows.at(0).toMap().value(QStringLiteral("iconSource")).toString().isEmpty();
        }());
    QCOMPARE(rows.at(0).toMap().value(QStringLiteral("key")).toString(),
             QStringLiteral("object-2"));
    QCOMPARE(rows.at(0).toMap().value(QStringLiteral("label")).toString(),
             QStringLiteral("Layer 2"));
    QCOMPARE(rows.at(0).toMap().value(QStringLiteral("layerKind")).toString(),
             QStringLiteral("layer"));
    QCOMPARE(rows.at(0).toMap().value(QStringLiteral("contentKind")).toString(),
             QStringLiteral("layer"));
    QCOMPARE(rows.at(0).toMap().value(QStringLiteral("iconGlyph")).toString(), QString());
    const QString layerThumbnailSource =
        rows.at(0).toMap().value(QStringLiteral("iconSource")).toString();
    const QString layerThumbnailPath = QUrl(layerThumbnailSource).toLocalFile();
    if (!layerThumbnailPath.isEmpty())
    {
        QVERIFY(QFileInfo::exists(layerThumbnailPath));
        QCOMPARE(QImage(layerThumbnailPath).size(), QSize(32, 32));
    }
    QCOMPARE(rows.at(0).toMap().value(QStringLiteral("selected")).toBool(), true);
    QCOMPARE(rows.at(1).toMap().value(QStringLiteral("key")).toString(),
             QStringLiteral("object-1"));
    QCOMPARE(rows.at(1).toMap().value(QStringLiteral("label")).toString(),
             QStringLiteral("Layer 1"));
    QCOMPARE(rows.at(1).toMap().value(QStringLiteral("layerKind")).toString(),
             QStringLiteral("layer"));
    QCOMPARE(rows.at(1).toMap().value(QStringLiteral("contentKind")).toString(),
             QStringLiteral("layer"));
    QCOMPARE(rows.at(2).toMap().value(QStringLiteral("key")).toString(),
             QStringLiteral("raster-canvas"));
    QCOMPARE(rows.at(2).toMap().value(QStringLiteral("iconGlyph")).toString(), QString());
    QVERIFY(!rows.at(2).toMap().value(QStringLiteral("iconSource")).toString().isEmpty());

    QQmlExpression coalesceThumbnailRefresh(
        engine.rootContext(), object.data(),
        QStringLiteral("requestRasterLayerThumbnailRefresh(2);"
                       "requestRasterLayerThumbnailRefresh(2);"
                       "Object.keys(pendingRasterLayerThumbnailRefreshes).length;"));
    const QVariant pendingRefreshCount = coalesceThumbnailRefresh.evaluate();
    QVERIFY2(!coalesceThumbnailRefresh.hasError(),
             qPrintable(coalesceThumbnailRefresh.error().toString()));
    QCOMPARE(pendingRefreshCount.toInt(), 1);

    QQmlExpression flushThumbnailRefresh(
        engine.rootContext(), object.data(),
        QStringLiteral("flushPendingLayerThumbnailRefreshes();"
                       "Object.keys(pendingRasterLayerThumbnailRefreshes).length;"));
    const QVariant remainingRefreshCount = flushThumbnailRefresh.evaluate();
    QVERIFY2(!flushThumbnailRefresh.hasError(),
             qPrintable(flushThumbnailRefresh.error().toString()));
    QCOMPARE(remainingRefreshCount.toInt(), 0);

    QQmlExpression clearThumbnailState(
        engine.rootContext(), object.data(),
        QStringLiteral("setRasterLayerThumbnailSource(2, \"image://old-thumbnail\");"
                       "requestRasterLayerThumbnailRefresh(2);"
                       "clearRasterLayerThumbnailState(2);"
                       "[Object.keys(pendingRasterLayerThumbnailRefreshes).length,"
                       " rasterLayerThumbnailSources[\"2\"] === undefined].join(\",\");"));
    const QVariant clearedThumbnailState = clearThumbnailState.evaluate();
    QVERIFY2(!clearThumbnailState.hasError(), qPrintable(clearThumbnailState.error().toString()));
    QCOMPARE(clearedThumbnailState.toString(), QStringLiteral("0,true"));

    QQmlExpression transformableSelection(
        engine.rootContext(), object.data(),
        QStringLiteral("hasTransformableSelectedDrawableObject();"));
    const QVariant transformableSelectionResult = transformableSelection.evaluate();
    QVERIFY2(!transformableSelection.hasError(),
             qPrintable(transformableSelection.error().toString()));
    QCOMPARE(transformableSelectionResult.toBool(), false);

    QQmlExpression beginBlankLayerTransform(
        engine.rootContext(), object.data(),
        QStringLiteral("beginDrawableObjectTransform(10, 10);"));
    const QVariant beginBlankLayerTransformResult = beginBlankLayerTransform.evaluate();
    QVERIFY2(!beginBlankLayerTransform.hasError(),
             qPrintable(beginBlankLayerTransform.error().toString()));
    QCOMPARE(beginBlankLayerTransformResult.toBool(), false);
    QCOMPARE(rootItem->property("selectedDrawableObjectId").toInt(), -1);

    QQmlExpression deleteLayer(engine.rootContext(), object.data(),
                               QStringLiteral("deleteLayerByKey(\"object-2\");"));
    const QVariant deleteResult = deleteLayer.evaluate();
    QVERIFY2(!deleteLayer.hasError(), qPrintable(deleteLayer.error().toString()));
    QCOMPARE(deleteResult.toBool(), true);
    objects = rootItem->property("drawableObjects").toList();
    QCOMPARE(objects.size(), 1);
    QCOMPARE(objects.first().toMap().value(QStringLiteral("name")).toString(),
             QStringLiteral("Layer 1"));
}

void tst_DrawingSurfaceItem::shapeAndTextToolsCreateSeparateLayerRows()
{
    qmlRegisterType<DrawingSurfaceItem>("Congregation", 1, 0, "DrawingSurfaceItem");

    QQmlEngine engine;

    QQmlComponent component(&engine);
    const QString drawingSurfaceQml = QFINDTESTDATA("../App/qml/painting/DrawingSurface.qml");
    QVERIFY2(!drawingSurfaceQml.isEmpty(), "DrawingSurface.qml test data was not found");
    component.loadUrl(QUrl::fromLocalFile(drawingSurfaceQml));
    QTRY_VERIFY(component.isReady() || component.isError());
    QVERIFY2(component.isReady(), qPrintable(qmlErrorsToString(component.errors())));

    PaletteUtils paletteUtils;
    CanvasDocumentViewModel viewModel(&paletteUtils);

    QVariantMap initialProperties;
    initialProperties.insert(QStringLiteral("width"), 500);
    initialProperties.insert(QStringLiteral("height"), 360);
    initialProperties.insert(QStringLiteral("documentViewModel"),
                             QVariant::fromValue(static_cast<QObject*>(&viewModel)));

    QScopedPointer<QObject> object(component.createWithInitialProperties(initialProperties));
    QVERIFY2(!object.isNull(), qPrintable(qmlErrorsToString(component.errors())));
    auto* rootItem = qobject_cast<QQuickItem*>(object.data());
    QVERIFY(rootItem);

    QQmlExpression createObjects(engine.rootContext(), object.data(),
                                 QStringLiteral("toolMode = \"shape\";"
                                                "shapeKind = \"ellipse\";"
                                                "beginShapeDrag(24, 28, false);"
                                                "updateShapeDrag(84, 76, false);"
                                                "commitActiveShape();"
                                                "toolMode = \"text\";"
                                                "beginTextPlacement(96, 44);"));
    createObjects.evaluate();
    QVERIFY2(!createObjects.hasError(), qPrintable(createObjects.error().toString()));

    QQuickItem* textToolEditor = findItemByObjectName(rootItem, QStringLiteral("textToolEditor"));
    QVERIFY(textToolEditor);
    textToolEditor->setProperty("text", QStringLiteral("Caption"));

    QQmlExpression commitText(engine.rootContext(), object.data(),
                              QStringLiteral("commitActiveText();"));
    commitText.evaluate();
    QVERIFY2(!commitText.hasError(), qPrintable(commitText.error().toString()));

    QVariantList objects = rootItem->property("drawableObjects").toList();
    QCOMPARE(objects.size(), 3);
    QCOMPARE(objects.at(0).toMap().value(QStringLiteral("type")).toString(),
             QStringLiteral("layer"));
    QCOMPARE(objects.at(1).toMap().value(QStringLiteral("type")).toString(),
             QStringLiteral("shape"));
    QCOMPARE(objects.at(2).toMap().value(QStringLiteral("type")).toString(),
             QStringLiteral("text"));

    QVariantList rows = rootItem->property("layerHierarchyRows").toList();
    QCOMPARE(rows.size(), 4);
    QCOMPARE(rows.at(0).toMap().value(QStringLiteral("label")).toString(),
             QStringLiteral("Caption"));
    QCOMPARE(rows.at(0).toMap().value(QStringLiteral("layerKind")).toString(),
             QStringLiteral("layer"));
    QCOMPARE(rows.at(0).toMap().value(QStringLiteral("contentKind")).toString(),
             QStringLiteral("text"));
    QCOMPARE(rows.at(0).toMap().value(QStringLiteral("selected")).toBool(), true);
    QCOMPARE(rows.at(1).toMap().value(QStringLiteral("label")).toString(),
             QStringLiteral("Ellipse"));
    QCOMPARE(rows.at(1).toMap().value(QStringLiteral("layerKind")).toString(),
             QStringLiteral("layer"));
    QCOMPARE(rows.at(1).toMap().value(QStringLiteral("contentKind")).toString(),
             QStringLiteral("shape"));
    QCOMPARE(rows.at(2).toMap().value(QStringLiteral("label")).toString(),
             QStringLiteral("Layer 1"));
    QCOMPARE(rows.at(2).toMap().value(QStringLiteral("layerKind")).toString(),
             QStringLiteral("layer"));
    QCOMPARE(rows.at(2).toMap().value(QStringLiteral("contentKind")).toString(),
             QStringLiteral("layer"));
    QCOMPARE(rows.at(3).toMap().value(QStringLiteral("key")).toString(),
             QStringLiteral("raster-canvas"));
    QCOMPARE(rows.at(3).toMap().value(QStringLiteral("layerKind")).toString(),
             QStringLiteral("raster"));
}

void tst_DrawingSurfaceItem::savesRasterLayerItemsAsIndependentCanvasLayers()
{
    PaletteUtils paletteUtils;
    CanvasDocumentViewModel viewModel(&paletteUtils);
    viewModel.setBrushSize(10);
    viewModel.setBrushColor(QColor(QStringLiteral("#1976d2")));

    DrawingSurfaceItem baseItem;
    baseItem.setWidth(48);
    baseItem.setHeight(36);
    baseItem.setDocumentViewModel(&viewModel);

    DrawingSurfaceItem layerItem;
    layerItem.setWidth(48);
    layerItem.setHeight(36);
    layerItem.setDocumentViewModel(&viewModel);

    layerItem.beginStroke(18, 18, 1.0, false);
    layerItem.endStroke(18, 18, 1.0, false);
    QTRY_COMPARE(layerItem.strokeCount(), 1);

    QVariantMap layerObject;
    layerObject.insert(QStringLiteral("id"), 1);
    layerObject.insert(QStringLiteral("type"), QStringLiteral("layer"));
    layerObject.insert(QStringLiteral("name"), QStringLiteral("Layer 1"));
    layerObject.insert(QStringLiteral("x"), 0);
    layerObject.insert(QStringLiteral("y"), 0);
    layerObject.insert(QStringLiteral("width"), 48);
    layerObject.insert(QStringLiteral("height"), 36);
    layerObject.insert(QStringLiteral("opacity"), 1);
    layerObject.insert(QStringLiteral("visible"), true);

    QVariantMap rasterLayerDescriptor;
    rasterLayerDescriptor.insert(QStringLiteral("objectId"), 1);
    rasterLayerDescriptor.insert(QStringLiteral("item"),
                                 QVariant::fromValue(static_cast<QObject*>(&layerItem)));

    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString layeredPath = dir.filePath(QStringLiteral("layered-output.png"));
    QVERIFY(baseItem.saveToFileWithObjectsAndRasterLayers(layeredPath, {layerObject},
                                                          {rasterLayerDescriptor}));
    const QImage layered(layeredPath);
    QVERIFY(!layered.isNull());
    QCOMPARE(layered.size(), QSize(48, 36));
    QVERIFY(layered.pixelColor(18, 18).alpha() > 0);
    QVERIFY(layered.pixelColor(18, 18).blue() > 120);

    const QString baseOnlyPath = dir.filePath(QStringLiteral("base-only-output.png"));
    QVERIFY(baseItem.saveToFileWithObjectsAndRasterLayers(baseOnlyPath, {}, {}));
    const QImage baseOnly(baseOnlyPath);
    QVERIFY(!baseOnly.isNull());
    QCOMPARE(baseOnly.pixelColor(18, 18).rgba(), QColor(Qt::white).rgba());
}

void tst_DrawingSurfaceItem::deletingRasterLayerRemovesItsPaintFromQmlComposite()
{
    qmlRegisterType<DrawingSurfaceItem>("Congregation", 1, 0, "DrawingSurfaceItem");

    QQmlEngine engine;

    QQmlComponent component(&engine);
    const QString drawingSurfaceQml = QFINDTESTDATA("../App/qml/painting/DrawingSurface.qml");
    QVERIFY2(!drawingSurfaceQml.isEmpty(), "DrawingSurface.qml test data was not found");
    component.loadUrl(QUrl::fromLocalFile(drawingSurfaceQml));
    QTRY_VERIFY(component.isReady() || component.isError());
    QVERIFY2(component.isReady(), qPrintable(qmlErrorsToString(component.errors())));

    PaletteUtils paletteUtils;
    CanvasDocumentViewModel viewModel(&paletteUtils);

    QVariantMap initialProperties;
    initialProperties.insert(QStringLiteral("width"), 500);
    initialProperties.insert(QStringLiteral("height"), 360);
    initialProperties.insert(QStringLiteral("documentViewModel"),
                             QVariant::fromValue(static_cast<QObject*>(&viewModel)));
    initialProperties.insert(QStringLiteral("brushColor"), QColor(QStringLiteral("#1976d2")));
    initialProperties.insert(QStringLiteral("brushSize"), 10);
    initialProperties.insert(QStringLiteral("toolMode"), QStringLiteral("brush"));

    QScopedPointer<QObject> object(component.createWithInitialProperties(initialProperties));
    QVERIFY2(!object.isNull(), qPrintable(qmlErrorsToString(component.errors())));

    auto* rootItem = qobject_cast<QQuickItem*>(object.data());
    QVERIFY(rootItem);
    QCOMPARE(rootItem->property("selectedDrawableObjectId").toInt(), 1);

    QTRY_VERIFY(
        [&]()
        {
            QQmlExpression layerReady(
                engine.rootContext(), object.data(),
                QStringLiteral(
                    "rasterLayerItemById(1) !== null && rasterLayerItemById(1).width > 1"));
            const QVariant result = layerReady.evaluate();
            return !layerReady.hasError() && result.toBool();
        }());

    QQmlExpression paintLayer(engine.rootContext(), object.data(),
                              QStringLiteral("var rasterSurface = rasterLayerItemById(1);"
                                             "rasterSurface.beginStroke(24, 24, 1.0, false);"
                                             "rasterSurface.endStroke(24, 24, 1.0, false);"));
    paintLayer.evaluate();
    QVERIFY2(!paintLayer.hasError(), qPrintable(paintLayer.error().toString()));

    QTRY_VERIFY(
        [&]()
        {
            QQmlExpression strokeCommitted(
                engine.rootContext(), object.data(),
                QStringLiteral(
                    "rasterLayerItemById(1) !== null && rasterLayerItemById(1).strokeCount === 1"));
            const QVariant result = strokeCommitted.evaluate();
            return !strokeCommitted.hasError() && result.toBool();
        }());

    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString beforeDeletePath = dir.filePath(QStringLiteral("before-delete.png"));
    QQmlExpression saveBeforeDelete(engine.rootContext(), object.data(),
                                    QStringLiteral("saveToFile(\"%1\");")
                                        .arg(QUrl::fromLocalFile(beforeDeletePath).toString()));
    const QVariant saveBeforeResult = saveBeforeDelete.evaluate();
    QVERIFY2(!saveBeforeDelete.hasError(), qPrintable(saveBeforeDelete.error().toString()));
    QCOMPARE(saveBeforeResult.toBool(), true);

    const QImage beforeDelete(beforeDeletePath);
    QVERIFY(!beforeDelete.isNull());
    QVERIFY(beforeDelete.pixelColor(24, 24).alpha() > 0);

    QQmlExpression deleteLayer(engine.rootContext(), object.data(),
                               QStringLiteral("deleteLayerByKey(\"object-1\");"));
    const QVariant deleteResult = deleteLayer.evaluate();
    QVERIFY2(!deleteLayer.hasError(), qPrintable(deleteLayer.error().toString()));
    QCOMPARE(deleteResult.toBool(), true);

    const QString afterDeletePath = dir.filePath(QStringLiteral("after-delete.png"));
    QQmlExpression saveAfterDelete(
        engine.rootContext(), object.data(),
        QStringLiteral("saveToFile(\"%1\");").arg(QUrl::fromLocalFile(afterDeletePath).toString()));
    const QVariant saveAfterResult = saveAfterDelete.evaluate();
    QVERIFY2(!saveAfterDelete.hasError(), qPrintable(saveAfterDelete.error().toString()));
    QCOMPARE(saveAfterResult.toBool(), true);

    const QImage afterDelete(afterDeletePath);
    QVERIFY(!afterDelete.isNull());
    QCOMPARE(afterDelete.pixelColor(24, 24).rgba(), QColor(Qt::white).rgba());
}

void tst_DrawingSurfaceItem::addsManyRasterLayersWithoutSnapshotChurn()
{
    qmlRegisterType<DrawingSurfaceItem>("Congregation", 1, 0, "DrawingSurfaceItem");

    QQmlEngine engine;

    QQmlComponent component(&engine);
    const QString drawingSurfaceQml = QFINDTESTDATA("../App/qml/painting/DrawingSurface.qml");
    QVERIFY2(!drawingSurfaceQml.isEmpty(), "DrawingSurface.qml test data was not found");
    component.loadUrl(QUrl::fromLocalFile(drawingSurfaceQml));
    QTRY_VERIFY(component.isReady() || component.isError());
    QVERIFY2(component.isReady(), qPrintable(qmlErrorsToString(component.errors())));

    PaletteUtils paletteUtils;
    CanvasDocumentViewModel viewModel(&paletteUtils);

    QVariantMap initialProperties;
    initialProperties.insert(QStringLiteral("width"), 500);
    initialProperties.insert(QStringLiteral("height"), 360);
    initialProperties.insert(QStringLiteral("documentViewModel"),
                             QVariant::fromValue(static_cast<QObject*>(&viewModel)));

    QScopedPointer<QObject> object(component.createWithInitialProperties(initialProperties));
    QVERIFY2(!object.isNull(), qPrintable(qmlErrorsToString(component.errors())));
    auto* rootItem = qobject_cast<QQuickItem*>(object.data());
    QVERIFY(rootItem);

    QQmlExpression addLayers(
        engine.rootContext(), object.data(),
        QStringLiteral("for (let index = 0; index < 32; ++index) { addEmptyLayer(); } true;"));
    const QVariant addLayersResult = addLayers.evaluate();
    QVERIFY2(!addLayers.hasError(), qPrintable(addLayers.error().toString()));
    QCOMPARE(addLayersResult.toBool(), true);

    QVariantList objects = rootItem->property("drawableObjects").toList();
    QCOMPARE(objects.size(), 33);
    QCOMPARE(objects.first().toMap().value(QStringLiteral("name")).toString(),
             QStringLiteral("Layer 1"));
    QCOMPARE(objects.last().toMap().value(QStringLiteral("name")).toString(),
             QStringLiteral("Layer 33"));
    QCOMPARE(rootItem->property("selectedDrawableObjectId").toInt(), 33);

    QQmlExpression visualModelCount(engine.rootContext(), object.data(),
                                    QStringLiteral("drawableObjectVisualModelCount();"));
    const QVariant visualModelCountResult = visualModelCount.evaluate();
    QVERIFY2(!visualModelCount.hasError(), qPrintable(visualModelCount.error().toString()));
    QCOMPARE(visualModelCountResult.toInt(), 33);

    QTRY_VERIFY(
        [&]()
        {
            QQmlExpression rasterLayerItemCount(
                engine.rootContext(), object.data(),
                QStringLiteral("Object.keys(rasterLayerItems).length;"));
            const QVariant result = rasterLayerItemCount.evaluate();
            return !rasterLayerItemCount.hasError() && result.toInt() == 33;
        }());

    QQmlExpression snapshotCount(engine.rootContext(), object.data(),
                                 QStringLiteral("Object.keys(rasterLayerSnapshotSources).length;"));
    const QVariant snapshotCountResult = snapshotCount.evaluate();
    QVERIFY2(!snapshotCount.hasError(), qPrintable(snapshotCount.error().toString()));
    QCOMPARE(snapshotCountResult.toInt(), 0);

    QVariantList rows = rootItem->property("layerHierarchyRows").toList();
    QCOMPARE(rows.size(), 34);
    QCOMPARE(rows.first().toMap().value(QStringLiteral("label")).toString(),
             QStringLiteral("Layer 33"));
    QCOMPARE(rows.first().toMap().value(QStringLiteral("selected")).toBool(), true);
    QCOMPARE(rows.last().toMap().value(QStringLiteral("key")).toString(),
             QStringLiteral("raster-canvas"));
}

void tst_DrawingSurfaceItem::renamesLayerRowsAndDrawableObjectMetadata()
{
    qmlRegisterType<DrawingSurfaceItem>("Congregation", 1, 0, "DrawingSurfaceItem");

    QQmlEngine engine;

    QQmlComponent component(&engine);
    const QString drawingSurfaceQml = QFINDTESTDATA("../App/qml/painting/DrawingSurface.qml");
    QVERIFY2(!drawingSurfaceQml.isEmpty(), "DrawingSurface.qml test data was not found");
    component.loadUrl(QUrl::fromLocalFile(drawingSurfaceQml));
    QTRY_VERIFY(component.isReady() || component.isError());
    QVERIFY2(component.isReady(), qPrintable(qmlErrorsToString(component.errors())));

    PaletteUtils paletteUtils;
    CanvasDocumentViewModel viewModel(&paletteUtils);

    QVariantMap initialProperties;
    initialProperties.insert(QStringLiteral("width"), 500);
    initialProperties.insert(QStringLiteral("height"), 360);
    initialProperties.insert(QStringLiteral("documentViewModel"),
                             QVariant::fromValue(static_cast<QObject*>(&viewModel)));

    QScopedPointer<QObject> object(component.createWithInitialProperties(initialProperties));
    QVERIFY2(!object.isNull(), qPrintable(qmlErrorsToString(component.errors())));
    auto* rootItem = qobject_cast<QQuickItem*>(object.data());
    QVERIFY(rootItem);

    QQmlExpression canRenameLayer(engine.rootContext(), object.data(),
                                  QStringLiteral("canRenameLayerByKey(\"object-1\");"));
    const QVariant canRenameResult = canRenameLayer.evaluate();
    QVERIFY2(!canRenameLayer.hasError(), qPrintable(canRenameLayer.error().toString()));
    QCOMPARE(canRenameResult.toBool(), true);

    QQmlExpression rasterRenameCheck(engine.rootContext(), object.data(),
                                     QStringLiteral("canRenameLayerByKey(\"raster-canvas\");"));
    const QVariant rasterRenameCheckResult = rasterRenameCheck.evaluate();
    QVERIFY2(!rasterRenameCheck.hasError(), qPrintable(rasterRenameCheck.error().toString()));
    QCOMPARE(rasterRenameCheckResult.toBool(), false);

    QQmlExpression renameLayer(engine.rootContext(), object.data(),
                               QStringLiteral("renameLayerByKey(\"object-1\", \"  Ink pass  \");"));
    const QVariant renameResult = renameLayer.evaluate();
    QVERIFY2(!renameLayer.hasError(), qPrintable(renameLayer.error().toString()));
    QCOMPARE(renameResult.toBool(), true);

    QVariantList objects = rootItem->property("drawableObjects").toList();
    QCOMPARE(objects.size(), 1);
    QCOMPARE(objects.first().toMap().value(QStringLiteral("name")).toString(),
             QStringLiteral("Ink pass"));

    QVariantList rows = rootItem->property("layerHierarchyRows").toList();
    QCOMPARE(rows.size(), 2);
    QCOMPARE(rows.first().toMap().value(QStringLiteral("label")).toString(),
             QStringLiteral("Ink pass"));

    QQmlExpression layerName(engine.rootContext(), object.data(),
                             QStringLiteral("layerNameByKey(\"object-1\");"));
    const QVariant layerNameResult = layerName.evaluate();
    QVERIFY2(!layerName.hasError(), qPrintable(layerName.error().toString()));
    QCOMPARE(layerNameResult.toString(), QStringLiteral("Ink pass"));

    QQmlExpression rejectBlankRename(engine.rootContext(), object.data(),
                                     QStringLiteral("renameLayerByKey(\"object-1\", \"   \");"));
    const QVariant rejectBlankRenameResult = rejectBlankRename.evaluate();
    QVERIFY2(!rejectBlankRename.hasError(), qPrintable(rejectBlankRename.error().toString()));
    QCOMPARE(rejectBlankRenameResult.toBool(), false);

    objects = rootItem->property("drawableObjects").toList();
    QCOMPARE(objects.first().toMap().value(QStringLiteral("name")).toString(),
             QStringLiteral("Ink pass"));
}

void tst_DrawingSurfaceItem::layersExposeHierarchyRowsAndReorderDrawableObjects()
{
    qmlRegisterType<DrawingSurfaceItem>("Congregation", 1, 0, "DrawingSurfaceItem");

    QQmlEngine engine;

    QQmlComponent component(&engine);
    const QString drawingSurfaceQml = QFINDTESTDATA("../App/qml/painting/DrawingSurface.qml");
    QVERIFY2(!drawingSurfaceQml.isEmpty(), "DrawingSurface.qml test data was not found");
    component.loadUrl(QUrl::fromLocalFile(drawingSurfaceQml));
    QTRY_VERIFY(component.isReady() || component.isError());
    QVERIFY2(component.isReady(), qPrintable(qmlErrorsToString(component.errors())));

    PaletteUtils paletteUtils;
    CanvasDocumentViewModel viewModel(&paletteUtils);

    QVariantMap initialProperties;
    initialProperties.insert(QStringLiteral("width"), 500);
    initialProperties.insert(QStringLiteral("height"), 360);
    initialProperties.insert(QStringLiteral("documentViewModel"),
                             QVariant::fromValue(static_cast<QObject*>(&viewModel)));

    QScopedPointer<QObject> object(component.createWithInitialProperties(initialProperties));
    QVERIFY2(!object.isNull(), qPrintable(qmlErrorsToString(component.errors())));
    auto* rootItem = qobject_cast<QQuickItem*>(object.data());
    QVERIFY(rootItem);

    QQmlExpression appendObjects(
        engine.rootContext(), object.data(),
        QStringLiteral("appendDrawableObject({ id: 2, type: \"shape\", x: 10, y: 20, width: 30, "
                       "height: 28, shapeKind: \"rectangle\", color: \"#1976d2\" });"
                       "appendDrawableObject({ id: 3, type: \"text\", x: 40, y: 50, width: 120, "
                       "height: 32, text: \"Label\", fontPixelSize: 18, color: \"#111111\" });"));
    appendObjects.evaluate();
    QVERIFY2(!appendObjects.hasError(), qPrintable(appendObjects.error().toString()));

    QVariantList rows = rootItem->property("layerHierarchyRows").toList();
    QCOMPARE(rows.size(), 4);
    QCOMPARE(rows.at(0).toMap().value(QStringLiteral("objectId")).toInt(), 3);
    QCOMPARE(rows.at(0).toMap().value(QStringLiteral("layerKind")).toString(),
             QStringLiteral("layer"));
    QCOMPARE(rows.at(0).toMap().value(QStringLiteral("contentKind")).toString(),
             QStringLiteral("text"));
    QCOMPARE(rows.at(0).toMap().value(QStringLiteral("label")).toString(), QStringLiteral("Label"));
    QCOMPARE(rows.at(0).toMap().value(QStringLiteral("selected")).toBool(), true);
    QCOMPARE(rows.at(1).toMap().value(QStringLiteral("objectId")).toInt(), 2);
    QCOMPARE(rows.at(1).toMap().value(QStringLiteral("layerKind")).toString(),
             QStringLiteral("layer"));
    QCOMPARE(rows.at(1).toMap().value(QStringLiteral("contentKind")).toString(),
             QStringLiteral("shape"));
    QCOMPARE(rows.at(1).toMap().value(QStringLiteral("label")).toString(),
             QStringLiteral("Rectangle"));
    QCOMPARE(rows.at(1).toMap().value(QStringLiteral("draggable")).toBool(), true);
    QCOMPARE(rows.at(2).toMap().value(QStringLiteral("objectId")).toInt(), 1);
    QCOMPARE(rows.at(2).toMap().value(QStringLiteral("layerKind")).toString(),
             QStringLiteral("layer"));
    QCOMPARE(rows.at(2).toMap().value(QStringLiteral("contentKind")).toString(),
             QStringLiteral("layer"));
    QCOMPARE(rows.at(2).toMap().value(QStringLiteral("label")).toString(),
             QStringLiteral("Layer 1"));
    QCOMPARE(rows.at(3).toMap().value(QStringLiteral("key")).toString(),
             QStringLiteral("raster-canvas"));
    QCOMPARE(rows.at(3).toMap().value(QStringLiteral("draggable")).toBool(), false);

    QQmlExpression selectLayer(engine.rootContext(), object.data(),
                               QStringLiteral("activateLayerByKey(\"object-2\");"));
    const QVariant selectResult = selectLayer.evaluate();
    QVERIFY2(!selectLayer.hasError(), qPrintable(selectLayer.error().toString()));
    QCOMPARE(selectResult.toBool(), true);
    QCOMPARE(rootItem->property("selectedDrawableObjectId").toInt(), 2);

    QQmlExpression reorderLayers(
        engine.rootContext(), object.data(),
        QStringLiteral("applyLayerHierarchyOrder(["
                       "{ key: \"object-2\", objectId: 2, layerKind: \"layer\", depth: 0 },"
                       "{ key: \"object-3\", objectId: 3, layerKind: \"layer\", depth: 0 },"
                       "{ key: \"object-1\", objectId: 1, layerKind: \"layer\", depth: 0 },"
                       "{ key: \"raster-canvas\", objectId: -1, layerKind: \"raster\", depth: 0 }"
                       "]);"));
    const QVariant reorderResult = reorderLayers.evaluate();
    QVERIFY2(!reorderLayers.hasError(), qPrintable(reorderLayers.error().toString()));
    QCOMPARE(reorderResult.toBool(), true);

    QVariantList objects = rootItem->property("drawableObjects").toList();
    QCOMPARE(objects.size(), 3);
    QCOMPARE(objects.at(0).toMap().value(QStringLiteral("id")).toInt(), 1);
    QCOMPARE(objects.at(1).toMap().value(QStringLiteral("id")).toInt(), 3);
    QCOMPARE(objects.at(2).toMap().value(QStringLiteral("id")).toInt(), 2);

    rows = rootItem->property("layerHierarchyRows").toList();
    QCOMPARE(rows.at(0).toMap().value(QStringLiteral("objectId")).toInt(), 2);
    QCOMPARE(rows.at(1).toMap().value(QStringLiteral("objectId")).toInt(), 3);
    QCOMPARE(rows.at(2).toMap().value(QStringLiteral("objectId")).toInt(), 1);
    QCOMPARE(rows.at(3).toMap().value(QStringLiteral("key")).toString(),
             QStringLiteral("raster-canvas"));

    QQmlExpression deleteLayer(engine.rootContext(), object.data(),
                               QStringLiteral("deleteLayerByKey(\"object-2\");"));
    const QVariant deleteResult = deleteLayer.evaluate();
    QVERIFY2(!deleteLayer.hasError(), qPrintable(deleteLayer.error().toString()));
    QCOMPARE(deleteResult.toBool(), true);

    objects = rootItem->property("drawableObjects").toList();
    QCOMPARE(objects.size(), 2);
    QCOMPARE(objects.at(0).toMap().value(QStringLiteral("id")).toInt(), 1);
    QCOMPARE(objects.at(1).toMap().value(QStringLiteral("id")).toInt(), 3);
    QCOMPARE(rootItem->property("selectedDrawableObjectId").toInt(), -1);
}

void tst_DrawingSurfaceItem::drawsAndSavesStroke()
{
    PaletteUtils paletteUtils;
    CanvasDocumentViewModel viewModel(&paletteUtils);
    DrawingSurfaceItem item;
    item.setWidth(128);
    item.setHeight(96);
    item.setDocumentViewModel(&viewModel);

    QSignalSpy contentChanged(&item, &DrawingSurfaceItem::rasterContentChanged);
    item.beginStroke(10, 10, 1.0, false);
    QVERIFY(item.appendStrokePoint(40, 40, 1.0, false));
    item.endStroke(60, 48, 1.0, false);

    QTRY_COMPARE(item.strokeCount(), 1);
    QTRY_VERIFY(contentChanged.count() > 0);

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString outputPath = dir.filePath(QStringLiteral("stroke-output.png"));
    QVERIFY(item.saveToFile(outputPath));
    QVERIFY(QFileInfo::exists(outputPath));

    const QImage saved(outputPath);
    QVERIFY(!saved.isNull());
    QCOMPARE(saved.size(), QSize(128, 96));
}

void tst_DrawingSurfaceItem::remoteInputPreviewsStrokeWithoutCommittingClientDocument()
{
    PaletteUtils participantPaletteUtils;
    CanvasDocumentViewModel participantViewModel(&participantPaletteUtils);
    DrawingSurfaceItem participantItem;
    participantItem.setWidth(128);
    participantItem.setHeight(96);
    participantItem.setDocumentViewModel(&participantViewModel);
    participantItem.setBrushSize(12);
    participantItem.setLivePreviewFrameIntervalMs(0);
    participantItem.setRemoteInputMode(true);

    QQuickWindow previewWindow;
    previewWindow.setColor(Qt::white);
    previewWindow.setGeometry(0, 0, 128, 96);
    participantItem.setParentItem(previewWindow.contentItem());
    previewWindow.show();
    QVERIFY2(QTest::qWaitForWindowExposed(&previewWindow, nativeWindowExposureTimeoutMs),
             "The remote stroke preview window was not exposed before the compositor timeout");
    QTRY_VERIFY_WITH_TIMEOUT(!participantItem.rendering(), 5000);

    const RasterLayer* initialRaster = participantItem.selectedRasterPixels();
    QVERIFY(initialRaster);
    const std::vector<std::uint32_t> initialPixels = initialRaster->pixels;
    QVERIFY(!participantItem.canUndo());

    QSignalSpy remoteStrokeSpy(&participantItem, &DrawingSurfaceItem::remoteStrokeRequested);
    QSignalSpy previewRenderSpy(&participantItem, &DrawingSurfaceItem::renderCompleted);
    QTest::qWait(100);
    previewRenderSpy.clear();
    participantItem.beginStroke(10, 12, 0.4, true);
    QVERIFY(participantItem.liveStrokeActive());
    QVERIFY(participantItem.appendStrokePoint(30, 32, 0.6, true));
    QTRY_VERIFY_WITH_TIMEOUT(previewRenderSpy.size() > 0, 5000);
    QTRY_VERIFY_WITH_TIMEOUT(!participantItem.rendering(), 5000);
    const RasterLayer* previewRaster = participantItem.selectedRasterPixels();
    QVERIFY(previewRaster);
    QVERIFY(previewRaster->pixels != initialPixels);
    QCOMPARE(participantItem.strokeCount(), 0);
    QTest::qWait(50);
    const QImage activePreviewFrame = previewWindow.grabWindow();
    QVERIFY(!activePreviewFrame.isNull());
    const int activePreviewPixels = countLogicalAnnulusPixels(
        activePreviewFrame, previewWindow.size(), QPointF(30, 32), 0, 8,
        [](const QColor& pixel)
        {
            return pixel.alpha() > 200 && pixel.red() < 200 && pixel.green() < 200 &&
                   pixel.blue() < 200;
        });
    QVERIFY2(activePreviewPixels > 0,
             "The participant's active remote stroke did not render in its local window");

    const int previewRenderCount = previewRenderSpy.size();
    participantItem.endStroke(50, 52, 0.8, true);

    QVERIFY(!participantItem.liveStrokeActive());
    QTRY_VERIFY_WITH_TIMEOUT(previewRenderSpy.size() > previewRenderCount, 5000);
    QTRY_VERIFY_WITH_TIMEOUT(!participantItem.rendering(), 5000);
    QCOMPARE(participantItem.strokeCount(), 0);
    const RasterLayer* restoredRaster = participantItem.selectedRasterPixels();
    QVERIFY(restoredRaster);
    QCOMPARE(restoredRaster->pixels, initialPixels);
    QVERIFY(!participantItem.canUndo());
    QTest::qWait(50);
    const QImage restoredPreviewFrame = previewWindow.grabWindow();
    QVERIFY(!restoredPreviewFrame.isNull());
    const int restoredPreviewPixels = countLogicalAnnulusPixels(
        restoredPreviewFrame, previewWindow.size(), QPointF(30, 32), 0, 8,
        [](const QColor& pixel)
        {
            return pixel.alpha() > 200 && pixel.red() < 200 && pixel.green() < 200 &&
                   pixel.blue() < 200;
        });
    QCOMPARE(restoredPreviewPixels, 0);
    QCOMPARE(remoteStrokeSpy.size(), 1);
    const QVariantList points = remoteStrokeSpy.first().at(0).toList();
    QCOMPARE(points.size(), 3);
    QCOMPARE(points.first().toMap().value(QStringLiteral("x")).toDouble(), 10.0);
    QCOMPARE(points.last().toMap().value(QStringLiteral("pressure")).toDouble(), 0.8);
    QVERIFY(remoteStrokeSpy.first().at(1).toBool());

    const QVariantMap style{{QStringLiteral("tool"), QStringLiteral("brush")},
                            {QStringLiteral("color"), QStringLiteral("#224466")},
                            {QStringLiteral("size"), 8.0},
                            {QStringLiteral("flow"), 1.0},
                            {QStringLiteral("opacity"), 1.0},
                            {QStringLiteral("hardness"), 1.0},
                            {QStringLiteral("spacing"), 0.0},
                            {QStringLiteral("spacingRatio"), 0.0},
                            {QStringLiteral("pressureMinimum"), 0.0},
                            {QStringLiteral("pressureCenter"), 0.5},
                            {QStringLiteral("pressureMaximum"), 1.0},
                            {QStringLiteral("pressureOpacity"), true},
                            {QStringLiteral("stabilizer"), 0.0}};
    PaletteUtils hostPaletteUtils;
    CanvasDocumentViewModel hostViewModel(&hostPaletteUtils);
    DrawingSurfaceItem hostItem;
    hostItem.setWidth(128);
    hostItem.setHeight(96);
    hostItem.setDocumentViewModel(&hostViewModel);

    QVERIFY(hostItem.applyRemoteStroke(style, points, true));
    QTRY_COMPARE(hostItem.strokeCount(), 1);
    QCOMPARE(participantItem.strokeCount(), 0);
    QVERIFY(participantItem.remoteInputMode());
    participantItem.setParentItem(nullptr);
}

void tst_DrawingSurfaceItem::qmlRemoteParticipantRoutesMutationToHostCanvas()
{
    QQmlEngine engine;
    QQmlComponent component(&engine);
    const QString drawingSurfaceQml = QFINDTESTDATA("../App/qml/painting/DrawingSurface.qml");
    QVERIFY2(!drawingSurfaceQml.isEmpty(), "DrawingSurface.qml test data was not found");
    component.loadUrl(QUrl::fromLocalFile(drawingSurfaceQml));
    QTRY_VERIFY(component.isReady() || component.isError());
    QVERIFY2(component.isReady(), qPrintable(qmlErrorsToString(component.errors())));

    PaletteUtils paletteUtils;
    CanvasDocumentViewModel viewModel(&paletteUtils);
    QVariantMap initialProperties{{QStringLiteral("width"), 320},
                                  {QStringLiteral("height"), 240},
                                  {QStringLiteral("documentViewModel"),
                                   QVariant::fromValue(static_cast<QObject*>(&viewModel))}};
    QScopedPointer<QObject> object(component.createWithInitialProperties(initialProperties));
    QVERIFY2(!object.isNull(), qPrintable(qmlErrorsToString(component.errors())));
    auto* rootItem = qobject_cast<QQuickItem*>(object.data());
    QVERIFY(rootItem);
    QQuickWindow window;
    window.setGeometry(0, 0, 320, 240);
    rootItem->setParentItem(window.contentItem());
    window.show();
    QTRY_VERIFY_WITH_TIMEOUT(window.isExposed(), nativeWindowExposureTimeoutMs);
    QTRY_VERIFY(findDrawingSurfaceItem(rootItem));

    QQmlExpression initialSnapshotExpression(engine.rootContext(), object.data(),
                                             QStringLiteral("canvasSessionSnapshot();"));
    const QByteArray initialSnapshot = initialSnapshotExpression.evaluate().toByteArray();
    QVERIFY2(!initialSnapshotExpression.hasError(),
             qPrintable(initialSnapshotExpression.error().toString()));
    QVERIFY(!initialSnapshot.isEmpty());

    QVERIFY(rootItem->setProperty("hostCanvasInputClient", true));
    QVERIFY(rootItem->setProperty("toolMode", QStringLiteral("fill")));
    QSignalSpy commandSpy(object.data(), SIGNAL(hostEditCommandRequested(QVariant)));
    QQmlExpression participantFill(engine.rootContext(), object.data(),
                                   QStringLiteral("fillAt(12, 14);"));
    QCOMPARE(participantFill.evaluate().toBool(), true);
    QVERIFY2(!participantFill.hasError(), qPrintable(participantFill.error().toString()));
    QCOMPARE(commandSpy.size(), 1);
    const QVariantMap command = commandSpy.first().first().toMap();
    QCOMPARE(command.value(QStringLiteral("type")).toString(), QStringLiteral("fill"));

    QQmlExpression clientSnapshotExpression(engine.rootContext(), object.data(),
                                            QStringLiteral("canvasSessionSnapshot();"));
    const QByteArray clientSnapshot = clientSnapshotExpression.evaluate().toByteArray();
    QVERIFY2(!clientSnapshotExpression.hasError(),
             qPrintable(clientSnapshotExpression.error().toString()));
    QCOMPARE(clientSnapshot, initialSnapshot);

    QVERIFY(rootItem->setProperty("hostCanvasInputClient", false));
    engine.rootContext()->setContextProperty(QStringLiteral("testHostCommand"), command);
    QQmlExpression hostApply(engine.rootContext(), object.data(),
                             QStringLiteral("applyHostEditCommand(testHostCommand);"));
    QCOMPARE(hostApply.evaluate().toBool(), true);
    QVERIFY2(!hostApply.hasError(), qPrintable(hostApply.error().toString()));

    QQmlExpression hostSnapshotExpression(engine.rootContext(), object.data(),
                                          QStringLiteral("canvasSessionSnapshot();"));
    const QByteArray hostSnapshot = hostSnapshotExpression.evaluate().toByteArray();
    QVERIFY2(!hostSnapshotExpression.hasError(),
             qPrintable(hostSnapshotExpression.error().toString()));
    QVERIFY(!hostSnapshot.isEmpty());
    QVERIFY(hostSnapshot != initialSnapshot);
    rootItem->setParentItem(nullptr);
}

void tst_DrawingSurfaceItem::rendersSharedRasterVectorTimelineDocument()
{
    using namespace iiSharedCanvas;

    PaletteUtils paletteUtils;
    CanvasDocumentViewModel viewModel(&paletteUtils);
    DrawingSurfaceItem item;
    item.setWidth(4);
    item.setHeight(4);
    item.setDocumentViewModel(&viewModel);

    Document mixed;
    mixed.extent = {4, 4};
    mixed.timeline = {{24, 1}, 2};
    mixed.assets.emplace_back(RasterAsset{"background", makeRasterLayer(4, 4, 0xff102030U)});
    mixed.assets.emplace_back(RasterAsset{"motion-0", makeRasterLayer(4, 4, 0x00000000U)});
    RasterLayer motionFrame = makeRasterLayer(4, 4, 0x00000000U);
    motionFrame.pixels[0] = 0xffff3366U;
    mixed.assets.emplace_back(RasterAsset{"motion-1", std::move(motionFrame)});

    VectorPath rectangle;
    rectangle.commands = {
        MoveTo{{1.0, 1.0}}, LineTo{{3.0, 1.0}}, LineTo{{3.0, 3.0}}, LineTo{{1.0, 3.0}}, ClosePath{},
    };
    rectangle.fill = SolidPaint{0xffffcc00U};
    mixed.assets.emplace_back(VectorAsset{"vector", {4, 4}, {std::move(rectangle)}});
    mixed.layers.emplace_back(BitmapLayer{
        {"background-layer", "Background", true, 1.0, {}, RasterBlendMode::SourceOver},
        StaticSource{"background"},
    });
    mixed.layers.emplace_back(BitmapLayer{
        {"motion-layer", "Motion", true, 1.0, {}, RasterBlendMode::SourceOver},
        KeyframedSource{{0, 1}},
    });
    mixed.frames = {
        {0, {{"motion-layer", "motion-0"}}},
        {1, {{"motion-layer", "motion-1"}}},
    };
    mixed.layers.emplace_back(VectorLayer{
        {"vector-layer", "Vector", true, 1.0, {}, RasterBlendMode::SourceOver},
        StaticSource{"vector"},
    });

    QVERIFY(item.document());
    *item.document() = std::move(mixed);
    QVERIFY(item.refresh());
    QTRY_VERIFY_WITH_TIMEOUT(item.framePixels(), 5000);
    QCOMPARE(item.framePixels()->pixels[0], 0xff102030U);
    QCOMPARE(item.framePixels()->pixels[5], 0xffffcc00U);

    item.setFrame(1);
    QCOMPARE(item.frame(), 1U);
    QTRY_VERIFY_WITH_TIMEOUT(item.framePixels() && item.framePixels()->pixels[0] == 0xffff3366U,
                             5000);
    QCOMPARE(item.framePixels()->pixels[5], 0xffffcc00U);
    QTRY_VERIFY_WITH_TIMEOUT(!item.rendering(), 5000);
}

void tst_DrawingSurfaceItem::rasterToolsPreserveMixedSharedCanvasLayers()
{
    using namespace iiSharedCanvas;

    PaletteUtils paletteUtils;
    CanvasDocumentViewModel viewModel(&paletteUtils);
    DrawingSurfaceItem item;
    item.setWidth(32);
    item.setHeight(32);
    item.setDocumentViewModel(&viewModel);

    Document mixed;
    mixed.extent = {32, 32};
    mixed.timeline = {{24, 1}, 1};
    mixed.assets.emplace_back(RasterAsset{"paint", makeRasterLayer(32, 32, 0x00000000U)});

    VectorPath rectangle;
    rectangle.commands = {
        MoveTo{{2.0, 2.0}},  LineTo{{10.0, 2.0}}, LineTo{{10.0, 10.0}},
        LineTo{{2.0, 10.0}}, ClosePath{},
    };
    rectangle.fill = SolidPaint{0xffff3366U};
    mixed.assets.emplace_back(VectorAsset{"vector", {32, 32}, {std::move(rectangle)}});
    mixed.layers.emplace_back(BitmapLayer{
        {"paint-layer", "Paint", true, 1.0, {}, RasterBlendMode::SourceOver},
        StaticSource{"paint"},
    });
    mixed.layers.emplace_back(VectorLayer{
        {"vector-layer", "Vector", true, 1.0, {}, RasterBlendMode::SourceOver},
        StaticSource{"vector"},
    });

    QVERIFY(item.document());
    *item.document() = std::move(mixed);
    QVERIFY(item.refresh());
    QVERIFY(item.selectLayer(QStringLiteral("paint-layer")));
    QTRY_VERIFY_WITH_TIMEOUT(
        item.framePixels() && item.framePixels()->pixels[4 * 32 + 4] == 0xffff3366U, 5000);

    const QColor shapeColor(QStringLiteral("#1976d2"));
    QVERIFY(item.commitShape(20, 20, 6, 6, QStringLiteral("rectangle"), shapeColor));

    const RasterAsset* paint = findRasterAsset(*item.document(), "paint");
    QVERIFY(paint);
    QCOMPARE(paint->pixels.pixels[4 * 32 + 4], 0x00000000U);
    QCOMPARE(paint->pixels.pixels[22 * 32 + 22], shapeColor.rgba());
    QTRY_VERIFY_WITH_TIMEOUT(item.framePixels() &&
                                 item.framePixels()->pixels[4 * 32 + 4] == 0xffff3366U &&
                                 item.framePixels()->pixels[22 * 32 + 22] == shapeColor.rgba(),
                             5000);
    QTRY_VERIFY_WITH_TIMEOUT(!item.rendering(), 5000);
}

void tst_DrawingSurfaceItem::rasterToolsRespectSelectedLayerTransform()
{
    using namespace iiSharedCanvas;

    PaletteUtils paletteUtils;
    CanvasDocumentViewModel viewModel(&paletteUtils);
    DrawingSurfaceItem item;
    item.setWidth(32);
    item.setHeight(32);
    item.setDocumentViewModel(&viewModel);

    Document transformed;
    transformed.extent = {32, 32};
    transformed.timeline = {{24, 1}, 1};
    transformed.assets.emplace_back(RasterAsset{"paint", makeRasterLayer(8, 8, 0x00000000U)});
    AffineTransform layerTransform;
    layerTransform.translationX = 10.0;
    layerTransform.translationY = 5.0;
    transformed.layers.emplace_back(BitmapLayer{
        {"paint-layer", "Paint", true, 1.0, layerTransform, RasterBlendMode::SourceOver},
        StaticSource{"paint"},
    });

    QVERIFY(item.document());
    *item.document() = std::move(transformed);
    QVERIFY(item.refresh());
    QVERIFY(item.selectLayer(QStringLiteral("paint-layer")));

    const QColor shapeColor(QStringLiteral("#43a047"));
    QVERIFY(item.commitShape(12, 7, 4, 4, QStringLiteral("rectangle"), shapeColor));

    const RasterAsset* paint = findRasterAsset(*item.document(), "paint");
    QVERIFY(paint);
    QCOMPARE(paint->pixels.width, 8);
    QCOMPARE(paint->pixels.height, 8);
    QCOMPARE(paint->pixels.pixels[3 * 8 + 3], shapeColor.rgba());
    QTRY_VERIFY_WITH_TIMEOUT(
        item.framePixels() && item.framePixels()->pixels[8 * 32 + 13] == shapeColor.rgba(), 5000);
    QTRY_VERIFY_WITH_TIMEOUT(!item.rendering(), 5000);
}

void tst_DrawingSurfaceItem::openingRasterReplacesMixedSharedCanvasDocument()
{
    using namespace iiSharedCanvas;

    PaletteUtils paletteUtils;
    CanvasDocumentViewModel viewModel(&paletteUtils);
    DrawingSurfaceItem item;
    item.setWidth(16);
    item.setHeight(16);
    item.setDocumentViewModel(&viewModel);

    Document mixed;
    mixed.extent = {16, 16};
    mixed.timeline = {{24, 1}, 1};
    mixed.assets.emplace_back(RasterAsset{"paint", makeRasterLayer(16, 16, 0x00000000U)});
    VectorPath rectangle;
    rectangle.commands = {
        MoveTo{{2.0, 2.0}},  LineTo{{14.0, 2.0}}, LineTo{{14.0, 14.0}},
        LineTo{{2.0, 14.0}}, ClosePath{},
    };
    rectangle.fill = SolidPaint{0xffff3366U};
    mixed.assets.emplace_back(VectorAsset{"vector", {16, 16}, {std::move(rectangle)}});
    mixed.layers.emplace_back(BitmapLayer{
        {"paint-layer", "Paint", true, 1.0, {}, RasterBlendMode::SourceOver},
        StaticSource{"paint"},
    });
    mixed.layers.emplace_back(VectorLayer{
        {"vector-layer", "Vector", true, 1.0, {}, RasterBlendMode::SourceOver},
        StaticSource{"vector"},
    });
    QVERIFY(item.document());
    *item.document() = std::move(mixed);
    QVERIFY(item.refresh());
    QVERIFY(item.selectLayer(QStringLiteral("paint-layer")));

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString rasterPath = dir.filePath(QStringLiteral("replacement.png"));
    QImage replacement(16, 16, QImage::Format_ARGB32);
    replacement.fill(QColor(QStringLiteral("#26c6da")));
    QVERIFY(replacement.save(rasterPath));

    QVERIFY(item.openRaster(rasterPath));
    QVERIFY(item.document());
    QCOMPARE(item.document()->assets.size(), std::size_t{1});
    QCOMPARE(item.document()->layers.size(), std::size_t{1});
    const RasterAsset* opened = findRasterAsset(*item.document(), "canvas.raster.0");
    QVERIFY(opened);
    QCOMPARE(opened->pixels.pixels[8 * 16 + 8], QColor(QStringLiteral("#26c6da")).rgba());
    QTRY_VERIFY_WITH_TIMEOUT(item.framePixels() && item.framePixels()->pixels[8 * 16 + 8] ==
                                                       QColor(QStringLiteral("#26c6da")).rgba(),
                             5000);
    QTRY_VERIFY_WITH_TIMEOUT(!item.rendering(), 5000);
}

void tst_DrawingSurfaceItem::roundTripsNativeSharedCanvasDocument()
{
    using namespace iiSharedCanvas;

    PaletteUtils paletteUtils;
    CanvasDocumentViewModel viewModel(&paletteUtils);
    DrawingSurfaceItem item;
    item.setWidth(32);
    item.setHeight(24);
    item.setDocumentViewModel(&viewModel);
    item.setBrushColor(QColor(QStringLiteral("#26c6da")));
    item.setBrushSize(6);
    item.beginStroke(4, 12, 1.0, false);
    QVERIFY(item.appendStrokePoint(28, 12, 1.0, false));
    item.endStroke(28, 12, 1.0, false);

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString nativePath = dir.filePath(QStringLiteral("canvas.iisc"));
    QVERIFY(item.saveToFile(nativePath));

    const QByteArray bytes = iiFileProvider::File::read(nativePath);
    {
        QLockFile competingWriter(nativePath + QStringLiteral(".iisacc-lock"));
        QVERIFY(competingWriter.tryLock(0));
        QVERIFY(!item.saveToFile(nativePath));
        QCOMPARE(iiFileProvider::File::read(nativePath), bytes);
    }
    QVERIFY(bytes.startsWith("IISC\r\n\x1a\n"));
    const IiscDecodeResult decoded = decodeIisc(
        std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(bytes.constData()),
                                      static_cast<std::size_t>(bytes.size())));
    QVERIFY2(decoded.ok(), decoded.error.message.c_str());
    QCOMPARE(decoded.document.extent.width, 32);
    QCOMPARE(decoded.document.extent.height, 24);

    DrawingSurfaceItem opened;
    opened.setWidth(1);
    opened.setHeight(1);
    opened.setDocumentViewModel(&viewModel);
    QVERIFY(opened.openRaster(nativePath));
    QVERIFY(opened.document());
    QVERIFY(!opened.document()->layers.empty());
    const auto& openedLayer = opened.document()->layers.front();
    QVERIFY(std::holds_alternative<iiSharedCanvas::BitmapLayer>(openedLayer));
    QCOMPARE(opened.selectedLayerId(),
             QString::fromStdString(iiSharedCanvas::layerProperties(openedLayer).id));
    QVERIFY(opened.selectedRasterPixels());
    QCOMPARE(opened.width(), 32.0);
    QCOMPARE(opened.height(), 24.0);

    const QString pngPath = dir.filePath(QStringLiteral("roundtrip.png"));
    QVERIFY(opened.saveToFile(pngPath));
    const QImage rendered(pngPath);
    QVERIFY(!rendered.isNull());
    QVERIFY(qAlpha(rendered.pixel(16, 12)) > 0);
    QVERIFY(iiFileProvider::File::remove(nativePath));
    QVERIFY(!opened.openRaster(nativePath));
    QCOMPARE(opened.width(), 32.0);
    QCOMPARE(opened.height(), 24.0);
}

void tst_DrawingSurfaceItem::roundTripsRecentCanvasContainerWithEditableObjects()
{
    PaletteUtils paletteUtils;
    CanvasDocumentViewModel viewModel(&paletteUtils);
    DrawingSurfaceItem background;
    background.setWidth(32);
    background.setHeight(24);
    background.setDocumentViewModel(&viewModel);
    background.setBrushColor(QColor(QStringLiteral("#26c6da")));
    background.setBrushSize(6);
    background.beginStroke(4, 12, 1.0, false);
    QVERIFY(background.appendStrokePoint(28, 12, 1.0, false));
    background.endStroke(28, 12, 1.0, false);

    DrawingSurfaceItem rasterLayer;
    rasterLayer.setWidth(32);
    rasterLayer.setHeight(24);
    rasterLayer.setDocumentViewModel(&viewModel);
    rasterLayer.setBrushColor(QColor(QStringLiteral("#ff3366")));
    rasterLayer.setBrushSize(4);
    rasterLayer.beginStroke(16, 4, 1.0, false);
    rasterLayer.endStroke(16, 20, 1.0, false);

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString sourceImagePath = directory.filePath(QStringLiteral("inserted.png"));
    QImage sourceImage(6, 5, QImage::Format_ARGB32_Premultiplied);
    sourceImage.fill(QColor(QStringLiteral("#43a047")));
    QVERIFY(sourceImage.save(sourceImagePath));

    QVariantList objects;
    objects.append(QVariantMap{
        {QStringLiteral("id"), 1},
        {QStringLiteral("type"), QStringLiteral("layer")},
        {QStringLiteral("name"), QStringLiteral("Ink")},
        {QStringLiteral("x"), 0},
        {QStringLiteral("y"), 0},
        {QStringLiteral("width"), 32},
        {QStringLiteral("height"), 24},
        {QStringLiteral("opacity"), 1.0},
        {QStringLiteral("visible"), true},
    });
    objects.append(QVariantMap{
        {QStringLiteral("id"), 2},
        {QStringLiteral("type"), QStringLiteral("image")},
        {QStringLiteral("name"), QStringLiteral("Reference")},
        {QStringLiteral("source"), QUrl::fromLocalFile(sourceImagePath).toString()},
        {QStringLiteral("originalSource"), QUrl::fromLocalFile(sourceImagePath).toString()},
        {QStringLiteral("x"), 3.0},
        {QStringLiteral("y"), 4.0},
        {QStringLiteral("width"), 6.0},
        {QStringLiteral("height"), 5.0},
        {QStringLiteral("originalWidth"), 6},
        {QStringLiteral("originalHeight"), 5},
        {QStringLiteral("opacity"), 1.0},
        {QStringLiteral("visible"), true},
    });
    objects.append(QVariantMap{
        {QStringLiteral("id"), 3},
        {QStringLiteral("type"), QStringLiteral("text")},
        {QStringLiteral("name"), QStringLiteral("Caption")},
        {QStringLiteral("x"), 2.0},
        {QStringLiteral("y"), 2.0},
        {QStringLiteral("width"), 18.0},
        {QStringLiteral("height"), 8.0},
        {QStringLiteral("text"), QStringLiteral("Recent")},
        {QStringLiteral("fontPixelSize"), 12.0},
        {QStringLiteral("color"), QStringLiteral("#ffffff")},
    });
    objects.append(QVariantMap{
        {QStringLiteral("id"), 4},
        {QStringLiteral("type"), QStringLiteral("shape")},
        {QStringLiteral("name"), QStringLiteral("Marker")},
        {QStringLiteral("x"), 20.0},
        {QStringLiteral("y"), 10.0},
        {QStringLiteral("width"), 8.0},
        {QStringLiteral("height"), 6.0},
        {QStringLiteral("shapeKind"), QStringLiteral("ellipse")},
        {QStringLiteral("color"), QStringLiteral("#ffcc00")},
    });
    const QVariantList rasterLayers{QVariantMap{
        {QStringLiteral("objectId"), 1},
        {QStringLiteral("item"), QVariant::fromValue<QObject*>(&rasterLayer)},
    }};

    const QString recentDirectory = directory.filePath(QStringLiteral("canvas"));
    const QString recentPath = QDir(recentDirectory).filePath(QStringLiteral("recent-canvas.vrc"));
    QVERIFY(background.saveRecentCanvas(recentPath, objects, rasterLayers, true));
    QVERIFY(QFileInfo(recentPath).size() > 0);
    QFile firstSnapshot(recentPath);
    QVERIFY(firstSnapshot.open(QIODevice::ReadOnly));
    const QByteArray firstSnapshotBytes = firstSnapshot.readAll();
    QVERIFY(firstSnapshotBytes.startsWith("CONGREGATIONRC\r\n\x1a\n"));
    firstSnapshot.close();

    {
        QLockFile competingWriter(recentPath + QStringLiteral(".iisacc-lock"));
        QVERIFY(competingWriter.tryLock(0));
        QVERIFY(!background.saveRecentCanvas(recentPath, objects, rasterLayers, false));
        QCOMPARE(iiFileProvider::File::read(recentPath), firstSnapshotBytes);
    }

    QVERIFY(background.saveRecentCanvas(recentPath, objects, rasterLayers, false));
    QFile latestSnapshot(recentPath);
    QVERIFY(latestSnapshot.open(QIODevice::ReadOnly));
    const QByteArray latestSnapshotBytes = latestSnapshot.readAll();
    QVERIFY(latestSnapshotBytes != firstSnapshotBytes);
    QCOMPARE(
        QDir(recentDirectory).entryList(QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot).size(),
        1);

    QString extractionDirectory;
    {
        DrawingSurfaceItem opened;
        opened.setWidth(1);
        opened.setHeight(1);
        opened.setDocumentViewModel(&viewModel);
        const QVariantMap restored = opened.openRecentCanvas(recentPath);
        QVERIFY(restored.value(QStringLiteral("valid")).toBool());
        QVERIFY(opened.document());
        QVERIFY(!opened.document()->layers.empty());
        const auto& openedLayer = opened.document()->layers.front();
        QVERIFY(std::holds_alternative<iiSharedCanvas::BitmapLayer>(openedLayer));
        QCOMPARE(opened.selectedLayerId(),
                 QString::fromStdString(iiSharedCanvas::layerProperties(openedLayer).id));
        QVERIFY(opened.selectedRasterPixels());
        QVERIFY(!restored.value(QStringLiteral("backgroundLayerPresent")).toBool());
        QCOMPARE(opened.width(), 32.0);
        QCOMPARE(opened.height(), 24.0);

        const QVariantList restoredObjects =
            restored.value(QStringLiteral("drawableObjects")).toList();
        QCOMPARE(restoredObjects.size(), 4);
        QCOMPARE(restoredObjects.at(0).toMap().value(QStringLiteral("type")).toString(),
                 QStringLiteral("layer"));
        const QString layerSnapshot =
            restoredObjects.at(0).toMap().value(QStringLiteral("snapshotSource")).toString();
        QVERIFY(QUrl(layerSnapshot).isLocalFile());
        QVERIFY(!QImage(QUrl(layerSnapshot).toLocalFile()).isNull());
        extractionDirectory = QFileInfo(QUrl(layerSnapshot).toLocalFile()).absolutePath();
        QCOMPARE(restoredObjects.at(1).toMap().value(QStringLiteral("type")).toString(),
                 QStringLiteral("image"));
        const QString restoredImageSource =
            restoredObjects.at(1).toMap().value(QStringLiteral("source")).toString();
        QVERIFY(QUrl(restoredImageSource).isLocalFile());
        QCOMPARE(QFileInfo(QUrl(restoredImageSource).toLocalFile()).absolutePath(),
                 extractionDirectory);
        QCOMPARE(QImage(QUrl(restoredImageSource).toLocalFile()).pixelColor(0, 0),
                 QColor(QStringLiteral("#43a047")));
        QCOMPARE(restoredObjects.at(2).toMap().value(QStringLiteral("text")).toString(),
                 QStringLiteral("Recent"));
        QCOMPARE(restoredObjects.at(3).toMap().value(QStringLiteral("shapeKind")).toString(),
                 QStringLiteral("ellipse"));

        const QString renderedPath = directory.filePath(QStringLiteral("restored-background.png"));
        QVERIFY(opened.saveToFile(renderedPath));
        const QImage rendered(renderedPath);
        QVERIFY(!rendered.isNull());
        QVERIFY(qAlpha(rendered.pixel(16, 12)) > 0);
        QVERIFY(QFileInfo::exists(extractionDirectory));
    }
    QVERIFY(!QFileInfo::exists(extractionDirectory));
}

void tst_DrawingSurfaceItem::recentCanvasPreservesVisualCanvasExtentAfterLateResize()
{
    PaletteUtils sourcePaletteUtils;
    CanvasDocumentViewModel sourceViewModel(&sourcePaletteUtils);
    DrawingSurfaceItem source;
    source.setWidth(1);
    source.setHeight(1);
    source.setDocumentViewModel(&sourceViewModel);
    source.resizeCanvasSurface(64, 48);
    QCOMPARE(source.width(), 64.0);
    QCOMPARE(source.height(), 48.0);

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString recentPath = directory.filePath(QStringLiteral("recent-canvas.vrc"));
    QVERIFY(source.saveRecentCanvas(recentPath, {}, {}, true));
    const QByteArray authorshipBeforeExport = source.document()->authorship.dump();
    const QByteArray inMemorySnapshot = source.exportCanvasSession({}, {}, true);
    QTest::qWait(20); // Cross the ledger's millisecond timestamp boundary.
    QCOMPARE(source.exportCanvasSession({}, {}, true), inMemorySnapshot);
    QCOMPARE(source.document()->authorship.dump(), authorshipBeforeExport);
    QVERIFY(!inMemorySnapshot.isEmpty());

    PaletteUtils memoryPaletteUtils;
    CanvasDocumentViewModel memoryViewModel(&memoryPaletteUtils);
    DrawingSurfaceItem memoryRestored;
    memoryRestored.setWidth(1);
    memoryRestored.setHeight(1);
    memoryRestored.setDocumentViewModel(&memoryViewModel);
    const QVariantMap memorySession = memoryRestored.importCanvasSession(inMemorySnapshot);
    QVERIFY(memorySession.value(QStringLiteral("valid")).toBool());
    QCOMPARE(memoryRestored.width(), 64.0);
    QCOMPARE(memoryRestored.height(), 48.0);

    PaletteUtils restoredPaletteUtils;
    CanvasDocumentViewModel restoredViewModel(&restoredPaletteUtils);
    DrawingSurfaceItem restored;
    restored.setWidth(1);
    restored.setHeight(1);
    restored.setDocumentViewModel(&restoredViewModel);
    const QVariantMap session = restored.openRecentCanvas(recentPath);
    QVERIFY(session.value(QStringLiteral("valid")).toBool());
    QCOMPARE(restored.width(), 64.0);
    QCOMPARE(restored.height(), 48.0);
}

void tst_DrawingSurfaceItem::roundTripsRecentCanvasThroughQmlSurface()
{
    qmlRegisterType<DrawingSurfaceItem>("Congregation", 1, 0, "DrawingSurfaceItem");

    QQmlEngine engine;
    QQmlComponent component(&engine);
    const QString drawingSurfaceQml = QFINDTESTDATA("../App/qml/painting/DrawingSurface.qml");
    QVERIFY2(!drawingSurfaceQml.isEmpty(), "DrawingSurface.qml test data was not found");
    component.loadUrl(QUrl::fromLocalFile(drawingSurfaceQml));
    QTRY_VERIFY(component.isReady() || component.isError());
    QVERIFY2(component.isReady(), qPrintable(qmlErrorsToString(component.errors())));

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString recentPath = directory.filePath(QStringLiteral("recent-canvas.vrc"));

    PaletteUtils sourcePaletteUtils;
    CanvasDocumentViewModel sourceViewModel(&sourcePaletteUtils);
    QVariantMap sourceProperties;
    sourceProperties.insert(QStringLiteral("width"), 500);
    sourceProperties.insert(QStringLiteral("height"), 360);
    sourceProperties.insert(QStringLiteral("documentViewModel"),
                            QVariant::fromValue(static_cast<QObject*>(&sourceViewModel)));
    sourceProperties.insert(QStringLiteral("brushColor"), QColor(QStringLiteral("#1976d2")));
    sourceProperties.insert(QStringLiteral("brushSize"), 10);
    sourceProperties.insert(QStringLiteral("toolMode"), QStringLiteral("brush"));

    QScopedPointer<QObject> sourceObject(component.createWithInitialProperties(sourceProperties));
    QVERIFY2(!sourceObject.isNull(), qPrintable(qmlErrorsToString(component.errors())));
    QTRY_VERIFY(
        [&]()
        {
            QQmlExpression layerReady(engine.rootContext(), sourceObject.data(),
                                      QStringLiteral("rasterLayerItemById(1) !== null"));
            const QVariant result = layerReady.evaluate();
            return !layerReady.hasError() && result.toBool();
        }());

    QQmlExpression paintLayer(engine.rootContext(), sourceObject.data(),
                              QStringLiteral("var rasterSurface = rasterLayerItemById(1);"
                                             "rasterSurface.beginStroke(0, 0, 1.0, false);"
                                             "rasterSurface.endStroke(0, 0, 1.0, false);"));
    paintLayer.evaluate();
    QVERIFY2(!paintLayer.hasError(), qPrintable(paintLayer.error().toString()));
    QCOMPARE(sourceObject->property("drawableObjects").toList().size(), 1);

    QQmlExpression saveRecent(engine.rootContext(), sourceObject.data(),
                              QStringLiteral("saveRecentCanvas(\"%1\");")
                                  .arg(QUrl::fromLocalFile(recentPath).toString()));
    const QVariant saved = saveRecent.evaluate();
    QVERIFY2(!saveRecent.hasError(), qPrintable(saveRecent.error().toString()));
    QVERIFY(saved.toBool());
    sourceObject.reset();

    PaletteUtils restoredPaletteUtils;
    CanvasDocumentViewModel restoredViewModel(&restoredPaletteUtils);
    QVariantMap restoredProperties;
    restoredProperties.insert(QStringLiteral("width"), 500);
    restoredProperties.insert(QStringLiteral("height"), 360);
    restoredProperties.insert(QStringLiteral("documentViewModel"),
                              QVariant::fromValue(static_cast<QObject*>(&restoredViewModel)));

    QScopedPointer<QObject> restoredObject(
        component.createWithInitialProperties(restoredProperties));
    QVERIFY2(!restoredObject.isNull(), qPrintable(qmlErrorsToString(component.errors())));
    QQmlExpression openRecent(engine.rootContext(), restoredObject.data(),
                              QStringLiteral("openRecentCanvas(\"%1\");")
                                  .arg(QUrl::fromLocalFile(recentPath).toString()));
    const QVariant opened = openRecent.evaluate();
    QVERIFY2(!openRecent.hasError(), qPrintable(openRecent.error().toString()));
    QVERIFY(opened.toBool());

    QTRY_VERIFY(
        [&]()
        {
            QQmlExpression layerReady(engine.rootContext(), restoredObject.data(),
                                      QStringLiteral("rasterLayerItemById(1) !== null"));
            const QVariant result = layerReady.evaluate();
            return !layerReady.hasError() && result.toBool();
        }());

    QQmlExpression restoredSnapshot(
        engine.rootContext(), restoredObject.data(),
        QStringLiteral("rasterLayerItemById(1).cacheRasterSnapshotSource();"));
    const QString restoredSnapshotUrl = restoredSnapshot.evaluate().toString();
    QVERIFY2(!restoredSnapshot.hasError(), qPrintable(restoredSnapshot.error().toString()));
    QVERIFY(QUrl(restoredSnapshotUrl).isLocalFile());
    const QImage rendered(QUrl(restoredSnapshotUrl).toLocalFile());
    QVERIFY(!rendered.isNull());
    const QColor restoredPixel = rendered.pixelColor(0, 0);
    QVERIFY(restoredPixel.alpha() > 0);
}

void tst_DrawingSurfaceItem::rejectsCorruptRecentCanvasWithoutReplacingTheCurrentDocument()
{
    PaletteUtils paletteUtils;
    CanvasDocumentViewModel viewModel(&paletteUtils);
    DrawingSurfaceItem item;
    item.setWidth(16);
    item.setHeight(12);
    item.setDocumentViewModel(&viewModel);
    item.setBrushColor(QColor(QStringLiteral("#26c6da")));
    item.beginStroke(8, 6, 1.0, false);
    item.endStroke(8, 6, 1.0, false);

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString corruptPath = directory.filePath(QStringLiteral("corrupt.vrc"));
    QVERIFY(item.saveRecentCanvas(corruptPath, {}, {}, true));

    item.setBrushColor(QColor(QStringLiteral("#ef5350")));
    item.beginStroke(2, 2, 1.0, false);
    item.endStroke(2, 2, 1.0, false);
    QVERIFY(item.document());
    const iiSharedCanvas::IiscEncodeResult documentBefore =
        iiSharedCanvas::encodeIisc(*item.document());
    QVERIFY(documentBefore.ok());

    QFile corrupt(corruptPath);
    QVERIFY(corrupt.open(QIODevice::ReadWrite));
    QVERIFY(corrupt.size() > 1);
    const qint64 lastByteOffset = corrupt.size() - 1;
    QVERIFY(corrupt.seek(lastByteOffset));
    char lastByte = 0;
    QCOMPARE(corrupt.read(&lastByte, 1), qint64{1});
    lastByte ^= 0x01;
    QVERIFY(corrupt.seek(lastByteOffset));
    QCOMPARE(corrupt.write(&lastByte, 1), qint64{1});
    QVERIFY(corrupt.flush());
    corrupt.close();

    const QVariantMap restored = item.openRecentCanvas(corruptPath);
    QVERIFY(!restored.value(QStringLiteral("valid")).toBool());
    QVERIFY(item.document());
    const iiSharedCanvas::IiscEncodeResult documentAfter =
        iiSharedCanvas::encodeIisc(*item.document());
    QVERIFY(documentAfter.ok());
    QVERIFY(documentAfter.bytes == documentBefore.bytes);
    QCOMPARE(item.width(), 16.0);
    QCOMPARE(item.height(), 12.0);
}

void tst_DrawingSurfaceItem::pressureSensitiveManualStrokesPreserveInputPressure()
{
    PaletteUtils paletteUtils;
    CanvasDocumentViewModel viewModel(&paletteUtils);
    DrawingSurfaceItem item;
    item.setWidth(96);
    item.setHeight(64);
    item.setMultithreadedEventsEnabled(false);
    item.setDocumentViewModel(&viewModel);

    QSignalSpy inputChanged(&item, &DrawingSurfaceItem::inputStateChanged);

    item.beginStroke(24, 24, 0.25, true);
    QCOMPARE(item.inputDevice(), QStringLiteral("tablet"));
    QVERIFY(qAbs(item.inputPressure() - 0.25) < 0.0001);
    QVERIFY(inputChanged.count() > 0);

    item.endStroke(24, 24, 0.25, true);
    QTRY_COMPARE(item.strokeCount(), 1);
}

void tst_DrawingSurfaceItem::erasesCommittedStrokePixels()
{
    PaletteUtils paletteUtils;
    CanvasDocumentViewModel viewModel(&paletteUtils);
    DrawingSurfaceItem item;
    item.setWidth(96);
    item.setHeight(64);
    item.setBrushSize(20);
    item.setBrushColor(QColor(QStringLiteral("#202020")));
    item.setDocumentViewModel(&viewModel);

    item.beginStroke(16, 32, 1.0, false);
    QVERIFY(item.appendStrokePoint(80, 32, 1.0, false));
    item.endStroke(80, 32, 1.0, false);
    QTRY_COMPARE(item.strokeCount(), 1);

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString beforePath = dir.filePath(QStringLiteral("before-erase.png"));
    QVERIFY(item.saveToFile(beforePath));
    const QImage before(beforePath);
    QVERIFY(qAlpha(before.pixel(48, 32)) > 0);

    item.setToolMode(QStringLiteral("eraser"));
    item.beginStroke(16, 32, 1.0, false);
    QVERIFY(item.appendStrokePoint(80, 32, 1.0, false));
    item.endStroke(80, 32, 1.0, false);
    QTRY_COMPARE(item.strokeCount(), 2);

    const QString afterPath = dir.filePath(QStringLiteral("after-erase.png"));
    QVERIFY(item.saveToFile(afterPath));
    const QImage after(afterPath);
    QVERIFY(qAlpha(after.pixel(48, 32)) < qAlpha(before.pixel(48, 32)));
}

void tst_DrawingSurfaceItem::commitsTextToRasterCanvas()
{
    PaletteUtils paletteUtils;
    CanvasDocumentViewModel viewModel(&paletteUtils);
    DrawingSurfaceItem item;
    item.setWidth(180);
    item.setHeight(96);
    item.setBrushColor(QColor(QStringLiteral("#d32f2f")));
    item.setDocumentViewModel(&viewModel);

    QVERIFY(item.commitText(16, 18, 140, QStringLiteral("Congregation"), 28,
                            QColor(QStringLiteral("#d32f2f"))));

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString outputPath = dir.filePath(QStringLiteral("text-output.png"));
    QVERIFY(item.saveToFile(outputPath));
    const QImage saved(outputPath);
    QVERIFY(!saved.isNull());
    QCOMPARE(saved.size(), QSize(180, 96));

    bool hasTextColorPixel = false;
    for (int y = 0; y < saved.height() && !hasTextColorPixel; ++y)
    {
        for (int x = 0; x < saved.width(); ++x)
        {
            const QColor pixel = saved.pixelColor(x, y);
            if (pixel.alpha() > 0 && pixel.red() > 140 && pixel.green() < 100 && pixel.blue() < 100)
            {
                hasTextColorPixel = true;
                break;
            }
        }
    }
    QVERIFY(hasTextColorPixel);
}

void tst_DrawingSurfaceItem::commitsShapeToRasterCanvas()
{
    PaletteUtils paletteUtils;
    CanvasDocumentViewModel viewModel(&paletteUtils);
    DrawingSurfaceItem item;
    item.setWidth(180);
    item.setHeight(120);
    item.setBrushColor(QColor(QStringLiteral("#1976d2")));
    item.setDocumentViewModel(&viewModel);

    const QStringList shapeKinds{QStringLiteral("rectangle"),    QStringLiteral("ellipse"),
                                 QStringLiteral("triangle"),     QStringLiteral("diamond"),
                                 QStringLiteral("star"),         QStringLiteral("rectanglebubble"),
                                 QStringLiteral("ellipsebubble")};

    for (int index = 0; index < shapeKinds.size(); ++index)
    {
        const qreal x = 10 + index % 4 * 40;
        const qreal y = 12 + index / 4 * 48;
        QVERIFY(item.commitShape(x, y, 30, 28, shapeKinds.at(index),
                                 QColor(QStringLiteral("#1976d2"))));
    }

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString outputPath = dir.filePath(QStringLiteral("shape-output.png"));
    QVERIFY(item.saveToFile(outputPath));
    const QImage saved(outputPath);
    QVERIFY(!saved.isNull());
    QCOMPARE(saved.size(), QSize(180, 120));

    bool hasShapeColorPixel = false;
    for (int y = 0; y < saved.height() && !hasShapeColorPixel; ++y)
    {
        for (int x = 0; x < saved.width(); ++x)
        {
            const QColor pixel = saved.pixelColor(x, y);
            if (pixel.alpha() > 0 && pixel.blue() > 120 && pixel.red() < 80 && pixel.green() > 70)
            {
                hasShapeColorPixel = true;
                break;
            }
        }
    }
    QVERIFY(hasShapeColorPixel);
    const QColor filledRectangleCenter = saved.pixelColor(25, 26);
    QVERIFY(filledRectangleCenter.alpha() > 0);
    QVERIFY(filledRectangleCenter.blue() > 120);
    QVERIFY(filledRectangleCenter.red() < 80);
    QVERIFY(filledRectangleCenter.green() > 70);
}

void tst_DrawingSurfaceItem::commitsSpeechBubbleTailsAsIntegratedSolidShapes()
{
    PaletteUtils paletteUtils;
    CanvasDocumentViewModel viewModel(&paletteUtils);
    DrawingSurfaceItem item;
    item.setWidth(180);
    item.setHeight(96);
    item.setDocumentViewModel(&viewModel);

    const QColor shapeColor(QStringLiteral("#1976d2"));
    QVERIFY(item.commitShape(16, 16, 64, 56, QStringLiteral("rectanglebubble"), shapeColor));
    QVERIFY(item.commitShape(96, 16, 64, 56, QStringLiteral("ellipsebubble"), shapeColor));

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString outputPath = dir.filePath(QStringLiteral("speech-bubble-output.png"));
    QVERIFY(item.saveToFile(outputPath));
    const QImage saved(outputPath);
    QVERIFY(!saved.isNull());
    QCOMPARE(saved.size(), QSize(180, 96));

    QVERIFY(isBlueShapePixel(saved.pixelColor(36, 62)));
    QVERIFY(isBlueShapePixel(saved.pixelColor(116, 59)));
    QVERIFY(isBlueShapePixel(saved.pixelColor(108, 68)));
}

void tst_DrawingSurfaceItem::fillsContiguousRasterRegion()
{
    PaletteUtils paletteUtils;
    CanvasDocumentViewModel viewModel(&paletteUtils);
    viewModel.setBrushSize(5);
    viewModel.setBrushColor(QColor(QStringLiteral("#101010")));
    DrawingSurfaceItem item;
    item.setWidth(96);
    item.setHeight(64);
    item.setDocumentViewModel(&viewModel);

    item.beginStroke(48, 0, 1.0, false);
    QVERIFY(item.appendStrokePoint(48, 63, 1.0, false));
    item.endStroke(48, 63, 1.0, false);
    QTRY_COMPARE(item.strokeCount(), 1);

    const QColor fillColor(QStringLiteral("#43a047"));
    QVERIFY(item.fillAt(12, 12, fillColor));

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString outputPath = dir.filePath(QStringLiteral("fill-output.png"));
    QVERIFY(item.saveToFile(outputPath));
    const QImage saved(outputPath);
    QVERIFY(!saved.isNull());
    QCOMPARE(saved.size(), QSize(96, 64));
    QCOMPARE(saved.pixelColor(12, 12).rgba(), fillColor.rgba());
    QCOMPARE(saved.pixelColor(84, 12).alpha(), 0);
    QVERIFY(saved.pixelColor(48, 32).alpha() > 0);
}

void tst_DrawingSurfaceItem::cachesLayerBitmapThumbnails()
{
    PaletteUtils paletteUtils;
    CanvasDocumentViewModel viewModel(&paletteUtils);
    DrawingSurfaceItem item;
    QVERIFY(item.cacheGrabbedThumbnailSource(nullptr).isEmpty());
    item.setWidth(96);
    item.setHeight(64);
    item.setDocumentViewModel(&viewModel);

    QSignalSpy contentChanged(&item, &DrawingSurfaceItem::rasterContentChanged);
    QVERIFY(item.commitShape(12, 10, 36, 28, QStringLiteral("rectangle"),
                             QColor(QStringLiteral("#1976d2"))));
    QVERIFY(contentChanged.count() > 0);

    const QString rasterThumbnailSource = item.cacheRasterThumbnailSource(32, 32);
    QVERIFY(!rasterThumbnailSource.isEmpty());
    const QString rasterThumbnailPath = QUrl(rasterThumbnailSource).toLocalFile();
    QVERIFY(QFileInfo::exists(rasterThumbnailPath));
    const QImage rasterThumbnail(rasterThumbnailPath);
    QVERIFY(!rasterThumbnail.isNull());
    QCOMPARE(rasterThumbnail.size(), QSize(32, 32));

    QVariantMap shapeObject;
    shapeObject.insert(QStringLiteral("id"), 1);
    shapeObject.insert(QStringLiteral("type"), QStringLiteral("shape"));
    shapeObject.insert(QStringLiteral("x"), 8);
    shapeObject.insert(QStringLiteral("y"), 6);
    shapeObject.insert(QStringLiteral("width"), 40);
    shapeObject.insert(QStringLiteral("height"), 30);
    shapeObject.insert(QStringLiteral("shapeKind"), QStringLiteral("ellipse"));
    shapeObject.insert(QStringLiteral("color"), QStringLiteral("#1976d2"));

    const QString objectThumbnailSource =
        item.cacheDrawableObjectThumbnailSource(shapeObject, 32, 32);
    QVERIFY(!objectThumbnailSource.isEmpty());
    const QString objectThumbnailPath = QUrl(objectThumbnailSource).toLocalFile();
    QVERIFY(QFileInfo::exists(objectThumbnailPath));
    const QImage objectThumbnail(objectThumbnailPath);
    QVERIFY(!objectThumbnail.isNull());
    QCOMPARE(objectThumbnail.size(), QSize(32, 32));

    bool hasObjectColorPixel = false;
    for (int y = 0; y < objectThumbnail.height() && !hasObjectColorPixel; ++y)
    {
        for (int x = 0; x < objectThumbnail.width(); ++x)
        {
            const QColor pixel = objectThumbnail.pixelColor(x, y);
            if (pixel.alpha() > 0 && pixel.blue() > 120 && pixel.red() < 80 && pixel.green() > 70)
            {
                hasObjectColorPixel = true;
                break;
            }
        }
    }
    QVERIFY(hasObjectColorPixel);
}

void tst_DrawingSurfaceItem::savesCompositeDrawableObjectsWithoutFlatteningRaster()
{
    PaletteUtils paletteUtils;
    CanvasDocumentViewModel viewModel(&paletteUtils);
    DrawingSurfaceItem item;
    item.setWidth(120);
    item.setHeight(90);
    item.setDocumentViewModel(&viewModel);

    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    QVariantMap shapeObject;
    shapeObject.insert(QStringLiteral("id"), 1);
    shapeObject.insert(QStringLiteral("type"), QStringLiteral("shape"));
    shapeObject.insert(QStringLiteral("x"), 16);
    shapeObject.insert(QStringLiteral("y"), 18);
    shapeObject.insert(QStringLiteral("width"), 48);
    shapeObject.insert(QStringLiteral("height"), 36);
    shapeObject.insert(QStringLiteral("shapeKind"), QStringLiteral("rectangle"));
    shapeObject.insert(QStringLiteral("color"), QStringLiteral("#1976d2"));

    const QColor imageObjectColor(QStringLiteral("#f4511e"));
    QImage imageObjectRaster(18, 14, QImage::Format_ARGB32);
    imageObjectRaster.fill(imageObjectColor);
    const QString imageObjectPath = dir.filePath(QStringLiteral("image-object.png"));
    QVERIFY(imageObjectRaster.save(imageObjectPath));

    QVariantMap imageObject;
    imageObject.insert(QStringLiteral("id"), 2);
    imageObject.insert(QStringLiteral("type"), QStringLiteral("image"));
    imageObject.insert(QStringLiteral("x"), 72);
    imageObject.insert(QStringLiteral("y"), 24);
    imageObject.insert(QStringLiteral("width"), 18);
    imageObject.insert(QStringLiteral("height"), 14);
    imageObject.insert(QStringLiteral("source"), QUrl::fromLocalFile(imageObjectPath).toString());

    QVariantList objects;
    objects.append(shapeObject);
    objects.append(imageObject);

    const QString compositePath = dir.filePath(QStringLiteral("composite-output.png"));
    QVERIFY(item.saveToFileWithObjects(compositePath, objects));

    const QImage composite(compositePath);
    QVERIFY(!composite.isNull());
    QCOMPARE(composite.size(), QSize(120, 90));

    bool hasShapeColorPixel = false;
    for (int y = 0; y < composite.height() && !hasShapeColorPixel; ++y)
    {
        for (int x = 0; x < composite.width(); ++x)
        {
            const QColor pixel = composite.pixelColor(x, y);
            if (pixel.alpha() > 0 && pixel.blue() > 120 && pixel.red() < 80 && pixel.green() > 70)
            {
                hasShapeColorPixel = true;
                break;
            }
        }
    }
    QVERIFY(hasShapeColorPixel);
    const QColor filledShapeCenter = composite.pixelColor(40, 36);
    QVERIFY(filledShapeCenter.alpha() > 0);
    QVERIFY(filledShapeCenter.blue() > 120);
    QVERIFY(filledShapeCenter.red() < 80);
    QVERIFY(filledShapeCenter.green() > 70);
    QCOMPARE(composite.pixelColor(76, 28).rgba(), imageObjectColor.rgba());

    const QString rasterOnlyPath = dir.filePath(QStringLiteral("raster-only-output.png"));
    QVERIFY(item.saveToFile(rasterOnlyPath));
    const QImage rasterOnly(rasterOnlyPath);
    QVERIFY(!rasterOnly.isNull());
    QVERIFY(rasterOnly.pixelColor(16, 18).alpha() == 0);
    QVERIFY(rasterOnly.pixelColor(76, 28).alpha() == 0);
}

void tst_DrawingSurfaceItem::savesBlankCanvasAsOpaquePsdBackground()
{
    PaletteUtils paletteUtils;
    CanvasDocumentViewModel viewModel(&paletteUtils);
    DrawingSurfaceItem item;
    item.setWidth(8);
    item.setHeight(6);
    item.setDocumentViewModel(&viewModel);
    item.newCanvas();

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString psdPath = dir.filePath(QStringLiteral("blank-canvas.psd"));
    QVERIFY(item.saveToFile(psdPath));

    const PsdImportedDocument importedDocument = PsdImageReader::readDocument(psdPath);
    QVERIFY(importedDocument.isValid());
    QCOMPARE(importedDocument.canvasSize, QSize(8, 6));
    QVERIFY(!importedDocument.mergedImage.isNull());
    QCOMPARE(importedDocument.layers.size(), 1);
    QCOMPARE(importedDocument.layers.at(0).name, QStringLiteral("Background"));
    QCOMPARE(importedDocument.layers.at(0).bounds, QRect(0, 0, 8, 6));

    const QColor expectedCanvasBackground(Qt::white);
    QCOMPARE(importedDocument.mergedImage.pixelColor(0, 0).rgba(), expectedCanvasBackground.rgba());
    QCOMPARE(importedDocument.mergedImage.pixelColor(7, 5).rgba(), expectedCanvasBackground.rgba());
    QCOMPARE(importedDocument.layers.at(0).image.pixelColor(0, 0).rgba(),
             expectedCanvasBackground.rgba());
    QCOMPARE(importedDocument.layers.at(0).image.pixelColor(7, 5).rgba(),
             expectedCanvasBackground.rgba());
}

void tst_DrawingSurfaceItem::savesPsdLayerRecordsInBottomToTopOrder()
{
    PaletteUtils paletteUtils;
    CanvasDocumentViewModel viewModel(&paletteUtils);
    DrawingSurfaceItem item;
    item.setWidth(16);
    item.setHeight(12);
    item.setDocumentViewModel(&viewModel);

    QVariantList objects;
    for (int layerIndex = 1; layerIndex <= 4; ++layerIndex)
    {
        QVariantMap layerObject;
        layerObject.insert(QStringLiteral("id"), layerIndex);
        layerObject.insert(QStringLiteral("type"), QStringLiteral("layer"));
        layerObject.insert(QStringLiteral("name"), QStringLiteral("Layer %1").arg(layerIndex));
        layerObject.insert(QStringLiteral("x"), 0);
        layerObject.insert(QStringLiteral("y"), 0);
        layerObject.insert(QStringLiteral("width"), 16);
        layerObject.insert(QStringLiteral("height"), 12);
        objects.append(layerObject);
    }

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString psdPath = dir.filePath(QStringLiteral("ordered-layers.psd"));
    QVERIFY(item.saveToFileWithObjects(psdPath, objects));

    QFile psdFile(psdPath);
    QVERIFY(psdFile.open(QIODevice::ReadOnly));
    QCOMPARE(psdLayerRecordNames(psdFile.readAll()),
             QStringList({QStringLiteral("Background"), QStringLiteral("Layer 1"),
                          QStringLiteral("Layer 2"), QStringLiteral("Layer 3"),
                          QStringLiteral("Layer 4")}));

    const PsdImportedDocument importedDocument = PsdImageReader::readDocument(psdPath);
    QVERIFY(importedDocument.isValid());
    QCOMPARE(importedDocument.layers.size(), 5);
    QCOMPARE(importedDocument.layers.at(0).name, QStringLiteral("Background"));
    QCOMPARE(importedDocument.layers.at(1).name, QStringLiteral("Layer 1"));
    QCOMPARE(importedDocument.layers.at(2).name, QStringLiteral("Layer 2"));
    QCOMPARE(importedDocument.layers.at(3).name, QStringLiteral("Layer 3"));
    QCOMPARE(importedDocument.layers.at(4).name, QStringLiteral("Layer 4"));
}

void tst_DrawingSurfaceItem::savesCompositeDrawableObjectsAsLayeredPsdWithMetadata()
{
    PaletteUtils paletteUtils;
    CanvasDocumentViewModel viewModel(&paletteUtils);
    DrawingSurfaceItem item;
    item.setWidth(20);
    item.setHeight(12);
    item.setDocumentViewModel(&viewModel);

    QVariantMap shapeObject;
    shapeObject.insert(QStringLiteral("id"), 1);
    shapeObject.insert(QStringLiteral("type"), QStringLiteral("shape"));
    shapeObject.insert(QStringLiteral("x"), 2);
    shapeObject.insert(QStringLiteral("y"), 3);
    shapeObject.insert(QStringLiteral("width"), 10);
    shapeObject.insert(QStringLiteral("height"), 6);
    shapeObject.insert(QStringLiteral("shapeKind"), QStringLiteral("rectangle"));
    shapeObject.insert(QStringLiteral("color"), QStringLiteral("#1976d2"));

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString psdPath = dir.filePath(QStringLiteral("composite-output.psd"));
    QVERIFY(item.saveToFileWithObjects(psdPath, {shapeObject}));

    QFile psdFile(psdPath);
    QVERIFY(psdFile.open(QIODevice::ReadOnly));
    const QByteArray psd = psdFile.readAll();
    QVERIFY(psd.size() > 40);
    QCOMPARE(psd.mid(0, 4), QByteArray("8BPS"));
    QCOMPARE(readUInt16(psd, 4), 1);
    QCOMPARE(readUInt16(psd, 12), 4);
    QCOMPARE(readUInt32(psd, 14), 12U);
    QCOMPARE(readUInt32(psd, 18), 20U);
    QCOMPARE(readUInt16(psd, 22), 8);
    QCOMPARE(readUInt16(psd, 24), 3);
    QCOMPARE(readUInt32(psd, 26), 0U);

    const qsizetype resourcesOffset = imageResourcesLengthOffset(psd);
    QVERIFY(resourcesOffset + 4 < psd.size());
    const quint32 imageResourcesLength = readUInt32(psd, static_cast<int>(resourcesOffset));
    QVERIFY(imageResourcesLength > 0);
    QVERIFY(psd.contains("CongregationLayerManifestBase64"));
    QVERIFY(psd.contains("CongregationLayerCount"));
    QVERIFY(psd.contains("base64-json"));

    const qsizetype layerOffset = layerMaskLengthOffset(psd);
    QVERIFY(layerOffset + 10 < psd.size());
    const quint32 layerMaskLength = readUInt32(psd, static_cast<int>(layerOffset));
    QVERIFY(layerMaskLength > 0);
    const quint32 layerInfoLength = readUInt32(psd, static_cast<int>(layerOffset + 4));
    QVERIFY(layerInfoLength > 0);
    QCOMPARE(readUInt16(psd, static_cast<int>(layerOffset + 8)), 2);
    QVERIFY(psd.contains("Background"));
    QVERIFY(psd.contains("Rectangle"));

    const PsdImportedDocument importedDocument = PsdImageReader::readDocument(psdPath);
    QVERIFY(importedDocument.isValid());
    QCOMPARE(importedDocument.canvasSize, QSize(20, 12));
    QCOMPARE(importedDocument.bitsPerChannel, 8);
    QVERIFY(!importedDocument.xmpMetadata.isEmpty());
    QCOMPARE(importedDocument.congregationManifest.value(QStringLiteral("compatibilityVersion")).toInt(),
             1);
    QCOMPARE(importedDocument.congregationManifest.value(QStringLiteral("layers")).toList().size(), 2);
    QCOMPARE(importedDocument.layers.size(), 2);
    QCOMPARE(importedDocument.layers.at(0).name, QStringLiteral("Background"));
    QCOMPARE(importedDocument.layers.at(0).bounds, QRect(0, 0, 20, 12));
    QCOMPARE(importedDocument.layers.at(1).name, QStringLiteral("Rectangle"));
    QCOMPARE(importedDocument.layers.at(1).bounds, QRect(2, 3, 10, 6));
    QCOMPARE(importedDocument.layers.at(1).blendModeKey, QStringLiteral("norm"));
    QCOMPARE(importedDocument.layers.at(1).opacity, 255);
    QVERIFY(importedDocument.layers.at(1).visible);
    QCOMPARE(importedDocument.layers.at(1).image.size(), QSize(20, 12));
    QVERIFY(isBlueShapePixel(importedDocument.layers.at(1).image.pixelColor(4, 4)));
}

void tst_DrawingSurfaceItem::opensLayeredPsdThroughPsdSdkReader()
{
    PaletteUtils paletteUtils;
    CanvasDocumentViewModel viewModel(&paletteUtils);
    DrawingSurfaceItem item;
    item.setWidth(24);
    item.setHeight(16);
    item.setDocumentViewModel(&viewModel);

    QVariantMap shapeObject;
    shapeObject.insert(QStringLiteral("id"), 1);
    shapeObject.insert(QStringLiteral("type"), QStringLiteral("shape"));
    shapeObject.insert(QStringLiteral("x"), 4);
    shapeObject.insert(QStringLiteral("y"), 5);
    shapeObject.insert(QStringLiteral("width"), 12);
    shapeObject.insert(QStringLiteral("height"), 8);
    shapeObject.insert(QStringLiteral("shapeKind"), QStringLiteral("rectangle"));
    shapeObject.insert(QStringLiteral("color"), QStringLiteral("#1976d2"));

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString psdPath = dir.filePath(QStringLiteral("roundtrip.psd"));
    QVERIFY(item.saveToFileWithObjects(psdPath, {shapeObject}));

    DrawingSurfaceItem openedItem;
    openedItem.setWidth(24);
    openedItem.setHeight(16);
    openedItem.setDocumentViewModel(&viewModel);
    QVERIFY(openedItem.openRaster(psdPath));
    QCOMPARE(openedItem.width(), 24.0);
    QCOMPARE(openedItem.height(), 16.0);

    const QString outputPath = dir.filePath(QStringLiteral("roundtrip-output.png"));
    QVERIFY(openedItem.saveToFile(outputPath));
    const QImage opened(outputPath);
    QVERIFY(!opened.isNull());
    QCOMPARE(opened.size(), QSize(24, 16));
    QVERIFY(isBlueShapePixel(opened.pixelColor(8, 8)));
}

void tst_DrawingSurfaceItem::createsPsdDrawablePreviewForQmlImage()
{
    PaletteUtils paletteUtils;
    CanvasDocumentViewModel viewModel(&paletteUtils);
    DrawingSurfaceItem item;
    item.setWidth(32);
    item.setHeight(20);
    item.setDocumentViewModel(&viewModel);

    QVariantMap shapeObject;
    shapeObject.insert(QStringLiteral("id"), 1);
    shapeObject.insert(QStringLiteral("type"), QStringLiteral("shape"));
    shapeObject.insert(QStringLiteral("x"), 0);
    shapeObject.insert(QStringLiteral("y"), 0);
    shapeObject.insert(QStringLiteral("width"), 32);
    shapeObject.insert(QStringLiteral("height"), 20);
    shapeObject.insert(QStringLiteral("shapeKind"), QStringLiteral("rectangle"));
    shapeObject.insert(QStringLiteral("color"), QStringLiteral("#1976d2"));

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString psdPath = dir.filePath(QStringLiteral("preview-source.psd"));
    QVERIFY(item.saveToFileWithObjects(psdPath, {shapeObject}));

    const QVariantMap object = item.imageObjectForFile(psdPath, 128, 128);
    QCOMPARE(object.value(QStringLiteral("sourceFormat")).toString(), QStringLiteral("psd"));
    QCOMPARE(object.value(QStringLiteral("originalSource")).toString(),
             QUrl::fromLocalFile(psdPath).toString());
    QCOMPARE(object.value(QStringLiteral("width")).toInt(), 32);
    QCOMPARE(object.value(QStringLiteral("height")).toInt(), 20);

    const QUrl previewUrl(object.value(QStringLiteral("source")).toString());
    QVERIFY(previewUrl.isLocalFile());
    const QImage preview(previewUrl.toLocalFile());
    QVERIFY(!preview.isNull());
    QCOMPARE(preview.size(), QSize(32, 20));
    QVERIFY(isBlueShapePixel(preview.pixelColor(4, 4)));
}

void tst_DrawingSurfaceItem::supportsUndoRedo()
{
    PaletteUtils paletteUtils;
    CanvasDocumentViewModel viewModel(&paletteUtils);
    DrawingSurfaceItem item;
    item.setWidth(80);
    item.setHeight(80);
    item.setDocumentViewModel(&viewModel);

    item.beginStroke(8, 8, 1.0, false);
    item.endStroke(20, 20, 1.0, false);

    QTRY_COMPARE(item.strokeCount(), 1);
    QTRY_VERIFY(item.canUndo());

    item.undo();
    QTRY_COMPARE(item.strokeCount(), 0);
    QTRY_VERIFY(item.canRedo());

    item.redo();
    QTRY_COMPARE(item.strokeCount(), 1);
}

void tst_DrawingSurfaceItem::opensRasterBackground()
{
    PaletteUtils paletteUtils;
    CanvasDocumentViewModel viewModel(&paletteUtils);
    DrawingSurfaceItem item;
    item.setWidth(120);
    item.setHeight(90);
    item.setDocumentViewModel(&viewModel);

    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    QImage image(32, 24, QImage::Format_ARGB32);
    image.fill(QColor(QStringLiteral("#26c6da")));
    const QString inputPath = dir.filePath(QStringLiteral("background.png"));
    QVERIFY(image.save(inputPath));

    QVERIFY(item.openRaster(inputPath));
    QVERIFY(item.hasBackground());
    QCOMPARE(item.backgroundSource(), QUrl::fromLocalFile(inputPath).toString());
    QCOMPARE(item.width(), 32.0);
    QCOMPARE(item.height(), 24.0);
}

void tst_DrawingSurfaceItem::opensLargeRasterAtOriginalImageSize()
{
    PaletteUtils paletteUtils;
    CanvasDocumentViewModel viewModel(&paletteUtils);
    DrawingSurfaceItem item;
    item.setWidth(320);
    item.setHeight(180);
    item.setDocumentViewModel(&viewModel);

    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    QImage image(1200, 600, QImage::Format_ARGB32);
    image.fill(QColor(QStringLiteral("#26c6da")));
    const QString inputPath = dir.filePath(QStringLiteral("large-background.png"));
    QVERIFY(image.save(inputPath));

    QVERIFY(item.openRaster(inputPath));
    QVERIFY(item.hasBackground());
    QCOMPARE(item.backgroundSource(), QUrl::fromLocalFile(inputPath).toString());
    QCOMPARE(item.width(), 1200.0);
    QCOMPARE(item.height(), 600.0);
    QCOMPARE(viewModel.canvasWidth(), 1200);
    QCOMPARE(viewModel.canvasHeight(), 600);

    const QString outputPath = dir.filePath(QStringLiteral("large-background-output.png"));
    QVERIFY(item.saveToFile(outputPath));
    const QImage saved(outputPath);
    QVERIFY(!saved.isNull());
    QCOMPARE(saved.size(), QSize(1200, 600));
}

void tst_DrawingSurfaceItem::opensRasterImagesAsCanvasAtSourceResolution()
{
    qmlRegisterType<DrawingSurfaceItem>("Congregation", 1, 0, "DrawingSurfaceItem");

    QQmlEngine engine;

    QQmlComponent component(&engine);
    const QString drawingSurfaceQml = QFINDTESTDATA("../App/qml/painting/DrawingSurface.qml");
    QVERIFY2(!drawingSurfaceQml.isEmpty(), "DrawingSurface.qml test data was not found");
    component.loadUrl(QUrl::fromLocalFile(drawingSurfaceQml));
    QTRY_VERIFY(component.isReady() || component.isError());
    QVERIFY2(component.isReady(), qPrintable(qmlErrorsToString(component.errors())));

    PaletteUtils paletteUtils;
    CanvasDocumentViewModel viewModel(&paletteUtils);

    QVariantMap initialProperties;
    initialProperties.insert(QStringLiteral("width"), 720);
    initialProperties.insert(QStringLiteral("height"), 480);
    initialProperties.insert(QStringLiteral("documentViewModel"),
                             QVariant::fromValue(static_cast<QObject*>(&viewModel)));
    initialProperties.insert(QStringLiteral("toolMode"), QStringLiteral("move"));

    QScopedPointer<QObject> object(component.createWithInitialProperties(initialProperties));
    QVERIFY2(!object.isNull(), qPrintable(qmlErrorsToString(component.errors())));
    auto* rootItem = qobject_cast<QQuickItem*>(object.data());
    QVERIFY(rootItem);

    DrawingSurfaceItem* canvasItem = findDrawingSurfaceItem(rootItem);
    QVERIFY(canvasItem);
    const QSize workspaceSize = workspaceCanvasSize(720, 480);
    QTRY_COMPARE(canvasItem->width(), static_cast<qreal>(workspaceSize.width()));
    QTRY_COMPARE(canvasItem->height(), static_cast<qreal>(workspaceSize.height()));

    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    QImage image(1600, 1200, QImage::Format_ARGB32);
    image.fill(QColor(QStringLiteral("#26c6da")));
    const QString inputPath = dir.filePath(QStringLiteral("oversized-open.png"));
    QVERIFY(image.save(inputPath));
    engine.rootContext()->setContextProperty(QStringLiteral("testOpenImageUrl"),
                                             QUrl::fromLocalFile(inputPath).toString());

    QQmlExpression openImage(engine.rootContext(), object.data(),
                             QStringLiteral("openRaster(testOpenImageUrl);"));
    const QVariant openResult = openImage.evaluate();
    QVERIFY2(!openImage.hasError(), qPrintable(openImage.error().toString()));
    QCOMPARE(openResult.toBool(), true);

    QVERIFY(canvasItem->width() > rootItem->width());
    QVERIFY(canvasItem->height() > rootItem->height());
    QTRY_COMPARE(canvasItem->width(), static_cast<qreal>(image.width()));
    QTRY_COMPARE(canvasItem->height(), static_cast<qreal>(image.height()));
    QVERIFY(canvasItem->hasBackground());
    QCOMPARE(canvasItem->backgroundSource(), QUrl::fromLocalFile(inputPath).toString());
    QTRY_COMPARE(rootItem->property("backgroundLayerPresent").toBool(), true);
    QTRY_COMPARE(viewModel.canvasWidth(), image.width());
    QTRY_COMPARE(viewModel.canvasHeight(), image.height());

    QVariantList objects = rootItem->property("drawableObjects").toList();
    QCOMPARE(objects.size(), 1);
    const QVariantMap layerObject = objects.first().toMap();
    QCOMPARE(layerObject.value(QStringLiteral("type")).toString(), QStringLiteral("layer"));
    QCOMPARE(layerObject.value(QStringLiteral("width")).toInt(), image.width());
    QCOMPARE(layerObject.value(QStringLiteral("height")).toInt(), image.height());
    QCOMPARE(rootItem->property("selectedDrawableObjectId").toInt(),
             layerObject.value(QStringLiteral("id")).toInt());

    QImage replacementImage(80, 60, QImage::Format_ARGB32);
    replacementImage.fill(QColor(QStringLiteral("#ef5350")));
    const QString replacementPath = dir.filePath(QStringLiteral("replacement-open.png"));
    QVERIFY(replacementImage.save(replacementPath));
    engine.rootContext()->setContextProperty(QStringLiteral("testReplacementImageUrl"),
                                             QUrl::fromLocalFile(replacementPath).toString());

    QQmlExpression replaceImage(engine.rootContext(), object.data(),
                                QStringLiteral("openRaster(testReplacementImageUrl);"));
    const QVariant replaceResult = replaceImage.evaluate();
    QVERIFY2(!replaceImage.hasError(), qPrintable(replaceImage.error().toString()));
    QCOMPARE(replaceResult.toBool(), true);

    QTRY_COMPARE(canvasItem->width(), static_cast<qreal>(replacementImage.width()));
    QTRY_COMPARE(canvasItem->height(), static_cast<qreal>(replacementImage.height()));
    QCOMPARE(canvasItem->backgroundSource(), QUrl::fromLocalFile(replacementPath).toString());
    QTRY_COMPARE(viewModel.canvasWidth(), replacementImage.width());
    QTRY_COMPARE(viewModel.canvasHeight(), replacementImage.height());

    objects = rootItem->property("drawableObjects").toList();
    QCOMPARE(objects.size(), 1);
    const QVariantMap replacementLayerObject = objects.first().toMap();
    QCOMPARE(replacementLayerObject.value(QStringLiteral("type")).toString(),
             QStringLiteral("layer"));
    QCOMPARE(replacementLayerObject.value(QStringLiteral("width")).toInt(),
             replacementImage.width());
    QCOMPARE(replacementLayerObject.value(QStringLiteral("height")).toInt(),
             replacementImage.height());
    QCOMPARE(rootItem->property("selectedDrawableObjectId").toInt(),
             replacementLayerObject.value(QStringLiteral("id")).toInt());

    const QString outputPath = dir.filePath(QStringLiteral("opened-canvas-output.png"));
    engine.rootContext()->setContextProperty(QStringLiteral("testSaveImageUrl"),
                                             QUrl::fromLocalFile(outputPath).toString());
    QQmlExpression saveImage(engine.rootContext(), object.data(),
                             QStringLiteral("saveToFile(testSaveImageUrl);"));
    const QVariant saveResult = saveImage.evaluate();
    QVERIFY2(!saveImage.hasError(), qPrintable(saveImage.error().toString()));
    QCOMPARE(saveResult.toBool(), true);

    const QImage saved(outputPath);
    QVERIFY(!saved.isNull());
    QCOMPARE(saved.size(), replacementImage.size());
    QCOMPARE(saved.pixelColor(12, 12).rgba(), QColor(QStringLiteral("#ef5350")).rgba());
}

QTEST_MAIN(tst_DrawingSurfaceItem)

#include "tst_drawingsurfaceitem.moc"
