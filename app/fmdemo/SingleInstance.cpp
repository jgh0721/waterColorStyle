#include "SingleInstance.h"

#include <QCryptographicHash>
#include <QDir>
#include <QLocalServer>
#include <QLocalSocket>

using namespace Qt::StringLiterals;

namespace fm::app {

SingleInstance::SingleInstance(const QString &key, QObject *parent)
    : QObject(parent)
    , m_key(key)
{
}

SingleInstance::~SingleInstance() = default;

QString SingleInstance::defaultKey()
{
    const QByteArray hash = QCryptographicHash::hash(QDir::homePath().toUtf8(), QCryptographicHash::Sha1).toHex().left(12);
    return u"fmtools-fmdemo-"_s + QString::fromLatin1(hash);
}

bool SingleInstance::forward(const QString &message, int timeoutMs) const
{
    QLocalSocket socket;
    socket.connectToServer(m_key);
    if (!socket.waitForConnected(timeoutMs))
        return false;
    socket.write(message.toUtf8() + '\n');
    socket.flush();
    // 파이프 버퍼에 바로 들어가면 남은 바이트가 없다(waitForBytesWritten은 기다릴 것이 없으면 false)
    const bool written = socket.bytesToWrite() == 0 || socket.waitForBytesWritten(timeoutMs);
    socket.disconnectFromServer();
    return written;
}

bool SingleInstance::listen()
{
    if (m_server)
        return m_server->isListening();
    m_server = new QLocalServer(this);
    m_server->setSocketOptions(QLocalServer::UserAccessOption);
    if (!m_server->listen(m_key)) {
        // Unix에서는 비정상 종료로 남은 소켓 파일을 지우고 다시 — Windows의 이름 있는 파이프는 남지 않는다
        QLocalServer::removeServer(m_key);
        if (!m_server->listen(m_key))
            return false;
    }
    connect(m_server, &QLocalServer::newConnection, this, [this] {
        while (QLocalSocket *socket = m_server->nextPendingConnection()) {
            auto read = [this, socket] {
                while (socket->canReadLine())
                    Q_EMIT messageReceived(QString::fromUtf8(socket->readLine()).chopped(1));
            };
            connect(socket, &QLocalSocket::readyRead, this, read);
            connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
            read();  // 연결을 받기 전에 이미 도착한 줄
        }
    });
    return true;
}

bool SingleInstance::isListening() const
{
    return m_server && m_server->isListening();
}

} // namespace fm::app
