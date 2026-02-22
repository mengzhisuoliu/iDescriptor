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

#ifndef INFOLABEL_H
#define INFOLABEL_H

#include <QLabel>
#include <QString>
#include <QTimer>

class InfoLabel : public QLabel
{
    Q_OBJECT

public:
    explicit InfoLabel(const QString &text = QString(),
                       const QString &textToCopy = QString(),
                       QWidget *parent = nullptr);

    void setOriginalText(const QString &text);
    void setTextToCopy(const QString &textToCopy);

protected:
    void mousePressEvent(QMouseEvent *event) override;

private slots:
    void restoreOriginalText();

private:
    QString m_originalText;
    QString m_textToCopy;
    QTimer *m_restoreTimer;
    QString m_style =
#ifdef WIN32
        QStringLiteral(
            "QLabel:hover { background-color: rgba(255, 255, 255, 0.1); "
            "border-radius: 2px; }"
            "QLabel { "
            "font-size: 14px;}");
#else
        QStringLiteral(
            "QLabel:hover { background-color: rgba(255, 255, 255, 0.1); "
            "border-radius: 2px; }");
#endif
};

#endif // INFOLABEL_H