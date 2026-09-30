#include "fmdialogs/ElevationDialog.h"

#include <fmwidgets/Banner.h>
#include <fmwidgets/Button.h>
#include <fmwidgets/DialogCards.h>
#include <fmwidgets/DialogChrome.h>
#include <fmwidgets/DialogFooter.h>
#include <fmwidgets/DialogHeader.h>

#include <QAccessible>
#include <QButtonGroup>
#include <QCheckBox>
#include <QEvent>
#include <QHBoxLayout>
#include <QRadioButton>
#include <QVBoxLayout>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace fm::dialogs {

using elev::Choice;

namespace {

// 03 §1.2 · §1.3 · §1.12
constexpr int kClientWidth = 558;     // 창 560 − 좌우 테두리
constexpr int kFrameHeight = 38;      // 목업 제목 표시줄 36 + 테두리 2

} // namespace

ElevationDialog::ElevationDialog(const elev::PromptSpec &spec, QWidget *parent)
    : QDialog(parent)
    , m_spec(spec)
{
    fm::ui::DialogChromeOptions chrome;
    chrome.icon = fm::ui::glyph::Shield;
    chrome.fixedSize = false;
    fm::ui::setupDialogChrome(this, chrome);
    setWindowTitle(spec.windowTitle);
    setAccessibleName(spec.heading);
    setAccessibleDescription(spec.description);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    auto *body = new QWidget(this);
    body->setObjectName(u"body"_s);
    auto *layout = new QVBoxLayout(body);
    layout->setContentsMargins(24, 8, 24, 20);
    layout->setSpacing(16);
    root->addWidget(body, 1);

    // 머리 → 배너 → 키-값 → 목록 → 선택지 → 대안 → 체크 상자(03 §1.1 고정 순서, 없는 섹션은 만들지 않는다)
    auto *header = new fm::ui::DialogHeader(body);
    header->setObjectName(u"header"_s);
    header->setSubtitleWrap(true);
    header->setTone(spec.badgeTone);
    header->setGlyph(spec.badgeGlyph);
    header->setTitle(spec.heading);
    header->setSubtitle(spec.description);
    layout->addWidget(header);

    if (spec.banner) {
        auto *banner = new fm::ui::Banner(spec.banner->tone, spec.banner->glyph, spec.banner->text, body);
        banner->setObjectName(u"banner"_s);
        banner->setIconAlignment(spec.banner->alignTop ? fm::ui::Banner::IconTop : fm::ui::Banner::IconCenter);
        layout->addWidget(banner);
    }
    if (!spec.rows.isEmpty()) {
        auto *card = new fm::ui::KeyValueCard(body);
        card->setObjectName(u"details"_s);
        card->setRows(spec.rows);
        layout->addWidget(card);
    }
    if (!spec.items.isEmpty()) {
        auto *list = new fm::ui::ItemListCard(body);
        list->setObjectName(u"items"_s);
        list->setItems(spec.items);
        layout->addWidget(list);
    }
    if (!spec.options.isEmpty()) {
        auto *box = new QWidget(body);
        box->setObjectName(u"options"_s);
        box->setAccessibleName(tr("처리 방법"));
        auto *boxLayout = new QVBoxLayout(box);
        boxLayout->setContentsMargins(0, 0, 0, 0);
        boxLayout->setSpacing(8);
        m_options = new QButtonGroup(this);
        m_options->setExclusive(true);
        const bool described = std::any_of(spec.options.cbegin(), spec.options.cend(),
                                           [](const elev::OptionSpec &o) { return !o.description.isEmpty(); });
        for (int i = 0; i < spec.options.size(); ++i) {
            const elev::OptionSpec &option = spec.options.at(i);
            QAbstractButton *radio = nullptr;
            if (described) {
                auto *row = new fm::ui::OptionRadio(option.text, option.description, box);
                boxLayout->addWidget(row);
                radio = row->radio();
            } else {
                radio = new QRadioButton(option.text, box);
                boxLayout->addWidget(radio);
            }
            m_options->addButton(radio, i);
        }
        const int initial = std::clamp(spec.defaultOption, 0, int(spec.options.size()) - 1);
        m_options->button(initial)->setChecked(true);
        connect(m_options, &QButtonGroup::idToggled, this, [this](int, bool on) {
            if (on)
                syncPrimary();
        });
        layout->addWidget(box);
    }
    if (spec.altAction) {
        auto *card = new fm::ui::ActionCard(body);
        card->setObjectName(u"altAction"_s);
        card->setText(spec.altAction->title);
        card->setDetail(spec.altAction->detail);
        card->setIcon(spec.altAction->icon);
        const Choice choice = spec.altAction->choice;
        connect(card, &QAbstractButton::clicked, this, [this, choice] { finish(choice); });
        layout->addWidget(card);
    }
    if (spec.check) {
        m_check = new QCheckBox(spec.check->text, body);
        m_check->setObjectName(u"applyCheck"_s);
        m_check->setChecked(spec.check->checked);
        layout->addWidget(m_check);
    }
    layout->addStretch(1);

    // 바닥: [앞쪽 단추] — 늘임 — [뒤쪽 단추]. QDialogButtonBox는 순서를 다시 매기므로 쓰지 않는다(03 §1.12).
    auto *footer = new fm::ui::DialogFooter(this);
    footer->setObjectName(u"footer"_s);
    auto *footerLayout = new QHBoxLayout(footer);
    footerLayout->setContentsMargins(24, 16, 24, 16);
    footerLayout->setSpacing(8);
    root->addWidget(footer);
    auto addButton = [&](const elev::ButtonSpec &b) {
        auto *button = new fm::ui::Button(b.text, footer);
        button->setRole(b.primary ? fm::ui::Button::Primary : fm::ui::Button::Normal);
        button->setGlyph(b.shield ? fm::ui::glyph::Shield : fm::ui::glyph::None);
        button->setToolTip(b.toolTip);
        if (b.isDefault) {
            button->setDefault(true);
            m_default = button;
        }
        if (b.followsOption)
            m_follow = button;
        connect(button, &QPushButton::clicked, this, [this, b] {
            if (b.choice == Choice::Cancel)
                reject();
            else if (b.followsOption && m_options)
                finish(m_spec.options.at(selectedOption()).choice);
            else
                finish(b.choice);
        });
        footerLayout->addWidget(button);
        m_buttons.append({button, b});
    };
    for (const elev::ButtonSpec &b : spec.buttons) {
        if (b.placement == elev::ButtonSpec::Leading)
            addButton(b);
    }
    footerLayout->addStretch(1);
    for (const elev::ButtonSpec &b : spec.buttons) {
        if (b.placement == elev::ButtonSpec::Trailing)
            addButton(b);
    }

    syncPrimary();
    fitHeight();
}

ElevationDialog::~ElevationDialog() = default;

int ElevationDialog::selectedOption() const
{
    return m_options ? m_options->checkedId() : -1;
}

void ElevationDialog::selectOption(int index)
{
    if (m_options && m_options->button(index))
        m_options->button(index)->setChecked(true);
}

fm::ui::Button *ElevationDialog::button(Choice choice) const
{
    const int option = selectedOption();
    for (const auto &[button, spec] : m_buttons) {
        const Choice c = spec.followsOption && option >= 0 ? m_spec.options.at(option).choice : spec.choice;
        if (c == choice)
            return button;
    }
    return nullptr;
}

// 선택지에 따라 기본 단추의 글자 · 방패가 바뀐다(사전 확인 · 이동 — 03 §3.8)
void ElevationDialog::syncPrimary()
{
    const int option = selectedOption();
    if (!m_follow || option < 0)
        return;
    const elev::OptionSpec &spec = m_spec.options.at(option);
    m_follow->setText(spec.primaryText);
    m_follow->setGlyph(spec.primaryShield ? fm::ui::glyph::Shield : fm::ui::glyph::None);
}

// 폭 558 고정, 높이 = max(목업 − 38, 내용) — 번역 · 큰 글꼴이면 늘어난다(03 §1.14)
void ElevationDialog::fitHeight()
{
    if (!layout() || m_buttons.isEmpty())
        return;  // 조립 중(창 설정이 스타일 변경을 먼저 보낸다)
    setMinimumSize(0, 0);
    setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
    layout()->activate();
    const int content = layout()->hasHeightForWidth() ? layout()->totalHeightForWidth(kClientWidth)
                                                      : layout()->totalSizeHint().height();
    setFixedSize(kClientWidth, std::max(m_spec.designSize.height() - kFrameHeight, content));
}

void ElevationDialog::choose(Choice choice)
{
    if (choice == Choice::Cancel) {
        reject();
        return;
    }
    for (int i = 0; i < m_spec.options.size(); ++i) {
        if (m_spec.options.at(i).choice == choice)
            selectOption(i);
    }
    finish(choice);
}

void ElevationDialog::finish(Choice choice)
{
    if (m_finished)
        return;
    m_finished = true;
    m_result = {choice, m_check && m_check->isChecked()};
    Q_EMIT decided(m_result);
    accept();
}

void ElevationDialog::reject()
{
    if (!m_finished) {
        m_finished = true;
        m_result = {Choice::Cancel, m_check && m_check->isChecked()};
        Q_EMIT decided(m_result);
    }
    QDialog::reject();
}

void ElevationDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    // 처음 포커스 = 기본 단추(소유권 창은 건너뛰기) — 실수로 Enter를 눌러도 기본 동작만 일어난다
    if (m_default)
        m_default->setFocus(Qt::OtherFocusReason);
    QAccessibleEvent alert(this, QAccessible::Alert);
    QAccessible::updateAccessibility(&alert);
}

void ElevationDialog::changeEvent(QEvent *event)
{
    QDialog::changeEvent(event);
    if (event->type() == QEvent::FontChange || event->type() == QEvent::StyleChange)
        fitHeight();
}

ElevationDialog *ElevationDialog::ask(QWidget *parent, const elev::PromptSpec &spec, std::function<void(elev::Result)> done)
{
    auto *dialog = new ElevationDialog(spec, parent);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowModality(parent ? Qt::WindowModal : Qt::ApplicationModal);
    connect(dialog, &ElevationDialog::decided, dialog, [done = std::move(done)](const elev::Result &result) {
        if (done)
            done(result);
    });
    dialog->open();
    return dialog;
}

} // namespace fm::dialogs
