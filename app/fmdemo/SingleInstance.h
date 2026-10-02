#pragma once

// 창 하나만 실행(설정 › 일반 › 창을 하나만 실행 — 04 §4.6) — 이름 있는 로컬 소켓(QLocalServer)으로
// 먼저 뜬 인스턴스에 요청을 넘긴다. 다시 실행하면 먼저 뜬 창이 새 탭으로 연다.

#include <QObject>
#include <QString>

class QLocalServer;

namespace fm::app {

class SingleInstance : public QObject
{
    Q_OBJECT
public:
    explicit SingleInstance(const QString &key = defaultKey(), QObject *parent = nullptr);
    ~SingleInstance() override;

    /// 이미 떠 있는 인스턴스에 message(열 폴더, 비면 새 탭만)를 넘겼으면 true — 이 프로세스는 끝내면 된다.
    bool forward(const QString &message, int timeoutMs = 1000) const;
    /// 첫 인스턴스로 듣기 시작한다(다른 인스턴스가 먼저 듣고 있으면 false).
    bool listen();
    bool isListening() const;

    /// 사용자마다 다른 이름(홈 폴더 경로의 해시 — 경로 자체는 이름에 넣지 않는다).
    static QString defaultKey();

Q_SIGNALS:
    void messageReceived(const QString &message);

private:
    QString m_key;
    QLocalServer *m_server = nullptr;
};

} // namespace fm::app
