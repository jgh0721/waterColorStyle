#include "fmstyle/PaperFluentStyle.h"

namespace fm::style {

// 그리기는 FmStyle이 디자인(Design::PaperFluent)을 보고 나눈다 — 시안1과 같은 구조를 두 벌로 두지 않으려고.
PaperFluentStyle::PaperFluentStyle()
    : FmStyle(Design::PaperFluent)
{
}

PaperFluentStyle::~PaperFluentStyle() = default;

} // namespace fm::style
