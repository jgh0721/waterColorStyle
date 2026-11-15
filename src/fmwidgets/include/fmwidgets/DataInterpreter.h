#pragma once
#include <QByteArrayView>
#include <QList>
#include <QPair>
#include <QWidget>

namespace fm::ui {
class Label;
class DataInterpreter : public QWidget
{
    Q_OBJECT
public:
    explicit DataInterpreter(QWidget* Parent = nullptr);
    void setData(QByteArrayView Data, qsizetype Offset = 0);
    // Short inputs remain explicitly unavailable; never pad missing bytes with zero.
    static QList<QPair<QString, QString>> Interpret(QByteArrayView Data);
private:
    Label* heading_;
    QList<Label*> values_;
};
}