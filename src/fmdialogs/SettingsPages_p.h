#pragma once

// 설정 페이지 9개 — 설정 창(SettingsDialog)만 쓰는 내부 헤더.

#include "fmdialogs/SettingsDialog.h"

namespace fm::dialogs {

SettingsPage *createAppearancePage(SettingsSession *session, QWidget *parent);
SettingsPage *createThemePage(SettingsSession *session, QWidget *parent);
SettingsPage *createPanelPage(SettingsSession *session, QWidget *parent);
SettingsPage *createThumbsPage(SettingsSession *session, QWidget *parent);
SettingsPage *createGroupsPage(SettingsSession *session, QWidget *parent);
SettingsPage *createColumnsPage(SettingsSession *session, QWidget *parent);
SettingsPage *createFileOpsPage(SettingsSession *session, QWidget *parent);
SettingsPage *createElevationPage(SettingsSession *session, QWidget *parent);
SettingsPage *createKeysPage(SettingsSession *session, QWidget *parent);

/// 탐색 아이콘(04 §1.3 — 16 px 선 1.3).
void paintSettingsNavIcon(QPainter *painter, const QString &pageId, const QRectF &rect, const QColor &color);

} // namespace fm::dialogs
