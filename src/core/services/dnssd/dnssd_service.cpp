/*
 * iDescriptor: A free and open-source idevice management tool.
 *
 * Copyright (C) 2025 Uncore <https://github.com/uncor3>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include "dnssd_service.h"
#include <QDebug>
#include <QMutexLocker>
#include <cstring>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <arpa/inet.h>
#include <sys/select.h>
#include <unistd.h>
#endif

DnssdService::DnssdService(QObject *parent)
    : QObject(parent), m_browseRef(nullptr), m_socketNotifier(nullptr),
      m_running(false)
{
}

DnssdService::~DnssdService() { stopBrowsing(); }

void DnssdService::startBrowsing()
{
    if (m_running)
        return;

    qDebug() << "Starting DNS-SD browsing for Apple devices";

    DNSServiceErrorType err =
        DNSServiceBrowse(&m_browseRef, 0, 0, "_apple-mobdev2._tcp", "local.",
                         browseCallback, this);

    if (err != kDNSServiceErr_NoError) {
        qWarning() << "DNSServiceBrowse failed:" << err;
        return;
    }

    int fd = DNSServiceRefSockFD(m_browseRef);
    m_socketNotifier = new QSocketNotifier(fd, QSocketNotifier::Read, this);
    connect(m_socketNotifier, &QSocketNotifier::activated, this,
            &DnssdService::processDnssdEvents);

    m_running = true;
}

void DnssdService::stopBrowsing()
{
    if (!m_running)
        return;

    qDebug() << "Stopping DNS-SD browsing";
    m_running = false;
    cleanupDnssd();

    QMutexLocker locker(&m_devicesMutex);
    m_networkDevices.clear();
    m_pendingDevices.clear();
}

QList<NetworkDevice> DnssdService::getNetworkDevices() const
{
    QMutexLocker locker(&m_devicesMutex);
    return m_networkDevices;
}

void DnssdService::processDnssdEvents()
{
    if (m_browseRef && m_running) {
        DNSServiceProcessResult(m_browseRef);
    }
}

void DnssdService::cleanupDnssd()
{
    if (m_socketNotifier) {
        m_socketNotifier->deleteLater();
        m_socketNotifier = nullptr;
    }

    if (m_browseRef) {
        DNSServiceRefDeallocate(m_browseRef);
        m_browseRef = nullptr;
    }
}

void DNSSD_API DnssdService::browseCallback(
    DNSServiceRef sdRef, DNSServiceFlags flags, uint32_t interfaceIndex,
    DNSServiceErrorType errorCode, const char *serviceName, const char *regtype,
    const char *replyDomain, void *context)
{
    Q_UNUSED(sdRef)
    Q_UNUSED(regtype)
    Q_UNUSED(replyDomain)

    if (errorCode != kDNSServiceErr_NoError)
        return;

    DnssdService *service = static_cast<DnssdService *>(context);

    if (flags & kDNSServiceFlagsAdd) {
        // Start resolving the service
        DNSServiceRef resolveRef;
        DNSServiceErrorType err =
            DNSServiceResolve(&resolveRef, 0, interfaceIndex, serviceName,
                              regtype, replyDomain, resolveCallback, context);

        if (err == kDNSServiceErr_NoError) {
            // Process the resolve synchronously for simplicity
            int fd = DNSServiceRefSockFD(resolveRef);
            fd_set readfds;
            FD_ZERO(&readfds);
            FD_SET(fd, &readfds);
            struct timeval timeout = {5, 0}; // 5 second timeout
            if (select(fd + 1, &readfds, nullptr, nullptr, &timeout) > 0) {
                if (FD_ISSET(fd, &readfds)) {
                    DNSServiceProcessResult(resolveRef);
                }
            }
            DNSServiceRefDeallocate(resolveRef);
        }
    } else {
        qDebug() << "Apple device removed:" << serviceName;
        emit service->deviceRemoved(QString::fromUtf8(serviceName));

        // Remove from our list
        QMutexLocker locker(&service->m_devicesMutex);
        service->m_networkDevices.removeIf(
            [serviceName](const NetworkDevice &dev) {
                return dev.name == QString::fromUtf8(serviceName);
            });
        service->m_pendingDevices.remove(QString::fromUtf8(serviceName));
    }
}

void DNSSD_API DnssdService::resolveCallback(
    DNSServiceRef sdRef, DNSServiceFlags flags, uint32_t interfaceIndex,
    DNSServiceErrorType errorCode, const char *fullname, const char *hosttarget,
    uint16_t port, uint16_t txtLen, const unsigned char *txtRecord,
    void *context)
{
    Q_UNUSED(sdRef)
    Q_UNUSED(flags)

    if (errorCode != kDNSServiceErr_NoError)
        return;

    DnssdService *service = static_cast<DnssdService *>(context);
    QString serviceName = QString::fromUtf8(fullname);

    // Store pending device info
    PendingDevice pending;
    pending.name = serviceName;
    pending.hostname = QString::fromUtf8(hosttarget);
    pending.port = ntohs(port);
    pending.interfaceIndex = interfaceIndex;
    pending.macAddress = serviceName.split('@').first();

    // Parse TXT records
    if (txtLen > 0 && txtRecord) {
        const unsigned char *ptr = txtRecord;
        const unsigned char *end = txtRecord + txtLen;

        while (ptr < end) {
            uint8_t len = *ptr++;
            if (ptr + len > end)
                break;

            QString record =
                QString::fromUtf8(reinterpret_cast<const char *>(ptr), len);
            int equalPos = record.indexOf('=');
            if (equalPos != -1) {
                QString key = record.left(equalPos);
                QString value = record.mid(equalPos + 1);
                pending.txt[key] = value;
            }
            ptr += len;
        }
    }

    service->m_pendingDevices[serviceName] = pending;

    qDebug() << "Resolved Apple device:" << serviceName
             << "host:" << pending.hostname << "port:" << pending.port;

    // Now resolve the IP address
    DNSServiceRef addrRef;
    DNSServiceErrorType err = DNSServiceGetAddrInfo(
        &addrRef, 0, interfaceIndex, kDNSServiceProtocol_IPv4, hosttarget,
        addrInfoCallback, context);

    if (err == kDNSServiceErr_NoError) {
        // Process the address resolution synchronously
        int fd = DNSServiceRefSockFD(addrRef);
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(fd, &readfds);
        struct timeval timeout = {5, 0}; // 5 second timeout
        if (select(fd + 1, &readfds, nullptr, nullptr, &timeout) > 0) {
            if (FD_ISSET(fd, &readfds)) {
                DNSServiceProcessResult(addrRef);
            }
        }
        DNSServiceRefDeallocate(addrRef);
    }
}

void DNSSD_API DnssdService::addrInfoCallback(
    DNSServiceRef sdRef, DNSServiceFlags flags, uint32_t interfaceIndex,
    DNSServiceErrorType errorCode, const char *hostname,
    const struct sockaddr *address, uint32_t ttl, void *context)
{
    Q_UNUSED(sdRef)
    Q_UNUSED(flags)
    Q_UNUSED(interfaceIndex)
    Q_UNUSED(ttl)

    if (errorCode != kDNSServiceErr_NoError)
        return;

    DnssdService *service = static_cast<DnssdService *>(context);
    QString hostnameStr = QString::fromUtf8(hostname);

    // Find the pending device with matching hostname
    QString deviceName;
    for (auto it = service->m_pendingDevices.begin();
         it != service->m_pendingDevices.end(); ++it) {
        if (it.value().hostname == hostnameStr) {
            deviceName = it.key();
            break;
        }
    }

    if (deviceName.isEmpty()) {
        qWarning() << "Could not find pending device for hostname:"
                   << hostnameStr;
        return;
    }

    PendingDevice pending = service->m_pendingDevices[deviceName];

    // Convert IP address
    char ip[INET_ADDRSTRLEN];
    auto *addr_in = reinterpret_cast<const struct sockaddr_in *>(address);
    inet_ntop(AF_INET, &addr_in->sin_addr, ip, sizeof(ip));

    NetworkDevice device;
    // Extract a better device name from hostname or use TXT records
    QString friendlyName = pending.hostname;
    if (friendlyName.endsWith(".local.")) {
        friendlyName =
            friendlyName.left(friendlyName.length() - 7); // Remove ".local."
        qDebug() << "friendly name:" << friendlyName;
    }

    // Try to get device name from TXT records first
    if (pending.txt.contains("DvNm")) {
        device.name = pending.txt["DvNm"];
        qDebug() << "Device name from DvNm TXT record:" << device.name;
    } else if (pending.txt.contains("Name")) {
        device.name = pending.txt["Name"];
        qDebug() << "Device name from Name TXT record:" << device.name;
    } else {
        // Use the cleaned hostname as fallback
        qDebug() << "Using hostname as device name:" << friendlyName;
        device.name = friendlyName;
    }

    device.hostname = pending.hostname;
    device.address = QString::fromUtf8(ip);
    device.port = pending.port > 0 ? pending.port : 22; // Default to SSH port
    device.macAddress = pending.macAddress;

    qDebug() << "Resolved IP for Apple device:" << device.name << "at"
             << device.address << ":" << device.port;

    // Add to our list if not already present
    {
        QMutexLocker locker(&service->m_devicesMutex);
        bool exists = std::any_of(service->m_networkDevices.begin(),
                                  service->m_networkDevices.end(),
                                  [&device](const NetworkDevice &existing) {
                                      return existing == device;
                                  });
        if (!exists) {
            service->m_networkDevices.append(device);
            emit service->deviceAdded(device);
        }
    }

    // Remove from pending
    service->m_pendingDevices.remove(deviceName);
}