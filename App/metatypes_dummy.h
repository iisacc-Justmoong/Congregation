#pragma once

#include <QObject>
#include <qqml.h>

// Anonymous QObject-derived type keeps the QML metatype pipeline happy
// without exposing additional QML API.
class CongregationMetaTypesDummy : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("QML.Element", "CongregationMetaTypesDummy")
    Q_CLASSINFO("QML.Creatable", "false")
    Q_CLASSINFO("QML.UncreatableReason", "Internal placeholder for metatype generation")

public:
    explicit CongregationMetaTypesDummy(QObject *parent = nullptr);
    ~CongregationMetaTypesDummy() override;
};
