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

#include "logindialog.h"
#include "appstoremanager.h"
#include "qprocessindicator.h"
#include <QApplication>
#include <QDialogButtonBox>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrent>

#ifdef WIN32
#include "platform/windows/win_common.h"
#endif

LoginDialog::LoginDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle("Login to App Store - iDescriptor");
    setModal(true);
    setFixedWidth(400);

#ifdef WIN32
    setupWinWindow(this);
#endif

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setSpacing(15);
    layout->setContentsMargins(20, 20, 20, 20);

    // Email
    QLabel *emailLabel = new QLabel("Email:");
    emailLabel->setStyleSheet("font-size: 14px");
    layout->addWidget(emailLabel);

    m_emailEdit = new QLineEdit();
    m_emailEdit->setPlaceholderText("Enter your email");
#ifndef WIN32
    m_emailEdit->setStyleSheet("padding: 8px; border: 1px solid #ddd; "
                               "border-radius: 4px; font-size: 14px;");
#endif

    layout->addWidget(m_emailEdit);

    // Password
    QLabel *passwordLabel = new QLabel("Password:");
    passwordLabel->setStyleSheet("font-size: 14px");
    layout->addWidget(passwordLabel);

    m_passwordEdit = new QLineEdit();
    m_passwordEdit->setPlaceholderText("Enter your password");
    m_passwordEdit->setEchoMode(QLineEdit::Password);
#ifndef WIN32
    m_passwordEdit->setStyleSheet("padding: 8px; border: 1px solid #ddd; "
                                  "border-radius: 4px; font-size: 14px;");
#endif
    layout->addWidget(m_passwordEdit);

    // Description
    QLabel *descriptionLabel = new QLabel(
        "You shouldn't be using your main account here and don't worry, "
        "your credentials won't be "
        "stored or shared anywhere. This App is open-source.");
    descriptionLabel->setStyleSheet("font-size: 10px; font-weight: thin;");
    descriptionLabel->setAlignment(Qt::AlignLeft);
    descriptionLabel->setWordWrap(true);
    layout->addWidget(descriptionLabel);

    QWidget *signInContainer = new QWidget(this);
    m_signInStackedLayout = new QStackedLayout(signInContainer);
    m_signInStackedLayout->setContentsMargins(0, 0, 0, 0);

    // Create the actual "Sign In" button
    m_signInButton = new QPushButton("Sign In");
#ifndef WIN32
    m_signInButton->setStyleSheet(
        "QPushButton { padding: 8px 16px; font-size: 14px; border-radius: 4px; "
        "background-color: #007AFF; color: white; border: none; min-width: "
        "80px; }"
        "QPushButton:hover { background-color: #0056CC; }");
#endif
    // Create the indicator
    QWidget *indicatorWidget = new QWidget();
    QVBoxLayout *indicatorLayout = new QVBoxLayout(indicatorWidget);
    indicatorLayout->setContentsMargins(0, 0, 0, 0);
    indicatorLayout->setAlignment(Qt::AlignCenter);
    m_indicator = new QProcessIndicator(this);
    m_indicator->setType(QProcessIndicator::line_rotate);
    m_indicator->setFixedSize(48, 24);
    indicatorLayout->addWidget(m_indicator);

    // Add button and indicator to the stacked layout
    m_signInStackedLayout->addWidget(m_signInButton);
    m_signInStackedLayout->addWidget(indicatorWidget);

    // Ensure the container has the same size as the button
    signInContainer->setFixedSize(m_signInButton->sizeHint());

    m_cancelButton = new QPushButton("Cancel");
#ifndef WIN32
    m_cancelButton->setStyleSheet(
        "QPushButton { padding: 8px 16px; font-size: 14px; border-radius: 4px; "
        "background-color: #f0f0f0; color: #333; border: 1px solid #ddd; "
        "min-width: 80px; }"
        "QPushButton:disabled { background-color: #eee; color: #aaa; border: "
        "1px solid #ddd; }");
#endif

    // Layout for the buttons
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_cancelButton);
    buttonLayout->addWidget(signInContainer);

    connect(m_signInButton, &QPushButton::clicked, this, &LoginDialog::signIn);
    connect(m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);

    layout->addLayout(buttonLayout);
}

QString LoginDialog::getEmail() const { return m_emailEdit->text(); }
QString LoginDialog::getPassword() const { return m_passwordEdit->text(); }

void LoginDialog::signIn()
{
    QString email = getEmail();
    QString password = getPassword();
    if (email.isEmpty() || password.isEmpty()) {
        QMessageBox::warning(this, "Login Failed",
                             "Email and password cannot be empty.");
        return;
    }

    AppStoreManager *manager = AppStoreManager::sharedInstance();
    if (!manager) {
        QMessageBox::critical(this, "Error",
                              "Failed to initialize App Store manager.");
        return;
    }

    // Show indicator and disable cancel button
    m_signInStackedLayout->setCurrentIndex(1);
    m_indicator->start();
    m_cancelButton->setEnabled(false);

    manager->loginWithCallback(
        email, password, [this](bool success, const QJsonObject &accountInfo) {
            // Hide indicator and re-enable buttons
            m_indicator->stop();
            m_signInStackedLayout->setCurrentIndex(0);
            m_cancelButton->setEnabled(true);

            if (success) {
                qDebug() << "Login successful";
                accept();
            } else {
                QMessageBox::warning(this, "Login Failed",
                                     "Login failed. Please check your "
                                     "credentials and 2FA code.");
            }
        });
}