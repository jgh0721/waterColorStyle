#include "fmdialogs/ProgressSimulator.h"

#include <QTimer>

#include <cmath>
#include <numbers>

using namespace Qt::StringLiterals;

namespace fm::dialogs {

namespace {
constexpr double kTick = 0.5;           // 초
constexpr double kTau = 1.2;            // 평활 시간 상수(초)
constexpr double kMB = 1024.0 * 1024.0;
} // namespace

ProgressSimulator::ProgressSimulator(QObject *parent)
    : QObject(parent)
    , m_timer(new QTimer(this))
{
    m_timer->setInterval(int(kTick * 1000));
    connect(m_timer, &QTimer::timeout, this, [this] {
        advance(kTick);
        Q_EMIT ticked();
    });
}

void ProgressSimulator::setFiles(const QList<qint64> &sizes)
{
    m_files = sizes;
    m_total = 0;
    for (qint64 s : sizes)
        m_total += s;
    reset();
}

void ProgressSimulator::reset()
{
    m_bytes = 0;
    m_bytesExact = 0.0;
    m_ema = 0.0;
    m_t = 0.0;
    m_active = 0.0;
    m_finished = false;
    m_samples.clear();
    m_samples.append({0, 0, m_paused});
}

void ProgressSimulator::preroll(double seconds)
{
    for (double s = 0; s < seconds - 1e-9; s += kTick)
        advance(kTick);
}

void ProgressSimulator::prerollTo(qint64 bytes, double maxSeconds)
{
    const qint64 target = std::min(bytes, m_total > 0 ? m_total - 1 : bytes);
    for (double s = 0; s < maxSeconds - 1e-9 && m_bytes < target && !m_finished; s += kTick)
        advance(kTick);
}

void ProgressSimulator::start()
{
    if (m_finished)
        reset();
    m_timer->start();
}

void ProgressSimulator::stop()
{
    m_timer->stop();
}

bool ProgressSimulator::isRunning() const
{
    return m_timer->isActive();
}

void ProgressSimulator::setPaused(bool paused)
{
    m_paused = paused;
}

double ProgressSimulator::rawSpeed(double t)
{
    double v = 172.0 + 14.0 * std::sin(t / 1.7) + 9.0 * std::sin(t / 0.9 + 1.0);
    const double c = std::fmod(t, 20.0);
    if (c > 11.0 && c < 14.0)
        v -= 70.0 * std::sin((c - 11.0) / 3.0 * std::numbers::pi);   // 느려짐
    if (c > 16.0 && c < 18.0)
        v += 28.0 * std::sin((c - 16.0) / 2.0 * std::numbers::pi);   // 빨라짐
    return std::max(0.0, v * std::min(1.0, t / 1.5));
}

void ProgressSimulator::advance(double dt)
{
    if (m_finished)
        return;
    m_t += dt;
    const double raw = m_paused ? 0.0 : rawSpeed(m_active + dt) * kMB;
    m_ema += (1.0 - std::exp(-dt / kTau)) * (raw - m_ema);
    if (!m_paused) {
        m_bytesExact += m_ema * dt;
        m_active += dt;
    }
    m_bytes = qint64(m_bytesExact);
    if (m_total > 0 && m_bytes >= m_total) {
        if (m_loop) {
            reset();
            Q_EMIT restarted();
            return;
        }
        m_bytes = m_total;
        m_bytesExact = double(m_total);
        m_samples.append({m_bytes, qint64(m_t * 1000), m_paused});
        m_timer->stop();
        m_finished = true;
        Q_EMIT finished();
        return;
    }
    m_samples.append({m_bytes, qint64(m_t * 1000), m_paused});
}

double ProgressSimulator::average() const noexcept
{
    return m_active > 0 ? double(m_bytes) / m_active : 0.0;
}

int ProgressSimulator::percent() const noexcept
{
    return m_total > 0 ? int(std::floor(100.0 * double(m_bytes) / double(m_total))) : 0;
}

int ProgressSimulator::fileIndex() const noexcept
{
    qint64 sum = 0;
    for (int i = 0; i < m_files.size(); ++i) {
        sum += m_files.at(i);
        if (m_bytes < sum)
            return i;
    }
    return std::max(0, int(m_files.size()) - 1);
}

qint64 ProgressSimulator::fileDone() const noexcept
{
    qint64 before = 0;
    const int index = fileIndex();
    for (int i = 0; i < index; ++i)
        before += m_files.at(i);
    return std::max<qint64>(0, m_bytes - before);
}

qint64 ProgressSimulator::fileSize() const noexcept
{
    const int index = fileIndex();
    return index < m_files.size() ? m_files.at(index) : 0;
}

QString ProgressSimulator::remainingText() const
{
    const double avg = average();
    if (m_paused || avg <= 0)
        return u"—"_s;
    const double remain = double(m_total - m_bytes) / avg;
    if (remain < 60)
        return tr("약 %1초").arg(std::max(1, int(std::lround(remain))));
    return tr("약 %1분").arg(int(std::lround(remain / 60.0)));
}

QString ProgressSimulator::elapsedText() const
{
    const int seconds = int(m_t);
    return u"%1:%2"_s.arg(seconds / 60, 2, 10, u'0').arg(seconds % 60, 2, 10, u'0');
}

} // namespace fm::dialogs
