#pragma once

#include "fmwidgets/Glyph.h"

#include <QStringList>
#include <QWidget>

class QAbstractButton;
class QButtonGroup;
class QHBoxLayout;

namespace fm::ui {

/// 이어 붙인 버튼 가운데 하나를 고르는 세그먼트 컨트롤 (목업의 1줄 · 2줄 · 자동 · 섬네일).
class SegmentedControl : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(QStringList items READ items WRITE setItems)
    Q_PROPERTY(int currentIndex READ currentIndex WRITE setCurrentIndex NOTIFY currentIndexChanged)
    Q_PROPERTY(Size size READ segmentSize WRITE setSegmentSize)
    Q_PROPERTY(bool expanding READ isExpanding WRITE setExpanding)
    Q_PROPERTY(QStringList itemToolTips READ itemToolTips WRITE setItemToolTips)

public:
    // Qt Designer · uic 호환을 위해 범위 없는 enum.
    /// Mini: 높이 20 · 11.5 px(그래프 축 전환), Small: 26 · 12 px(레코드 전환),
    /// Normal: 창 밀도를 따른다(대화상자 32 · 설정 30 · 메인 창 시안1 30 / 시안2 24).
    /// Compact: 메인 창 주소 줄의 보기 방식(시안1 28 · 12 px · 좌우 10, 시안2 24).
    enum Size { Mini, Small, Normal, Compact };
    Q_ENUM(Size)

    explicit SegmentedControl(QWidget *parent = nullptr);
    explicit SegmentedControl(const QStringList &items, QWidget *parent = nullptr);

    QStringList items() const { return m_items; }
    void setItems(const QStringList &items);

    int currentIndex() const noexcept { return m_current; }
    void setCurrentIndex(int index);

    Size segmentSize() const noexcept { return m_size; }
    void setSegmentSize(Size size);

    /// 조각을 같은 폭으로 늘려 가로를 채운다(다중 이름 변경의 확장자 대소문자).
    bool isExpanding() const noexcept { return m_expanding; }
    void setExpanding(bool expanding);

    QStringList itemToolTips() const { return m_toolTips; }
    void setItemToolTips(const QStringList &toolTips);

    /// 조각마다 아이콘(목업 글리프, 14 px). None이면 글자만. 색은 글자색을 따른다(켜짐 · 워터컬러 포함).
    QList<glyph::Glyph> itemGlyphs() const { return m_glyphs; }
    void setItemGlyphs(const QList<glyph::Glyph> &glyphs);

    int count() const noexcept { return int(m_items.size()); }
    QAbstractButton *button(int index) const;

Q_SIGNALS:
    void currentIndexChanged(int index);

protected:
    void changeEvent(QEvent *event) override;

private:
    void rebuild();
    void applyLook();
    void refreshIcons();

    QStringList m_items;
    QStringList m_toolTips;
    QList<glyph::Glyph> m_glyphs;
    int m_current = -1;
    Size m_size = Normal;
    bool m_expanding = false;
    QHBoxLayout *m_layout = nullptr;
    QButtonGroup *m_group = nullptr;
};

} // namespace fm::ui
