#include <regex>
#include <stdexcept>
#include <string>

struct ProductTypeVersion {
    int major;
    int minor;

    ProductTypeVersion(int maj = 0, int min = 0) : major(maj), minor(min) {}

    // Compare two product type versions
    // Returns: -1 if this < other, 0 if equal, 1 if this > other
    int compareTo(const ProductTypeVersion &other) const
    {
        if (major != other.major) {
            return major < other.major ? -1 : 1;
        }
        if (minor != other.minor) {
            return minor < other.minor ? -1 : 1;
        }
        return 0; // Equal
    }

    bool operator<(const ProductTypeVersion &other) const
    {
        return compareTo(other) < 0;
    }

    bool operator==(const ProductTypeVersion &other) const
    {
        return compareTo(other) == 0;
    }

    bool operator>(const ProductTypeVersion &other) const
    {
        return compareTo(other) > 0;
    }
};

namespace iDescriptor
{
/*
    Uniq is just a wrapper to get rid of mac and udid hell
*/
class Uniq
{
public:
    Uniq(const QString &uniq, bool isMac = false)
    {
        m_uniq = uniq;
        m_isMac = isMac;
    };

    Uniq(const std::string &uniq, bool isMac = false)
    {
        m_uniq = QString::fromStdString(uniq);
        m_isMac = isMac;
    };

    bool isMac() const { return m_isMac; };
    bool isUdid() const { return !m_isMac; }
    void set(const QString &uniq, bool isMac = false)
    {
        m_uniq = uniq;
        m_isMac = isMac;
    };
    void set(const std::string &uniq, bool isMac = false)
    {
        m_uniq = QString::fromStdString(uniq);
        m_isMac = isMac;
    };
    /* no need to set, unless needed */
    void setIP(const QString &ip) { m_ip = ip; }
    void setIP(const std::string &ip) { m_ip = QString::fromStdString(ip); }
    const QString &getIP() const { return m_ip; }
    const QString &get() const { return m_uniq; }
    operator QString() const { return m_uniq; }
    operator std::string() const { return m_uniq.toStdString(); }

private:
    QString m_uniq;
    bool m_isMac;
    QString m_ip;
};

class Utils
{
private:
    // Extract version numbers from iPhone product type string
    // Example: "iPhone8,1" -> ProductTypeVersion{8, 1}
    static ProductTypeVersion
    extractProductTypeVersion(const std::string &productType)
    {
        // Regex to match iPhone followed by major,minor numbers
        std::regex pattern(R"(iPhone(\d+),(\d+))");
        std::smatch matches;

        if (std::regex_search(productType, matches, pattern)) {
            if (matches.size() >= 3) {
                try {
                    int major = std::stoi(matches[1].str());
                    int minor = std::stoi(matches[2].str());
                    return ProductTypeVersion(major, minor);
                } catch (const std::invalid_argument &e) {
                    throw std::invalid_argument(
                        "Invalid numeric values in product type: " +
                        productType);
                }
            }
        }

        throw std::invalid_argument("Invalid iPhone product type format: " +
                                    productType);
    }

    static bool compare_product_type(const std::string &productType,
                                     const std::string &otherProductType)
    {
        try {
            ProductTypeVersion version1 =
                extractProductTypeVersion(productType);
            ProductTypeVersion version2 =
                extractProductTypeVersion(otherProductType);

            // Return true if productType is newer/higher than otherProductType
            return version1 > version2;
        } catch (const std::exception &e) {
            // Handle invalid product types - you might want to log this
            return false;
        }
    }

public:
    static bool isProductTypeNewer(const std::string &productType,
                                   const std::string &otherProductType)
    {
        return compare_product_type(productType, otherProductType);
    }

    static QString formatSize(uint64_t bytes)
    {
        const char *units[] = {"B", "KB", "MB", "GB", "TB"};
        int unitIndex = 0;
        double size = bytes;

        while (size >= 1024 && unitIndex < 4) {
            size /= 1024;
            unitIndex++;
        }

        return QString("%1 %2")
            .arg(QString::number(size, 'f', 1))
            .arg(units[unitIndex]);
    };

    static bool isVideoFile(const QString &fileName)
    {
        /* known iPhone video file extensions (AVI and MKV is not common but it
         * may be some from some app)*/
        return fileName.endsWith(".MOV", Qt::CaseInsensitive) ||
               fileName.endsWith(".MP4", Qt::CaseInsensitive) ||
               fileName.endsWith(".M4V", Qt::CaseInsensitive) ||
               fileName.endsWith(".AVI", Qt::CaseInsensitive) ||
               fileName.endsWith(".MKV", Qt::CaseInsensitive);
    }

    static bool isGalleryFile(const QString &fileName)
    {
        return fileName.endsWith(".JPG", Qt::CaseInsensitive) ||
               fileName.endsWith(".PNG", Qt::CaseInsensitive) ||
               fileName.endsWith(".HEIC", Qt::CaseInsensitive) ||
               fileName.endsWith(".MOV", Qt::CaseInsensitive) ||
               fileName.endsWith(".MP4", Qt::CaseInsensitive) ||
               fileName.endsWith(".M4V", Qt::CaseInsensitive);
    }

    static bool isPreviewableFile(const QString &fileName)
    {
        return isGalleryFile(fileName) || isVideoFile(fileName);
    }

    static QString formatFileSize(qint64 bytes)
    {
        const qint64 KB = 1024;
        const qint64 MB = KB * 1024;
        const qint64 GB = MB * 1024;

        if (bytes >= GB) {
            return QString("%1 GB").arg(
                QString::number(bytes / double(GB), 'f', 2));
        } else if (bytes >= MB) {
            return QString("%1 MB").arg(
                QString::number(bytes / double(MB), 'f', 1));
        } else if (bytes >= KB) {
            return QString("%1 KB").arg(
                QString::number(bytes / double(KB), 'f', 0));
        } else {
            return QString("%1 B").arg(bytes);
        }
    }

    static QString formatTransferRate(qint64 bytesPerSecond)
    {
        return formatFileSize(bytesPerSecond) + "/s";
    }
};
} // namespace iDescriptor