#pragma once

// 대화상자 · 변형 목록 — 목업 보드의 상태로 채운 대화상자를 변형 ID로 만든다(데모 --open · 스냅숏 · 테스트 · 카탈로그 창).

#include <QList>
#include <QSize>
#include <QString>

class QDialog;
class QWidget;

namespace fm::dialogs {

struct DialogVariant
{
    QString id;          // "delete.permanent"
    QString dialog;      // "delete" — 대화상자 묶음
    QString label;       // "영구 삭제"
    bool fromMockup = false;
    QSize client;        // 목업 클라이언트 크기(비교용)
};

QList<DialogVariant> dialogVariants();

/// 변형 ID로 대화상자를 만든다(부모 · 모덜리스). 모르는 ID면 nullptr.
/// "copy"처럼 묶음 이름만 주면 그 묶음의 첫 변형(목업 기본 상태).
QDialog *createDialog(const QString &id, QWidget *parent = nullptr);

} // namespace fm::dialogs
