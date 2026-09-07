#include "localcanvassession.h"

#include "nearbycongregationdiscovery.h"
#include "recentcanvascontainer.h"

#include <QAbstractSocket>
#include <QCborParserError>
#include <QCborValue>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QNetworkAddressEntry>
#include <QNetworkInterface>
#include <QSet>
#include <QStringList>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QUuid>
#include <QtEndian>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <optional>
#include <utility>

namespace
{
constexpr char frameMagic[] = {'V', 'C', 'A', 'N'};
constexpr quint8 protocolVersion = 1;
constexpr qsizetype frameHeaderSize = 16;
constexpr qint64 maximumControlPayloadBytes = 64 * 1024;
constexpr qint64 maximumEditCommandPayloadBytes = 64LL * 1024LL * 1024LL;
constexpr qint64 maximumFramePayloadBytes =
    RecentCanvasMaximumContainerBytes + maximumControlPayloadBytes;
constexpr int maximumParticipants = 16;
constexpr int maximumPendingInvitations = 16;
constexpr qsizetype maximumStrokePointCount = 4096;
constexpr qsizetype maximumLayerOrderCount = 4096;
constexpr qsizetype maximumTextLength = 1024 * 1024;
constexpr int connectionTimeoutMs = 8000;
constexpr qsizetype uuidTextSize = 36;

enum class MessageType : quint8
{
    Hello = 1,
    HelloAccepted = 2,
    EditCommand = 3,
    SnapshotState = 4,
    Participants = 5,
    Error = 6,
};

enum class FrameReadResult
{
    NeedMoreData,
    Ready,
    Invalid,
};

bool isCanonicalUuid(const QString& value)
{
    const QUuid uuid(value);
    return !uuid.isNull() &&
           uuid.toString(QUuid::WithoutBraces).compare(value, Qt::CaseInsensitive) == 0;
}

QString normalizedProfileName(const QString& profileName)
{
    const QString normalized = profileName.simplified().left(80);
    return normalized.isEmpty() ? LocalCanvasSession::tr("Unnamed member") : normalized;
}

bool isLocalNetworkAddress(const QHostAddress& address)
{
    if (address.protocol() != QAbstractSocket::IPv4Protocol || address.isMulticast() ||
        address == QHostAddress::AnyIPv4 || address == QHostAddress::Broadcast)
    {
        return false;
    }
    if (address.isLoopback())
    {
        return true;
    }

    const QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface& networkInterface : interfaces)
    {
        const QNetworkInterface::InterfaceFlags flags = networkInterface.flags();
        if (!flags.testFlag(QNetworkInterface::IsUp) ||
            !flags.testFlag(QNetworkInterface::IsRunning))
        {
            continue;
        }
        for (const QNetworkAddressEntry& entry : networkInterface.addressEntries())
        {
            const int prefixLength = entry.prefixLength();
            if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol && prefixLength > 0 &&
                address.isInSubnet(entry.ip(), prefixLength))
            {
                return true;
            }
        }
    }
    return false;
}

QByteArray encodeFrame(MessageType type, const QByteArray& payload)
{
    if (payload.size() < 0 || payload.size() > maximumFramePayloadBytes)
    {
        return {};
    }

    QByteArray encoded(frameHeaderSize + payload.size(), Qt::Uninitialized);
    std::memcpy(encoded.data(), frameMagic, sizeof(frameMagic));
    encoded[4] = static_cast<char>(protocolVersion);
    encoded[5] = static_cast<char>(type);
    encoded[6] = 0;
    encoded[7] = 0;
    qToBigEndian<quint64>(static_cast<quint64>(payload.size()),
                          reinterpret_cast<uchar*>(encoded.data() + 8));
    if (!payload.isEmpty())
    {
        std::memcpy(encoded.data() + frameHeaderSize, payload.constData(),
                    static_cast<std::size_t>(payload.size()));
    }
    return encoded;
}

FrameReadResult takeFrame(QByteArray& buffer, quint8& type, QByteArray& payload)
{
    if (buffer.size() < frameHeaderSize)
    {
        return FrameReadResult::NeedMoreData;
    }
    if (std::memcmp(buffer.constData(), frameMagic, sizeof(frameMagic)) != 0 ||
        static_cast<quint8>(buffer.at(4)) != protocolVersion || buffer.at(6) != 0 ||
        buffer.at(7) != 0)
    {
        return FrameReadResult::Invalid;
    }

    const quint64 rawPayloadSize =
        qFromBigEndian<quint64>(reinterpret_cast<const uchar*>(buffer.constData() + 8));
    if (rawPayloadSize > static_cast<quint64>(maximumFramePayloadBytes) ||
        rawPayloadSize > static_cast<quint64>(std::numeric_limits<qsizetype>::max()))
    {
        return FrameReadResult::Invalid;
    }
    const qsizetype payloadSize = static_cast<qsizetype>(rawPayloadSize);
    if (payloadSize > std::numeric_limits<qsizetype>::max() - frameHeaderSize)
    {
        return FrameReadResult::Invalid;
    }
    const qsizetype totalSize = frameHeaderSize + payloadSize;
    if (buffer.size() < totalSize)
    {
        return FrameReadResult::NeedMoreData;
    }

    type = static_cast<quint8>(buffer.at(5));
    payload = buffer.mid(frameHeaderSize, payloadSize);
    buffer.remove(0, totalSize);
    return FrameReadResult::Ready;
}

bool writeFrame(QTcpSocket* socket, MessageType type, const QByteArray& payload)
{
    if (!socket || socket->state() != QAbstractSocket::ConnectedState)
    {
        return false;
    }
    const QByteArray frame = encodeFrame(type, payload);
    return !frame.isEmpty() && socket->write(frame) == frame.size();
}

QByteArray compactJson(const QJsonObject& object)
{
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

std::optional<QJsonObject> jsonObject(const QByteArray& payload)
{
    if (payload.isEmpty() || payload.size() > maximumControlPayloadBytes)
    {
        return std::nullopt;
    }
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject())
    {
        return std::nullopt;
    }
    return document.object();
}

bool hasExactKeys(const QVariantMap& map, std::initializer_list<const char*> keys)
{
    if (map.size() != static_cast<qsizetype>(keys.size()))
    {
        return false;
    }
    return std::all_of(keys.begin(), keys.end(),
                       [&map](const char* key) { return map.contains(QString::fromLatin1(key)); });
}

bool isNumericVariant(const QVariant& value)
{
    switch (value.typeId())
    {
    case QMetaType::Double:
    case QMetaType::Float:
    case QMetaType::Int:
    case QMetaType::UInt:
    case QMetaType::LongLong:
    case QMetaType::ULongLong:
        return true;
    default:
        return false;
    }
}

std::optional<double> boundedNumber(const QVariant& value, double minimum, double maximum)
{
    bool converted = false;
    const double number = value.toDouble(&converted);
    if (!isNumericVariant(value) || !converted || !std::isfinite(number) || number < minimum ||
        number > maximum)
    {
        return std::nullopt;
    }
    return number;
}

std::optional<int> boundedInteger(const QVariant& value, int minimum, int maximum)
{
    const std::optional<double> number = boundedNumber(value, minimum, maximum);
    if (!number || std::floor(*number) != *number)
    {
        return std::nullopt;
    }
    return static_cast<int>(*number);
}

std::optional<bool> strictBoolean(const QVariant& value)
{
    if (value.typeId() != QMetaType::Bool)
    {
        return std::nullopt;
    }
    return value.toBool();
}

std::optional<QString> boundedString(const QVariant& value, qsizetype maximumLength,
                                     bool allowEmpty = false)
{
    if (value.typeId() != QMetaType::QString)
    {
        return std::nullopt;
    }
    const QString string = value.toString();
    if ((!allowEmpty && string.isEmpty()) || string.size() > maximumLength)
    {
        return std::nullopt;
    }
    return string;
}

std::optional<QString> normalizedLayerKey(const QVariant& value)
{
    const std::optional<QString> key = boundedString(value, 32);
    if (!key)
    {
        return std::nullopt;
    }
    if (*key == QStringLiteral("raster-canvas"))
    {
        return key;
    }
    if (!key->startsWith(QStringLiteral("object-")))
    {
        return std::nullopt;
    }
    bool converted = false;
    const int objectId = key->mid(7).toInt(&converted);
    if (!converted || objectId <= 0 || *key != QStringLiteral("object-%1").arg(objectId))
    {
        return std::nullopt;
    }
    return key;
}

std::optional<QString> normalizedColor(const QVariant& value)
{
    const std::optional<QString> color = boundedString(value, 9);
    if (!color || !color->startsWith(QLatin1Char('#')) ||
        (color->size() != 4 && color->size() != 7 && color->size() != 9))
    {
        return std::nullopt;
    }
    for (qsizetype index = 1; index < color->size(); ++index)
    {
        const QChar character = color->at(index).toLower();
        const bool asciiDigit =
            character >= QLatin1Char('0') && character <= QLatin1Char('9');
        if (!asciiDigit && (character < QLatin1Char('a') || character > QLatin1Char('f')))
        {
            return std::nullopt;
        }
    }
    return color->toLower();
}

std::optional<QVariantMap> normalizedEditCommand(const QVariantMap& command)
{
    const std::optional<QString> type = boundedString(command.value(QStringLiteral("type")), 32);
    if (!type)
    {
        return std::nullopt;
    }

    if (*type == QStringLiteral("fill"))
    {
        const std::optional<QString> layerKey =
            normalizedLayerKey(command.value(QStringLiteral("layerKey")));
        const std::optional<double> x =
            boundedNumber(command.value(QStringLiteral("x")), -10000000.0, 10000000.0);
        const std::optional<double> y =
            boundedNumber(command.value(QStringLiteral("y")), -10000000.0, 10000000.0);
        const std::optional<QString> color =
            normalizedColor(command.value(QStringLiteral("color")));
        if (!hasExactKeys(command, {"type", "layerKey", "x", "y", "color"}) || !layerKey || !x ||
            !y || !color)
        {
            return std::nullopt;
        }
        return QVariantMap{{QStringLiteral("type"), *type},
                           {QStringLiteral("layerKey"), *layerKey},
                           {QStringLiteral("x"), *x},
                           {QStringLiteral("y"), *y},
                           {QStringLiteral("color"), *color}};
    }

    if (*type == QStringLiteral("stroke"))
    {
        if (!hasExactKeys(command, {"type", "layerKey", "tool", "color", "size", "flow", "opacity",
                                    "hardness", "spacing", "spacingRatio", "pressureMinimum",
                                    "pressureCenter", "pressureMaximum", "pressureOpacity",
                                    "stabilizer", "pressureSensitive", "points"}))
        {
            return std::nullopt;
        }
        const std::optional<QString> layerKey =
            normalizedLayerKey(command.value(QStringLiteral("layerKey")));
        const std::optional<QString> tool = boundedString(command.value(QStringLiteral("tool")), 8);
        const std::optional<QString> color =
            normalizedColor(command.value(QStringLiteral("color")));
        const std::optional<double> size =
            boundedNumber(command.value(QStringLiteral("size")), 0.01, 4096.0);
        const std::optional<double> flow =
            boundedNumber(command.value(QStringLiteral("flow")), 0.0, 1.0);
        const std::optional<double> opacity =
            boundedNumber(command.value(QStringLiteral("opacity")), 0.0, 1.0);
        const std::optional<double> hardness =
            boundedNumber(command.value(QStringLiteral("hardness")), 0.0, 1.0);
        const std::optional<double> spacing =
            boundedNumber(command.value(QStringLiteral("spacing")), 0.0, 4096.0);
        const std::optional<double> spacingRatio =
            boundedNumber(command.value(QStringLiteral("spacingRatio")), 0.0, 1.0);
        const std::optional<double> pressureMinimum =
            boundedNumber(command.value(QStringLiteral("pressureMinimum")), 0.0, 1.0);
        const std::optional<double> pressureCenter =
            boundedNumber(command.value(QStringLiteral("pressureCenter")), 0.0, 1.0);
        const std::optional<double> pressureMaximum =
            boundedNumber(command.value(QStringLiteral("pressureMaximum")), 0.0, 1.0);
        const std::optional<bool> pressureOpacity =
            strictBoolean(command.value(QStringLiteral("pressureOpacity")));
        const std::optional<double> stabilizer =
            boundedNumber(command.value(QStringLiteral("stabilizer")), 0.0, 1.0);
        const std::optional<bool> pressureSensitive =
            strictBoolean(command.value(QStringLiteral("pressureSensitive")));
        const QVariantList points = command.value(QStringLiteral("points")).toList();
        if (!layerKey || !tool ||
            (*tool != QStringLiteral("brush") && *tool != QStringLiteral("eraser")) || !color ||
            !size || !flow || !opacity || !hardness || !spacing || !spacingRatio ||
            !pressureMinimum || !pressureCenter || !pressureMaximum || !pressureOpacity ||
            !stabilizer || !pressureSensitive || *pressureMinimum > *pressureCenter ||
            *pressureCenter > *pressureMaximum || points.size() < 2 ||
            points.size() > maximumStrokePointCount)
        {
            return std::nullopt;
        }

        QVariantList normalizedPoints;
        normalizedPoints.reserve(points.size());
        for (const QVariant& pointValue : points)
        {
            const QVariantMap point = pointValue.toMap();
            const std::optional<double> x =
                boundedNumber(point.value(QStringLiteral("x")), -10000000.0, 10000000.0);
            const std::optional<double> y =
                boundedNumber(point.value(QStringLiteral("y")), -10000000.0, 10000000.0);
            const std::optional<double> pressure =
                boundedNumber(point.value(QStringLiteral("pressure")), 0.0, 1.0);
            if (!hasExactKeys(point, {"x", "y", "pressure"}) || !x || !y || !pressure)
            {
                return std::nullopt;
            }
            normalizedPoints.append(QVariantMap{{QStringLiteral("x"), *x},
                                                {QStringLiteral("y"), *y},
                                                {QStringLiteral("pressure"), *pressure}});
        }
        return QVariantMap{{QStringLiteral("type"), *type},
                           {QStringLiteral("layerKey"), *layerKey},
                           {QStringLiteral("tool"), *tool},
                           {QStringLiteral("color"), *color},
                           {QStringLiteral("size"), *size},
                           {QStringLiteral("flow"), *flow},
                           {QStringLiteral("opacity"), *opacity},
                           {QStringLiteral("hardness"), *hardness},
                           {QStringLiteral("spacing"), *spacing},
                           {QStringLiteral("spacingRatio"), *spacingRatio},
                           {QStringLiteral("pressureMinimum"), *pressureMinimum},
                           {QStringLiteral("pressureCenter"), *pressureCenter},
                           {QStringLiteral("pressureMaximum"), *pressureMaximum},
                           {QStringLiteral("pressureOpacity"), *pressureOpacity},
                           {QStringLiteral("stabilizer"), *stabilizer},
                           {QStringLiteral("pressureSensitive"), *pressureSensitive},
                           {QStringLiteral("points"), normalizedPoints}};
    }

    if (*type == QStringLiteral("add-text"))
    {
        const std::optional<double> x =
            boundedNumber(command.value(QStringLiteral("x")), -10000000.0, 10000000.0);
        const std::optional<double> y =
            boundedNumber(command.value(QStringLiteral("y")), -10000000.0, 10000000.0);
        const std::optional<double> width =
            boundedNumber(command.value(QStringLiteral("width")), 1.0, 10000000.0);
        const std::optional<double> height =
            boundedNumber(command.value(QStringLiteral("height")), 1.0, 10000000.0);
        const std::optional<QString> text =
            boundedString(command.value(QStringLiteral("text")), maximumTextLength);
        const std::optional<double> fontSize =
            boundedNumber(command.value(QStringLiteral("fontSize")), 1.0, 4096.0);
        const std::optional<QString> color =
            normalizedColor(command.value(QStringLiteral("color")));
        if (!hasExactKeys(command,
                          {"type", "x", "y", "width", "height", "text", "fontSize", "color"}) ||
            !x || !y || !width || !height || !text || !fontSize || !color)
        {
            return std::nullopt;
        }
        return QVariantMap{{QStringLiteral("type"), *type},
                           {QStringLiteral("x"), *x},
                           {QStringLiteral("y"), *y},
                           {QStringLiteral("width"), *width},
                           {QStringLiteral("height"), *height},
                           {QStringLiteral("text"), *text},
                           {QStringLiteral("fontSize"), *fontSize},
                           {QStringLiteral("color"), *color}};
    }

    if (*type == QStringLiteral("add-shape"))
    {
        const std::optional<double> x =
            boundedNumber(command.value(QStringLiteral("x")), -10000000.0, 10000000.0);
        const std::optional<double> y =
            boundedNumber(command.value(QStringLiteral("y")), -10000000.0, 10000000.0);
        const std::optional<double> width =
            boundedNumber(command.value(QStringLiteral("width")), 1.0, 10000000.0);
        const std::optional<double> height =
            boundedNumber(command.value(QStringLiteral("height")), 1.0, 10000000.0);
        const std::optional<QString> shape =
            boundedString(command.value(QStringLiteral("shape")), 32);
        const std::optional<QString> color =
            normalizedColor(command.value(QStringLiteral("color")));
        static const QSet<QString> validShapes{
            QStringLiteral("rectangle"),    QStringLiteral("ellipse"),
            QStringLiteral("triangle"),     QStringLiteral("diamond"),
            QStringLiteral("star"),         QStringLiteral("rectanglebubble"),
            QStringLiteral("ellipsebubble")};
        if (!hasExactKeys(command, {"type", "x", "y", "width", "height", "shape", "color"}) || !x ||
            !y || !width || !height || !shape || !validShapes.contains(*shape) || !color)
        {
            return std::nullopt;
        }
        return QVariantMap{{QStringLiteral("type"), *type},     {QStringLiteral("x"), *x},
                           {QStringLiteral("y"), *y},           {QStringLiteral("width"), *width},
                           {QStringLiteral("height"), *height}, {QStringLiteral("shape"), *shape},
                           {QStringLiteral("color"), *color}};
    }

    if (*type == QStringLiteral("transform-object"))
    {
        const std::optional<int> objectId = boundedInteger(
            command.value(QStringLiteral("objectId")), 1, std::numeric_limits<int>::max());
        const std::optional<double> x =
            boundedNumber(command.value(QStringLiteral("x")), -10000000.0, 10000000.0);
        const std::optional<double> y =
            boundedNumber(command.value(QStringLiteral("y")), -10000000.0, 10000000.0);
        const std::optional<double> width =
            boundedNumber(command.value(QStringLiteral("width")), 1.0, 10000000.0);
        const std::optional<double> height =
            boundedNumber(command.value(QStringLiteral("height")), 1.0, 10000000.0);
        if (!hasExactKeys(command, {"type", "objectId", "x", "y", "width", "height"}) ||
            !objectId || !x || !y || !width || !height)
        {
            return std::nullopt;
        }
        return QVariantMap{
            {QStringLiteral("type"), *type},   {QStringLiteral("objectId"), *objectId},
            {QStringLiteral("x"), *x},         {QStringLiteral("y"), *y},
            {QStringLiteral("width"), *width}, {QStringLiteral("height"), *height}};
    }

    if (*type == QStringLiteral("new-canvas"))
    {
        const std::optional<int> width =
            boundedInteger(command.value(QStringLiteral("width")), 1, 8192);
        const std::optional<int> height =
            boundedInteger(command.value(QStringLiteral("height")), 1, 8192);
        const std::optional<bool> infinite =
            strictBoolean(command.value(QStringLiteral("infinite")));
        if (!hasExactKeys(command, {"type", "width", "height", "infinite"}) || !width || !height ||
            !infinite)
        {
            return std::nullopt;
        }
        return QVariantMap{{QStringLiteral("type"), *type},
                           {QStringLiteral("width"), *width},
                           {QStringLiteral("height"), *height},
                           {QStringLiteral("infinite"), *infinite}};
    }

    if (*type == QStringLiteral("ensure-region"))
    {
        const std::optional<int> x =
            boundedInteger(command.value(QStringLiteral("x")), std::numeric_limits<int>::min() + 1,
                           std::numeric_limits<int>::max());
        const std::optional<int> y =
            boundedInteger(command.value(QStringLiteral("y")), std::numeric_limits<int>::min() + 1,
                           std::numeric_limits<int>::max());
        const std::optional<int> width =
            boundedInteger(command.value(QStringLiteral("width")), 1, 10000000);
        const std::optional<int> height =
            boundedInteger(command.value(QStringLiteral("height")), 1, 10000000);
        if (!hasExactKeys(command, {"type", "x", "y", "width", "height"}) || !x || !y || !width ||
            !height)
        {
            return std::nullopt;
        }
        return QVariantMap{{QStringLiteral("type"), *type},
                           {QStringLiteral("x"), *x},
                           {QStringLiteral("y"), *y},
                           {QStringLiteral("width"), *width},
                           {QStringLiteral("height"), *height}};
    }

    if (*type == QStringLiteral("clear-canvas") || *type == QStringLiteral("add-layer"))
    {
        return hasExactKeys(command, {"type"})
                   ? std::optional<QVariantMap>(QVariantMap{{QStringLiteral("type"), *type}})
                   : std::nullopt;
    }

    if (*type == QStringLiteral("delete-layer") || *type == QStringLiteral("undo") ||
        *type == QStringLiteral("redo"))
    {
        const std::optional<QString> layerKey =
            normalizedLayerKey(command.value(QStringLiteral("layerKey")));
        if (!hasExactKeys(command, {"type", "layerKey"}) || !layerKey)
        {
            return std::nullopt;
        }
        return QVariantMap{{QStringLiteral("type"), *type},
                           {QStringLiteral("layerKey"), *layerKey}};
    }

    if (*type == QStringLiteral("rename-layer"))
    {
        const std::optional<QString> layerKey =
            normalizedLayerKey(command.value(QStringLiteral("layerKey")));
        const std::optional<QString> name =
            boundedString(command.value(QStringLiteral("name")), 256);
        if (!hasExactKeys(command, {"type", "layerKey", "name"}) || !layerKey || !name ||
            name->trimmed().isEmpty())
        {
            return std::nullopt;
        }
        return QVariantMap{{QStringLiteral("type"), *type},
                           {QStringLiteral("layerKey"), *layerKey},
                           {QStringLiteral("name"), name->trimmed()}};
    }

    if (*type == QStringLiteral("reorder-layers"))
    {
        const QVariantList layerKeys = command.value(QStringLiteral("layerKeys")).toList();
        if (!hasExactKeys(command, {"type", "layerKeys"}) || layerKeys.isEmpty() ||
            layerKeys.size() > maximumLayerOrderCount)
        {
            return std::nullopt;
        }
        QVariantList normalizedKeys;
        QSet<QString> seenKeys;
        for (const QVariant& keyValue : layerKeys)
        {
            const std::optional<QString> key = normalizedLayerKey(keyValue);
            if (!key || seenKeys.contains(*key))
            {
                return std::nullopt;
            }
            seenKeys.insert(*key);
            normalizedKeys.append(*key);
        }
        return QVariantMap{{QStringLiteral("type"), *type},
                           {QStringLiteral("layerKeys"), normalizedKeys}};
    }

    if (*type == QStringLiteral("insert-image"))
    {
        const QByteArray data = command.value(QStringLiteral("data")).toByteArray();
        const std::optional<double> x =
            boundedNumber(command.value(QStringLiteral("x")), -10000000.0, 10000000.0);
        const std::optional<double> y =
            boundedNumber(command.value(QStringLiteral("y")), -10000000.0, 10000000.0);
        const std::optional<bool> positioned =
            strictBoolean(command.value(QStringLiteral("positioned")));
        const std::optional<QString> name =
            boundedString(command.value(QStringLiteral("name")), 256);
        if (!hasExactKeys(command, {"type", "data", "x", "y", "positioned", "name"}) ||
            command.value(QStringLiteral("data")).typeId() != QMetaType::QByteArray ||
            data.isEmpty() || data.size() > maximumEditCommandPayloadBytes || !x || !y ||
            !positioned || !name)
        {
            return std::nullopt;
        }
        return QVariantMap{{QStringLiteral("type"), *type},
                           {QStringLiteral("data"), data},
                           {QStringLiteral("x"), *x},
                           {QStringLiteral("y"), *y},
                           {QStringLiteral("positioned"), *positioned},
                           {QStringLiteral("name"), *name}};
    }

    if (*type == QStringLiteral("open-raster"))
    {
        const QByteArray data = command.value(QStringLiteral("data")).toByteArray();
        if (!hasExactKeys(command, {"type", "data"}) ||
            command.value(QStringLiteral("data")).typeId() != QMetaType::QByteArray ||
            data.isEmpty() || data.size() > maximumEditCommandPayloadBytes)
        {
            return std::nullopt;
        }
        return QVariantMap{{QStringLiteral("type"), *type}, {QStringLiteral("data"), data}};
    }

    return std::nullopt;
}

QByteArray encodeEditCommand(const QVariantMap& command)
{
    const std::optional<QVariantMap> normalized = normalizedEditCommand(command);
    if (!normalized)
    {
        return {};
    }
    const QByteArray encoded = QCborValue::fromVariant(*normalized).toCbor();
    return encoded.size() <= maximumEditCommandPayloadBytes ? encoded : QByteArray{};
}

std::optional<QVariantMap> decodeEditCommand(const QByteArray& payload)
{
    if (payload.isEmpty() || payload.size() > maximumEditCommandPayloadBytes)
    {
        return std::nullopt;
    }
    QCborParserError parseError;
    const QCborValue value = QCborValue::fromCbor(payload, &parseError);
    if (parseError.error != QCborError::NoError || !value.isMap())
    {
        return std::nullopt;
    }
    return normalizedEditCommand(value.toVariant().toMap());
}

QByteArray snapshotStatePayload(quint64 revision, const QString& originPeerId,
                                const QByteArray& snapshot)
{
    const QByteArray origin = originPeerId.toLower().toUtf8();
    if (origin.size() != uuidTextSize)
    {
        return {};
    }
    QByteArray payload(static_cast<qsizetype>(sizeof(quint64)) + uuidTextSize + snapshot.size(),
                       Qt::Uninitialized);
    qToBigEndian<quint64>(revision, reinterpret_cast<uchar*>(payload.data()));
    std::memcpy(payload.data() + sizeof(quint64), origin.constData(),
                static_cast<std::size_t>(uuidTextSize));
    std::memcpy(payload.data() + sizeof(quint64) + uuidTextSize, snapshot.constData(),
                static_cast<std::size_t>(snapshot.size()));
    return payload;
}

bool decodeSnapshotState(const QByteArray& payload, quint64& revision, QString& originPeerId,
                         QByteArray& snapshot)
{
    const qsizetype prefixSize = static_cast<qsizetype>(sizeof(quint64)) + uuidTextSize;
    if (payload.size() <= prefixSize)
    {
        return false;
    }
    revision = qFromBigEndian<quint64>(reinterpret_cast<const uchar*>(payload.constData()));
    originPeerId = QString::fromLatin1(payload.constData() + sizeof(quint64), uuidTextSize);
    snapshot = payload.mid(prefixSize);
    return revision > 0 && isCanonicalUuid(originPeerId);
}

QJsonObject errorObject(const QString& code, const QString& message)
{
    return {{QStringLiteral("code"), code}, {QStringLiteral("message"), message}};
}
} // namespace

LocalCanvasSession::LocalCanvasSession(NearbyCongregationDiscovery* discovery, QObject* parent)
    : QObject(parent), m_discovery(discovery),
      m_ownSessionId(discovery ? discovery->sessionId()
                               : QUuid::createUuid().toString(QUuid::WithoutBraces)),
      m_sessionId(m_ownSessionId), m_peerId(QUuid::createUuid().toString(QUuid::WithoutBraces)),
      m_connectTimer(new QTimer(this))
{
    m_connectTimer->setSingleShot(true);
    m_connectTimer->setInterval(connectionTimeoutMs);
    connect(m_connectTimer, &QTimer::timeout, this,
            [this]()
            {
                if (m_state != SessionState::Joining)
                {
                    return;
                }
                setErrorString(tr("The nearby canvas did not respond."));
                if (m_clientSocket)
                {
                    m_clientSocket->abort();
                }
            });

    if (m_discovery)
    {
        connect(m_discovery, &NearbyCongregationDiscovery::availableCanvasSessionsChanged, this,
                &LocalCanvasSession::availableCanvasesChanged);
        connect(m_discovery, &NearbyCongregationDiscovery::availableInvitationTargetsChanged, this,
                &LocalCanvasSession::availableInviteesChanged);
        connect(m_discovery, &NearbyCongregationDiscovery::canvasInvitationReceived, this,
                &LocalCanvasSession::receiveCanvasInvitation);
    }
}

LocalCanvasSession::~LocalCanvasSession()
{
    stopSession();
}

bool LocalCanvasSession::active() const noexcept
{
    return m_state != SessionState::Idle;
}

bool LocalCanvasSession::hosting() const noexcept
{
    return m_state == SessionState::Hosting;
}

bool LocalCanvasSession::connected() const noexcept
{
    return m_state == SessionState::Connected;
}

bool LocalCanvasSession::currentUserIsHost() const noexcept
{
    return m_state != SessionState::Joining && m_state != SessionState::Connected;
}

QString LocalCanvasSession::state() const
{
    switch (m_state)
    {
    case SessionState::Hosting:
        return QStringLiteral("hosting");
    case SessionState::Joining:
        return QStringLiteral("joining");
    case SessionState::Connected:
        return QStringLiteral("connected");
    case SessionState::Idle:
    default:
        return QStringLiteral("idle");
    }
}

QString LocalCanvasSession::sessionId() const
{
    return m_sessionId;
}

QString LocalCanvasSession::peerId() const
{
    return m_peerId;
}

QString LocalCanvasSession::errorString() const
{
    return m_errorString;
}

QVariantList LocalCanvasSession::participantProfiles() const
{
    return m_participantProfiles;
}

int LocalCanvasSession::participantCount() const noexcept
{
    return static_cast<int>(m_participantProfiles.size());
}

QVariantList LocalCanvasSession::availableCanvases() const
{
    QVariantList result;
    if (!m_discovery)
    {
        return result;
    }

    for (const QVariant& value : m_discovery->availableCanvasSessions())
    {
        QVariantMap canvas = value.toMap();
        const QString advertisedSessionId = canvas.value(QStringLiteral("sessionId")).toString();
        if (!isCanonicalUuid(advertisedSessionId) ||
            (hosting() && advertisedSessionId == m_sessionId))
        {
            continue;
        }
        const QString address = canvas.value(QStringLiteral("address")).toString();
        canvas.insert(QStringLiteral("displayName"), tr("Canvas at %1").arg(address));
        result.append(canvas);
    }
    return result;
}

QVariantList LocalCanvasSession::availableInvitees() const
{
    QVariantList result;
    if (!m_discovery)
    {
        return result;
    }

    for (const QVariant& value : m_discovery->availableInvitationTargets())
    {
        const QVariantMap target = value.toMap();
        const QString targetSessionId = target.value(QStringLiteral("sessionId")).toString();
        const QString address = target.value(QStringLiteral("address")).toString();
        if (!isCanonicalUuid(targetSessionId) || address.isEmpty())
        {
            continue;
        }
        result.append(
            QVariantMap{{QStringLiteral("sessionId"), targetSessionId},
                        {QStringLiteral("address"), address},
                        {QStringLiteral("displayName"), tr("Congregation at %1").arg(address)}});
    }
    return result;
}

QVariantMap LocalCanvasSession::pendingInvitation() const
{
    return m_pendingInvitations.isEmpty() ? QVariantMap{} : m_pendingInvitations.first().toMap();
}

int LocalCanvasSession::pendingInvitationCount() const noexcept
{
    return static_cast<int>(m_pendingInvitations.size());
}

bool LocalCanvasSession::invitationsAllowed() const noexcept
{
    return m_invitationsAllowed;
}

quint16 LocalCanvasSession::listenPort() const noexcept
{
    return m_server ? m_server->serverPort() : 0;
}

quint64 LocalCanvasSession::revision() const noexcept
{
    return m_revision;
}

bool LocalCanvasSession::startHosting(const QString& profileName)
{
    stopSession();
    m_localProfileName = normalizedProfileName(profileName);
    m_sessionId = m_ownSessionId;

    m_server = new QTcpServer(this);
    m_server->setMaxPendingConnections(maximumParticipants);
    connect(m_server, &QTcpServer::newConnection, this,
            &LocalCanvasSession::handleHostNewConnection);
    if (!m_server->listen(QHostAddress::AnyIPv4, 0))
    {
        setErrorString(m_server->errorString());
        delete m_server;
        m_server = nullptr;
        return false;
    }

    setErrorString({});
    setState(SessionState::Hosting);
    if (m_discovery)
    {
        m_discovery->setHostedCanvasPort(m_server->serverPort());
    }
    QTimer::singleShot(0, this,
                       [this]()
                       {
                           if (hosting() && m_latestSnapshot.isEmpty())
                           {
                               emit snapshotRequested();
                           }
                       });
    return true;
}

bool LocalCanvasSession::joinCanvas(const QString& requestedSessionId, const QString& profileName)
{
    for (const QVariant& value : availableCanvases())
    {
        const QVariantMap canvas = value.toMap();
        if (canvas.value(QStringLiteral("sessionId")).toString() != requestedSessionId)
        {
            continue;
        }
        return joinCanvasAt(requestedSessionId, canvas.value(QStringLiteral("address")).toString(),
                            canvas.value(QStringLiteral("port")).toInt(), profileName);
    }
    setErrorString(tr("The nearby canvas is no longer available."));
    return false;
}

bool LocalCanvasSession::joinCanvasAt(const QString& requestedSessionId, const QString& address,
                                      int port, const QString& profileName)
{
    const QHostAddress hostAddress(address);
    if (!isCanonicalUuid(requestedSessionId) || !isLocalNetworkAddress(hostAddress) || port <= 0 ||
        port > 65535)
    {
        setErrorString(tr("The nearby canvas address is invalid."));
        return false;
    }

    stopSession();
    m_sessionId = requestedSessionId.toLower();
    m_localProfileName = normalizedProfileName(profileName);
    m_clientSocket = new QTcpSocket(this);
    m_clientSocket->setReadBufferSize(maximumFramePayloadBytes + frameHeaderSize);
    connect(m_clientSocket, &QTcpSocket::connected, this,
            &LocalCanvasSession::handleClientConnected);
    connect(m_clientSocket, &QTcpSocket::readyRead, this,
            &LocalCanvasSession::handleClientReadyRead);
    connect(m_clientSocket, &QTcpSocket::disconnected, this,
            &LocalCanvasSession::handleClientDisconnected);
    connect(m_clientSocket, &QTcpSocket::errorOccurred, this,
            [this](QAbstractSocket::SocketError) { handleClientError(); });

    setErrorString({});
    setState(SessionState::Joining);
    m_connectTimer->start();
    m_clientSocket->connectToHost(hostAddress, static_cast<quint16>(port));
    return true;
}

void LocalCanvasSession::stopSession()
{
    const bool wasActive = active();
    m_stopping = true;
    if (m_discovery && hosting())
    {
        m_discovery->setHostedCanvasPort(0);
    }
    resetTransport();
    m_stopping = false;

    m_sessionId = m_ownSessionId;
    m_hostPeerId.clear();
    m_latestSnapshot.clear();
    m_latestOriginPeerId.clear();
    if (m_revision != 0)
    {
        m_revision = 0;
        emit revisionChanged();
    }
    clearParticipants();
    if (wasActive)
    {
        setState(SessionState::Idle);
    }
}

void LocalCanvasSession::setLocalProfileName(const QString& profileName)
{
    const QString normalized = normalizedProfileName(profileName);
    if (m_localProfileName == normalized)
    {
        return;
    }
    m_localProfileName = normalized;
    if (hosting())
    {
        broadcastParticipantProfiles();
        return;
    }
    if (connected() && m_clientSocket)
    {
        const QJsonObject hello{
            {QStringLiteral("service"), QStringLiteral("com.iisacc.congregation.canvas")},
            {QStringLiteral("version"), protocolVersion},
            {QStringLiteral("session"), m_sessionId},
            {QStringLiteral("peer"), m_peerId},
            {QStringLiteral("name"), m_localProfileName},
            {QStringLiteral("update"), true}};
        writeFrame(m_clientSocket, MessageType::Hello, compactJson(hello));
    }
}

bool LocalCanvasSession::publishSnapshot(const QByteArray& snapshot)
{
    if (!validSnapshot(snapshot))
    {
        setErrorString(tr("The canvas snapshot is invalid or too large."));
        return false;
    }
    if (!hosting())
    {
        return false;
    }
    acceptSnapshot(snapshot, m_peerId);
    setErrorString({});
    return true;
}

bool LocalCanvasSession::submitEditCommand(const QVariantMap& command)
{
    if (!connected() || !m_clientSocket)
    {
        return false;
    }
    const QByteArray payload = encodeEditCommand(command);
    if (payload.isEmpty() || !writeFrame(m_clientSocket, MessageType::EditCommand, payload))
    {
        setErrorString(tr("The host canvas command could not be sent."));
        return false;
    }
    setErrorString({});
    return true;
}

bool LocalCanvasSession::removeParticipant(const QString& participantPeerId)
{
    if (!hosting() || !isCanonicalUuid(participantPeerId))
    {
        return false;
    }
    for (auto iterator = m_hostPeers.begin(); iterator != m_hostPeers.end(); ++iterator)
    {
        if (iterator->accepted && iterator->peerId == participantPeerId)
        {
            disconnectHostPeer(iterator.key(), QStringLiteral("removed"),
                               tr("The host removed you from this canvas."));
            return true;
        }
    }
    return false;
}

void LocalCanvasSession::setInvitationsAllowed(bool allowed)
{
    if (m_invitationsAllowed == allowed)
    {
        return;
    }
    m_invitationsAllowed = allowed;
    if (m_discovery)
    {
        m_discovery->setInvitationsAllowed(allowed);
    }
    if (!allowed && !m_pendingInvitations.isEmpty())
    {
        m_pendingInvitations.clear();
        emit pendingInvitationsChanged();
    }
    emit invitationsAllowedChanged();
}

bool LocalCanvasSession::invitePeer(const QString& targetSessionId, const QString& profileName)
{
    if (!m_discovery || connected() || m_state == SessionState::Joining)
    {
        return false;
    }

    const QString normalizedTargetSessionId = targetSessionId.toLower();
    const QVariantList invitationTargets = m_discovery->availableInvitationTargets();
    const bool targetAvailable =
        std::any_of(invitationTargets.cbegin(), invitationTargets.cend(),
                    [&normalizedTargetSessionId](const QVariant& value)
                    {
                        return value.toMap().value(QStringLiteral("sessionId")).toString() ==
                               normalizedTargetSessionId;
                    });
    if (!isCanonicalUuid(normalizedTargetSessionId) || !targetAvailable)
    {
        setErrorString(tr("The nearby Congregation user is no longer available for invitations."));
        return false;
    }

    if (!hosting() && !startHosting(profileName))
    {
        return false;
    }
    setLocalProfileName(profileName);
    const QString invitationId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const bool sent = m_discovery->sendCanvasInvitation(invitationId, normalizedTargetSessionId,
                                                        listenPort(), m_localProfileName);
    if (!sent)
    {
        setErrorString(tr("The canvas invitation could not be sent."));
        return false;
    }
    setErrorString({});
    return true;
}

bool LocalCanvasSession::respondToPendingInvitation(bool accepted, const QString& profileName)
{
    if (m_pendingInvitations.isEmpty())
    {
        return false;
    }

    const QVariantMap invitation = m_pendingInvitations.takeFirst().toMap();
    emit pendingInvitationsChanged();
    if (!accepted)
    {
        return true;
    }

    return joinCanvasAt(invitation.value(QStringLiteral("sessionId")).toString(),
                        invitation.value(QStringLiteral("address")).toString(),
                        invitation.value(QStringLiteral("port")).toInt(), profileName);
}

void LocalCanvasSession::receiveCanvasInvitation(const QVariantMap& invitation)
{
    if (!m_invitationsAllowed)
    {
        return;
    }

    const QString invitationId =
        invitation.value(QStringLiteral("invitationId")).toString().toLower();
    const QString senderSessionId =
        invitation.value(QStringLiteral("sessionId")).toString().toLower();
    const QHostAddress senderAddress(invitation.value(QStringLiteral("address")).toString());
    const int canvasPort = invitation.value(QStringLiteral("port")).toInt();
    const QString profileName =
        normalizedProfileName(invitation.value(QStringLiteral("profileName")).toString());
    if (!isCanonicalUuid(invitationId) || !isCanonicalUuid(senderSessionId) ||
        senderSessionId == m_ownSessionId || !isLocalNetworkAddress(senderAddress) ||
        canvasPort <= 0 || canvasPort > 65535)
    {
        return;
    }

    for (const QVariant& pendingValue : std::as_const(m_pendingInvitations))
    {
        if (pendingValue.toMap().value(QStringLiteral("invitationId")).toString() == invitationId)
        {
            return;
        }
    }
    for (qsizetype index = m_pendingInvitations.size() - 1; index >= 0; --index)
    {
        if (m_pendingInvitations.at(index).toMap().value(QStringLiteral("sessionId")).toString() ==
            senderSessionId)
        {
            m_pendingInvitations.removeAt(index);
        }
    }
    while (m_pendingInvitations.size() >= maximumPendingInvitations)
    {
        m_pendingInvitations.removeFirst();
    }
    m_pendingInvitations.append(QVariantMap{{QStringLiteral("invitationId"), invitationId},
                                            {QStringLiteral("sessionId"), senderSessionId},
                                            {QStringLiteral("address"), senderAddress.toString()},
                                            {QStringLiteral("port"), canvasPort},
                                            {QStringLiteral("profileName"), profileName},
                                            {QStringLiteral("profileImageSource"), QString{}}});
    emit pendingInvitationsChanged();
}

void LocalCanvasSession::setState(SessionState stateValue)
{
    if (m_state == stateValue)
    {
        return;
    }
    m_state = stateValue;
    emit stateChanged();
    emit availableCanvasesChanged();
}

void LocalCanvasSession::setErrorString(const QString& errorStringValue)
{
    if (m_errorString == errorStringValue)
    {
        return;
    }
    m_errorString = errorStringValue;
    emit errorStringChanged();
}

void LocalCanvasSession::clearParticipants()
{
    if (m_participantProfiles.isEmpty())
    {
        return;
    }
    m_participantProfiles.clear();
    emit participantProfilesChanged();
}

void LocalCanvasSession::rebuildHostParticipantProfiles()
{
    QVariantList profiles;
    QStringList peerIds;
    for (const HostPeer& peer : std::as_const(m_hostPeers))
    {
        if (peer.accepted)
        {
            peerIds.append(peer.peerId);
        }
    }
    peerIds.sort(Qt::CaseInsensitive);
    for (const QString& remotePeerId : peerIds)
    {
        for (const HostPeer& peer : std::as_const(m_hostPeers))
        {
            if (!peer.accepted || peer.peerId != remotePeerId)
            {
                continue;
            }
            profiles.append(QVariantMap{{QStringLiteral("peerId"), peer.peerId},
                                        {QStringLiteral("profileName"), peer.profileName},
                                        {QStringLiteral("profileImageSource"), QString{}},
                                        {QStringLiteral("isHost"), false},
                                        {QStringLiteral("isMe"), false},
                                        {QStringLiteral("removable"), true}});
            break;
        }
    }
    if (m_participantProfiles == profiles)
    {
        return;
    }
    m_participantProfiles = std::move(profiles);
    emit participantProfilesChanged();
}

QByteArray LocalCanvasSession::participantPayload() const
{
    QJsonArray participants;
    participants.append(QJsonObject{{QStringLiteral("peer"), m_peerId},
                                    {QStringLiteral("name"), m_localProfileName},
                                    {QStringLiteral("host"), true}});

    QStringList peerIds;
    for (const HostPeer& peer : std::as_const(m_hostPeers))
    {
        if (peer.accepted)
        {
            peerIds.append(peer.peerId);
        }
    }
    peerIds.sort(Qt::CaseInsensitive);
    for (const QString& remotePeerId : peerIds)
    {
        for (const HostPeer& peer : std::as_const(m_hostPeers))
        {
            if (peer.accepted && peer.peerId == remotePeerId)
            {
                participants.append(QJsonObject{{QStringLiteral("peer"), peer.peerId},
                                                {QStringLiteral("name"), peer.profileName},
                                                {QStringLiteral("host"), false}});
                break;
            }
        }
    }
    return QJsonDocument(participants).toJson(QJsonDocument::Compact);
}

void LocalCanvasSession::broadcastParticipantProfiles()
{
    if (!hosting())
    {
        return;
    }
    rebuildHostParticipantProfiles();
    const QByteArray payload = participantPayload();
    for (auto iterator = m_hostPeers.cbegin(); iterator != m_hostPeers.cend(); ++iterator)
    {
        if (iterator->accepted)
        {
            writeFrame(iterator.key(), MessageType::Participants, payload);
        }
    }
}

void LocalCanvasSession::applyClientParticipantProfiles(const QByteArray& payload)
{
    if (payload.isEmpty() || payload.size() > maximumControlPayloadBytes)
    {
        return;
    }
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &error);
    if (error.error != QJsonParseError::NoError || !document.isArray() ||
        document.array().size() > maximumParticipants + 1)
    {
        return;
    }

    QVariantList profiles;
    QSet<QString> peerIds;
    int hostCount = 0;
    bool includesCurrentUser = false;
    for (const QJsonValue& value : document.array())
    {
        if (!value.isObject())
        {
            return;
        }
        const QJsonObject participant = value.toObject();
        const QString participantPeerId = participant.value(QStringLiteral("peer")).toString();
        const QString name =
            normalizedProfileName(participant.value(QStringLiteral("name")).toString());
        const bool isHost = participant.value(QStringLiteral("host")).toBool();
        if (!isCanonicalUuid(participantPeerId) || peerIds.contains(participantPeerId))
        {
            return;
        }
        peerIds.insert(participantPeerId);
        hostCount += isHost ? 1 : 0;
        if (participantPeerId == m_peerId)
        {
            includesCurrentUser = true;
            continue;
        }
        profiles.append(QVariantMap{{QStringLiteral("peerId"), participantPeerId},
                                    {QStringLiteral("profileName"), name},
                                    {QStringLiteral("profileImageSource"), QString{}},
                                    {QStringLiteral("isHost"), isHost},
                                    {QStringLiteral("isMe"), false},
                                    {QStringLiteral("removable"), false}});
    }
    if (hostCount != 1 || !includesCurrentUser)
    {
        return;
    }
    if (m_participantProfiles == profiles)
    {
        return;
    }
    m_participantProfiles = std::move(profiles);
    emit participantProfilesChanged();
}

void LocalCanvasSession::handleHostNewConnection()
{
    while (m_server && m_server->hasPendingConnections())
    {
        QTcpSocket* socket = m_server->nextPendingConnection();
        if (!socket)
        {
            continue;
        }
        if (!isLocalNetworkAddress(socket->peerAddress()))
        {
            disconnectHostPeer(socket, QStringLiteral("network"),
                               tr("Canvas sharing accepts local-network participants only."));
            continue;
        }
        if (m_hostPeers.size() >= maximumParticipants)
        {
            disconnectHostPeer(socket, QStringLiteral("full"),
                               tr("This canvas already has the maximum number of participants."));
            continue;
        }
        socket->setReadBufferSize(maximumFramePayloadBytes + frameHeaderSize);
        m_hostPeers.insert(socket, HostPeer{});
        connect(socket, &QTcpSocket::readyRead, this,
                [this, socket]() { handleHostReadyRead(socket); });
        connect(socket, &QTcpSocket::disconnected, this,
                [this, socket]() { handleHostDisconnected(socket); });
        QTimer::singleShot(connectionTimeoutMs, socket,
                           [this, socket]()
                           {
                               const auto iterator = m_hostPeers.constFind(socket);
                               if (iterator != m_hostPeers.cend() && !iterator->accepted)
                               {
                                   disconnectHostPeer(
                                       socket, QStringLiteral("timeout"),
                                       tr("The canvas connection handshake timed out."));
                               }
                           });
    }
}

void LocalCanvasSession::handleHostReadyRead(QTcpSocket* socket)
{
    auto iterator = m_hostPeers.find(socket);
    if (iterator == m_hostPeers.end())
    {
        return;
    }
    iterator->receiveBuffer.append(socket->readAll());
    while (true)
    {
        quint8 type = 0;
        QByteArray payload;
        const FrameReadResult result = takeFrame(iterator->receiveBuffer, type, payload);
        if (result == FrameReadResult::NeedMoreData)
        {
            return;
        }
        if (result == FrameReadResult::Invalid)
        {
            disconnectHostPeer(socket, QStringLiteral("protocol"),
                               tr("The canvas connection used an invalid protocol frame."));
            return;
        }
        processHostFrame(socket, type, payload);
        if (!m_hostPeers.contains(socket) || socket->state() != QAbstractSocket::ConnectedState)
        {
            return;
        }
        iterator = m_hostPeers.find(socket);
    }
}

void LocalCanvasSession::handleHostDisconnected(QTcpSocket* socket)
{
    const auto iterator = m_hostPeers.find(socket);
    const bool accepted = iterator != m_hostPeers.end() && iterator->accepted;
    if (iterator != m_hostPeers.end())
    {
        m_hostPeers.erase(iterator);
    }
    socket->deleteLater();
    if (accepted && hosting() && !m_stopping)
    {
        broadcastParticipantProfiles();
    }
}

void LocalCanvasSession::handleClientConnected()
{
    if (!m_clientSocket || m_state != SessionState::Joining)
    {
        return;
    }
    const QJsonObject hello{
        {QStringLiteral("service"), QStringLiteral("com.iisacc.congregation.canvas")},
        {QStringLiteral("version"), protocolVersion},
        {QStringLiteral("session"), m_sessionId},
        {QStringLiteral("peer"), m_peerId},
        {QStringLiteral("name"), m_localProfileName}};
    if (!writeFrame(m_clientSocket, MessageType::Hello, compactJson(hello)))
    {
        setErrorString(tr("The canvas connection handshake could not be sent."));
        m_clientSocket->abort();
    }
}

void LocalCanvasSession::handleClientReadyRead()
{
    if (!m_clientSocket)
    {
        return;
    }
    m_clientReceiveBuffer.append(m_clientSocket->readAll());
    while (true)
    {
        quint8 type = 0;
        QByteArray payload;
        const FrameReadResult result = takeFrame(m_clientReceiveBuffer, type, payload);
        if (result == FrameReadResult::NeedMoreData)
        {
            return;
        }
        if (result == FrameReadResult::Invalid)
        {
            setErrorString(tr("The host sent an invalid canvas protocol frame."));
            m_clientSocket->abort();
            return;
        }
        processClientFrame(type, payload);
        if (!m_clientSocket || m_clientSocket->state() != QAbstractSocket::ConnectedState)
        {
            return;
        }
    }
}

void LocalCanvasSession::handleClientDisconnected()
{
    if (!m_clientSocket)
    {
        return;
    }
    m_clientSocket->deleteLater();
    m_clientSocket = nullptr;
    m_connectTimer->stop();
    m_clientReceiveBuffer.clear();
    clearParticipants();
    if (m_stopping)
    {
        return;
    }
    if (m_errorString.isEmpty())
    {
        setErrorString(tr("The host closed the canvas session."));
    }
    m_sessionId = m_ownSessionId;
    setState(SessionState::Idle);
}

void LocalCanvasSession::handleClientError()
{
    if (!m_clientSocket || m_stopping)
    {
        return;
    }
    if (m_errorString.isEmpty())
    {
        setErrorString(m_clientSocket->errorString());
    }
}

void LocalCanvasSession::processHostFrame(QTcpSocket* socket, quint8 rawType,
                                          const QByteArray& payload)
{
    auto iterator = m_hostPeers.find(socket);
    if (iterator == m_hostPeers.end())
    {
        return;
    }
    const MessageType type = static_cast<MessageType>(rawType);
    if (type == MessageType::Hello)
    {
        const std::optional<QJsonObject> hello = jsonObject(payload);
        if (!hello ||
            hello->value(QStringLiteral("service")).toString() !=
                QStringLiteral("com.iisacc.congregation.canvas") ||
            hello->value(QStringLiteral("version")).toInt() != protocolVersion ||
            hello->value(QStringLiteral("session")).toString() != m_sessionId)
        {
            disconnectHostPeer(socket, QStringLiteral("handshake"),
                               tr("The canvas connection handshake is invalid."));
            return;
        }

        const QString remotePeerId = hello->value(QStringLiteral("peer")).toString().toLower();
        if (!isCanonicalUuid(remotePeerId) || remotePeerId == m_peerId)
        {
            disconnectHostPeer(socket, QStringLiteral("identity"),
                               tr("The canvas participant identity is invalid."));
            return;
        }
        for (auto peerIterator = m_hostPeers.cbegin(); peerIterator != m_hostPeers.cend();
             ++peerIterator)
        {
            if (peerIterator.key() != socket && peerIterator->accepted &&
                peerIterator->peerId == remotePeerId)
            {
                disconnectHostPeer(socket, QStringLiteral("duplicate"),
                                   tr("This Congregation session is already connected."));
                return;
            }
        }

        const bool profileUpdate = hello->value(QStringLiteral("update")).toBool(false);
        if (iterator->accepted && !profileUpdate)
        {
            disconnectHostPeer(socket, QStringLiteral("protocol"),
                               tr("The canvas connection repeated its handshake."));
            return;
        }
        if (iterator->accepted && iterator->peerId != remotePeerId)
        {
            disconnectHostPeer(socket, QStringLiteral("identity"),
                               tr("The canvas participant identity changed."));
            return;
        }

        iterator->peerId = remotePeerId;
        iterator->profileName =
            normalizedProfileName(hello->value(QStringLiteral("name")).toString());
        if (!iterator->accepted)
        {
            iterator->accepted = true;
            const QJsonObject accepted{
                {QStringLiteral("service"), QStringLiteral("com.iisacc.congregation.canvas")},
                {QStringLiteral("version"), protocolVersion},
                {QStringLiteral("session"), m_sessionId},
                {QStringLiteral("peer"), m_peerId},
                {QStringLiteral("name"), m_localProfileName}};
            writeFrame(socket, MessageType::HelloAccepted, compactJson(accepted));
        }
        broadcastParticipantProfiles();
        if (!m_latestSnapshot.isEmpty())
        {
            sendLatestSnapshot(socket);
        }
        else
        {
            emit snapshotRequested();
        }
        return;
    }

    if (!iterator->accepted || type != MessageType::EditCommand)
    {
        disconnectHostPeer(socket, QStringLiteral("protocol"),
                           tr("The canvas connection sent an unexpected message."));
        return;
    }

    const std::optional<QVariantMap> command = decodeEditCommand(payload);
    if (!command)
    {
        disconnectHostPeer(socket, QStringLiteral("command"),
                           tr("The host canvas command is invalid or too large."));
        return;
    }
    emit editCommandReceived(*command, iterator->peerId);
}

void LocalCanvasSession::processClientFrame(quint8 rawType, const QByteArray& payload)
{
    const MessageType type = static_cast<MessageType>(rawType);
    if (type == MessageType::Error)
    {
        const std::optional<QJsonObject> error = jsonObject(payload);
        setErrorString(error ? error->value(QStringLiteral("message")).toString()
                             : tr("The host rejected the canvas connection."));
        if (m_clientSocket)
        {
            m_clientSocket->disconnectFromHost();
        }
        return;
    }
    if (type == MessageType::HelloAccepted)
    {
        if (m_state != SessionState::Joining)
        {
            if (m_clientSocket)
            {
                m_clientSocket->abort();
            }
            return;
        }
        const std::optional<QJsonObject> hello = jsonObject(payload);
        if (!hello ||
            hello->value(QStringLiteral("service")).toString() !=
                QStringLiteral("com.iisacc.congregation.canvas") ||
            hello->value(QStringLiteral("version")).toInt() != protocolVersion ||
            hello->value(QStringLiteral("session")).toString() != m_sessionId ||
            !isCanonicalUuid(hello->value(QStringLiteral("peer")).toString()))
        {
            setErrorString(tr("The host returned an invalid canvas handshake."));
            if (m_clientSocket)
            {
                m_clientSocket->abort();
            }
            return;
        }
        m_hostPeerId = hello->value(QStringLiteral("peer")).toString().toLower();
        m_connectTimer->stop();
        setErrorString({});
        setState(SessionState::Connected);
        return;
    }
    if (m_state != SessionState::Connected)
    {
        return;
    }
    if (type == MessageType::Participants)
    {
        applyClientParticipantProfiles(payload);
        return;
    }
    if (type != MessageType::SnapshotState)
    {
        setErrorString(tr("The host sent an unexpected canvas message."));
        if (m_clientSocket)
        {
            m_clientSocket->abort();
        }
        return;
    }

    quint64 incomingRevision = 0;
    QString originPeerId;
    QByteArray snapshot;
    if (!decodeSnapshotState(payload, incomingRevision, originPeerId, snapshot) ||
        !validSnapshot(snapshot))
    {
        setErrorString(tr("The host sent an invalid canvas update."));
        if (m_clientSocket)
        {
            m_clientSocket->abort();
        }
        return;
    }
    if (incomingRevision <= m_revision)
    {
        return;
    }
    m_revision = incomingRevision;
    emit revisionChanged();
    emit snapshotReceived(snapshot, incomingRevision, originPeerId);
}

void LocalCanvasSession::acceptSnapshot(const QByteArray& snapshot, const QString& originPeerId)
{
    if (!hosting() || m_revision == std::numeric_limits<quint64>::max())
    {
        return;
    }
    m_latestSnapshot = snapshot;
    m_latestOriginPeerId = originPeerId;
    ++m_revision;
    emit revisionChanged();
    if (originPeerId != m_peerId)
    {
        emit snapshotReceived(snapshot, m_revision, originPeerId);
    }

    const QByteArray payload = snapshotStatePayload(m_revision, originPeerId, snapshot);
    for (auto iterator = m_hostPeers.cbegin(); iterator != m_hostPeers.cend(); ++iterator)
    {
        if (iterator->accepted)
        {
            writeFrame(iterator.key(), MessageType::SnapshotState, payload);
        }
    }
}

void LocalCanvasSession::sendLatestSnapshot(QTcpSocket* socket) const
{
    if (m_latestSnapshot.isEmpty() || !isCanonicalUuid(m_latestOriginPeerId))
    {
        return;
    }
    writeFrame(socket, MessageType::SnapshotState,
               snapshotStatePayload(m_revision, m_latestOriginPeerId, m_latestSnapshot));
}

void LocalCanvasSession::disconnectHostPeer(QTcpSocket* socket, const QString& code,
                                            const QString& message)
{
    if (!socket)
    {
        return;
    }
    writeFrame(socket, MessageType::Error, compactJson(errorObject(code, message)));
    socket->disconnectFromHost();
    if (socket->state() != QAbstractSocket::UnconnectedState)
    {
        QTimer::singleShot(1000, socket, &QTcpSocket::abort);
    }
}

void LocalCanvasSession::resetTransport()
{
    m_connectTimer->stop();
    if (m_server)
    {
        m_server->close();
    }
    const QList<QTcpSocket*> hostSockets = m_hostPeers.keys();
    for (QTcpSocket* socket : hostSockets)
    {
        disconnectHostPeer(socket, QStringLiteral("ended"),
                           tr("The host stopped sharing this canvas."));
        socket->abort();
        socket->deleteLater();
    }
    m_hostPeers.clear();
    if (m_server)
    {
        delete m_server;
        m_server = nullptr;
    }
    if (m_clientSocket)
    {
        m_clientSocket->disconnect(this);
        m_clientSocket->abort();
        m_clientSocket->deleteLater();
        m_clientSocket = nullptr;
    }
    m_clientReceiveBuffer.clear();
}

bool LocalCanvasSession::validSnapshot(const QByteArray& snapshot) const
{
    return !snapshot.isEmpty() && snapshot.size() <= RecentCanvasMaximumContainerBytes &&
           decodeRecentCanvasContainer(snapshot).ok();
}
