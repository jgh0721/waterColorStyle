#pragma once

// 일괄 스냅숏(PLAN §9) — 메인 창과 대화상자 변형 전부를 디자인 · 변형 5조합으로 1배율 PNG로 저장하고,
// 목업식 제목 표시줄 틀을 입힌 판과 훑어보기용 index.html을 만든다.
//   fmdemo --shot <폴더> [--only copy,settings.keys,main] [--themes std-light,wc-navy]

#include <QStringList>

class QIcon;
class QImage;
class QString;

namespace fm::app {

struct SnapshotOptions
{
    QString dir;
    QStringList only;     // 화면 ID 접두어(비면 전부). "main" = 메인 창
    QStringList themes;   // std-light · std-dark · wc-light · wc-dark · wc-navy (비면 전부)
    int settleMs = 250;   // 보이고 나서 찍을 때까지(설정 창은 더 길게)
};

/// 끝나면 종료 코드(0 = 성공).
int runSnapshots(const SnapshotOptions &options);

/// 목업식 창 틀 — 시안1은 36 px(메인 32) 제목 · 모서리 8 · 1 px --line, 시안2는 27 px 그라데이션 제목(CC_TitleBar) · 3 px 파란 틀.
/// 지금 ThemeManager의 디자인 · 색을 쓴다.
QImage mockFrame(const QImage &client, const QString &title, const QIcon &icon, bool mainWindow);

} // namespace fm::app
