#include "fmwidgets/DataInterpreter.h"
#include <fmwidgets/Label.h>
#include <QDateTime>
#include <QGridLayout>
#include <QtEndian>
#include <QTimeZone>

namespace fm::ui {
namespace {
QString Hex(quint64 Value, int Digits) { return "0x" + QString::number(Value, 16).rightJustified(Digits, '0').toUpper(); }
}
DataInterpreter::DataInterpreter(QWidget* Parent) : QWidget(Parent)
{
    auto* Layout = new QGridLayout(this); Layout->setContentsMargins(0, 0, 0, 0);
    heading_ = new Label(tr("데이터 해석"), Label::Heading, this); Layout->addWidget(heading_, 0, 0, 1, 2);
    int Row = 1;
    for (const auto& Item : Interpret({})) {
        Layout->addWidget(new Label(Item.first, Label::FieldLabel, this), Row, 0);
        auto* Value = new Label(Item.second, Label::PathMeta, this); Value->setTextFormat(Qt::PlainText);
        Value->setTextInteractionFlags(Qt::TextSelectableByMouse); Value->setWordWrap(true);
        values_.append(Value); Layout->addWidget(Value, Row++, 1);
    }
    Layout->setColumnStretch(1, 1);
}
QList<QPair<QString, QString>> DataInterpreter::Interpret(QByteArrayView Data)
{
    const auto Missing = tr("바이트 부족");
    QList<QPair<QString, QString>> Rows{{"DWORD", Missing}, {"QWORD", Missing}, {"FILETIME", Missing}, {"UTF-16", Missing}, {"GUID", Missing}, {"SID", Missing}};
    if (Data.size() >= 4) {
        const auto Value = qFromLittleEndian<quint32>(Data.data()); Rows[0].second = Hex(Value, 8) + " (" + QString::number(Value) + ')';
    }
    if (Data.size() >= 8) {
        const auto Value = qFromLittleEndian<quint64>(Data.data()); Rows[1].second = Hex(Value, 16);
        if (!Value) Rows[2].second = tr("0 (시각 없음)");
        else {
            const auto Date = QDateTime::fromMSecsSinceEpoch(qint64(Value / 10000) - 11644473600000LL, QTimeZone::UTC);
            Rows[2].second = Date.isValid() && Date.date().year() >= 1601 && Date.date().year() <= 9999
                ? Date.toString("yyyy-MM-dd HH:mm:ss.zzz 'UTC'") : tr("날짜 범위 밖");
        }
    }
    if (Data.size() >= 2) {
        QString Text;
        for (qsizetype I = 0; I + 1 < Data.size() && I < 64; I += 2) {
            const auto Unit = qFromLittleEndian<quint16>(Data.data() + I); if (!Unit) break;
            const QChar Character(Unit); Text.append(Character.isPrint() || Character.isSurrogate() ? Character : QChar(0x00b7));
        }
        Rows[3].second = Text.isEmpty() ? tr("(비어 있음)") : Text;
    }
    if (Data.size() >= 16) {
        const auto A = qFromLittleEndian<quint32>(Data.data());
        const auto B = qFromLittleEndian<quint16>(Data.data() + 4), C = qFromLittleEndian<quint16>(Data.data() + 6);
        const auto Tail = QByteArray(Data.data() + 8, 8).toHex().toUpper();
        Rows[4].second = QString("{%1-%2-%3-%4-%5}").arg(QString::number(A, 16).rightJustified(8, '0').toUpper(),
            QString::number(B, 16).rightJustified(4, '0').toUpper(), QString::number(C, 16).rightJustified(4, '0').toUpper(),
            QString::fromLatin1(Tail.left(4)), QString::fromLatin1(Tail.mid(4)));
    }
    if (Data.size() >= 8) {
        const auto Revision = quint8(Data[0]), Count = quint8(Data[1]);
        if (Revision == 1 && Count <= 15 && Data.size() >= 8 + qsizetype(Count) * 4) {
            quint64 Authority = 0; for (int I = 2; I < 8; ++I) Authority = (Authority << 8) | quint8(Data[I]);
            QString Sid = "S-1-" + QString::number(Authority);
            for (int I = 0; I < Count; ++I) Sid += '-' + QString::number(qFromLittleEndian<quint32>(Data.data() + 8 + I * 4));
            Rows[5].second = Sid;
        } else Rows[5].second = tr("올바른 SID 형식 아님");
    }
    return Rows;
}
void DataInterpreter::setData(QByteArrayView Data, qsizetype Offset)
{
    Offset = qBound<qsizetype>(0, Offset, Data.size());
    heading_->setText(tr("데이터 해석 · 오프셋 %1").arg(Hex(quint64(Offset), 4)));
    const auto Rows = Interpret(Data.sliced(Offset, qMin<qsizetype>(68, Data.size() - Offset)));
    for (int I = 0; I < values_.size(); ++I) values_[I]->setText(Rows[I].second);
}
}