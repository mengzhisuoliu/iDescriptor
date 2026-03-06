#ifndef HEARTBEATTHREAD_H
#define HEARTBEATTHREAD_H
#include "iDescriptor.h"
#include <QDebug>
#include <QThread>

using namespace IdeviceFFI;

class HeartbeatThread : public QThread
{
    Q_OBJECT
public:
    HeartbeatThread(HeartbeatClientHandle *heartbeat,
                    iDescriptor::Uniq macAddress, QObject *parent = nullptr)
        : QThread(parent), m_hb(Heartbeat::adopt(heartbeat)),
          m_macAddress(macAddress)
    {
    }

    void run() override
    {
        qDebug() << "Heartbeat thread started";
        try {
            u_int64_t interval = 15;

            while (!isInterruptionRequested()) {
                // 1. Wait for Marco with current interval
                Result result = m_hb.get_marco(interval);
                if (result.is_err()) {
                    qDebug()
                        << "Failed to get marco:"
                        << QString::fromStdString(result.unwrap_err().message);
                    m_tries++;
                    emit heartbeatFailed(m_macAddress, m_tries);
                    if (m_tries >= HEARTBEAT_RETRY_LIMIT) {
                        qDebug()
                            << "Maximum heartbeat retries reached, exiting for "
                               "device"
                            << m_macAddress;
                        emit heartbeatThreadExited(m_macAddress);
                        break;
                    }
                    // If get_marco failed, skip the rest of this iteration
                    // and try again with the current interval.
                    continue;
                }

                // 2. Get the new interval from device
                interval = result.unwrap();
                qDebug() << "Received marco, new interval:" << interval;

                // 3. Send Polo response
                Result polo_result = m_hb.send_polo();
                if (polo_result.is_err()) {
                    qDebug() << "Failed to send polo:"
                             << QString::fromStdString(
                                    polo_result.unwrap_err().message);
                    m_tries++;
                    emit heartbeatFailed(m_macAddress, m_tries);
                    if (m_tries >= HEARTBEAT_RETRY_LIMIT) {
                        qDebug() << "Maximum heartbeat retries reached, "
                                    "exiting for "
                                    "device"
                                 << m_macAddress;
                        emit heartbeatThreadExited(m_macAddress);
                        break;
                    }
                    // If send_polo failed, skip the rest of this iteration
                    // and try again with the current interval.
                    continue;
                }

                // If both marco and polo succeeded:
                qDebug() << "Sent polo successfully";
                interval += 5; // Increment interval for the next cycle
                m_initialCompleted = true; // Mark as initially completed after
                                           // first successful full cycle
            }
        } catch (const std::exception &e) {
            qDebug() << "Heartbeat error:" << e.what();

            emit heartbeatThreadExited(m_macAddress);
        }
    }

    bool initialCompleted() const { return m_initialCompleted; }

private:
    Heartbeat m_hb;
    bool m_initialCompleted = false;
    iDescriptor::Uniq m_macAddress;
    unsigned int m_tries = 0;

signals:
    void heartbeatFailed(const QString &macAddress, unsigned int tries = 0);
    void heartbeatThreadExited(const iDescriptor::Uniq &uniq);
};
#endif // HEARTBEATTHREAD_H