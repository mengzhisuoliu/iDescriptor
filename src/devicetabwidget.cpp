#include "devicetabwidget.h"
#include <QCheckBox>
#include <QComboBox>
#include <QFrame>
#include <QParallelAnimationGroup>
#include <QPropertyAnimation>
#include <QScrollArea>
#include <QSpinBox>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>
#include <QWheelEvent>
DeviceTabWidget::DeviceTabWidget(QWidget *parent) : QTabWidget(parent)
{
    setTabsClosable(false);
    setTabPosition(QTabWidget::West); // Set tabs to appear on the left side
    connect(this, &QTabWidget::tabCloseRequested, this,
            &DeviceTabWidget::onCloseTab);
}
int DeviceTabWidget::addTabCustom(QWidget *widget, const QString &text)
{
    int index = addTab(widget, text);
    QWidget *tabWidget = createTabWidget(text, index);
    // tabWidget->setMinimumHeight(220); // Set a minimum height for the tab
    // widget tabWidget->setSizePolicy(QSizePolicy::Expanding,
    // QSizePolicy::Expanding);
    // tabWidget->setSizePolicy(QSizePolicy::Expanding,
    // QSizePolicy::Preferred);
    tabBar()->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    tabBar()->setTabButton(index, QTabBar::LeftSide, tabWidget);
    tabBar()->setTabText(index, ""); // Clear the default text
    return index;
}
void DeviceTabWidget::wheelEvent(QWheelEvent *event)
{
    // Ignore wheel events to prevent tab switching with scroll wheel
    event->ignore();
}
void DeviceTabWidget::setTabIcon(int index, const QPixmap &icon)
{
    if (index >= 0 && index < count()) {
        QString text = tabBar()->tabText(index);
        if (text.isEmpty()) {
            // Get text from the custom widget if it exists
            QWidget *tabWidget = tabBar()->tabButton(index, QTabBar::LeftSide);
            if (tabWidget) {
                QLabel *textLabel = tabWidget->findChild<QLabel *>("textLabel");
                if (textLabel) {
                    text = textLabel->text();
                }
            }
        }
        QWidget *newTabWidget = createTabWidget(text, index);
        tabBar()->setTabButton(index, QTabBar::LeftSide, newTabWidget);
    }
}
QWidget *DeviceTabWidget::createTabWidget(const QString &text, int index)
{
    QWidget *tabWidget = new QWidget();
    // tabWidget->setMinimumHeight(220); // Set a minimum height for the tab
    // widget
    tabWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    tabWidget->setStyleSheet("QWidget { "
                             "}");
    QVBoxLayout *mainLayout = new QVBoxLayout(tabWidget);
    mainLayout->setContentsMargins(5, 2, 5, 2);
    mainLayout->setSpacing(2);
    mainLayout->setSizeConstraint(QLayout::SetMinimumSize);
    // Top section with icon and text
    QWidget *topSection = new QWidget();
    QHBoxLayout *topLayout = new QHBoxLayout(topSection);
    topLayout->setContentsMargins(0, 0, 0, 0);
    topLayout->setSpacing(5);
    // Add text
    QLabel *textLabel = new QLabel(text);
    textLabel->setObjectName("textLabel");
    topLayout->addWidget(textLabel);
    mainLayout->addWidget(topSection);
    // Create collapsible options section
    QPushButton *toggleButton = new QPushButton("▶ Options");
    toggleButton->setFlat(true);
    toggleButton->setStyleSheet(
        "QPushButton { text-align: left; padding: 2px; }");
    toggleButton->setMinimumHeight(20);
    toggleButton->setMaximumHeight(25);
    toggleButton->setCheckable(true);
    toggleButton->setChecked(false);
    QWidget *contentWidget = new QWidget();
    contentWidget->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
    QVBoxLayout *optionsLayout = new QVBoxLayout(contentWidget);
    optionsLayout->setContentsMargins(5, 5, 5, 5);
    optionsLayout->setSpacing(3);
    // Create navigation buttons
    QPushButton *infoBtn = new QPushButton("Info");
    QPushButton *appsBtn = new QPushButton("Apps");
    QPushButton *galleryBtn = new QPushButton("Gallery");
    QPushButton *filesBtn = new QPushButton("Files");
    // Set button properties
    QList<QPushButton *> buttons = {infoBtn, appsBtn, galleryBtn, filesBtn};
    for (QPushButton *btn : buttons) {
        btn->setMaximumHeight(25);
        btn->setCheckable(true);
        btn->setStyleSheet("QPushButton { "
                           "  background-color: #f0f0f0; "
                           "  border: 1px solid #ccc; "
                           "  padding: 4px 8px; "
                           "  text-align: center; "
                           "} "
                           "QPushButton:checked { "
                           "  background-color: #0078d4; "
                           "  color: white; "
                           "  border: 1px solid #005a9e; "
                           "} "
                           "QPushButton:hover { "
                           "  background-color: #e5e5e5; "
                           "} "
                           "QPushButton:checked:hover { "
                           "  background-color: #106ebe; "
                           "}");
    }
    // Set info as default active
    infoBtn->setChecked(true);
    // Connect button group behavior and emit signals
    for (QPushButton *btn : buttons) {
        connect(btn, &QPushButton::clicked, [this, buttons, btn, index]() {
            for (QPushButton *otherBtn : buttons) {
                if (otherBtn != btn) {
                    otherBtn->setChecked(false);
                }
            }
            btn->setChecked(true);
            emit navigationButtonClicked(index, btn->text());
        });
    }
    // Add buttons to layout
    optionsLayout->addWidget(infoBtn);
    optionsLayout->addWidget(appsBtn);
    optionsLayout->addWidget(galleryBtn);
    optionsLayout->addWidget(filesBtn);
    // Set the content widget in the scroll area
    // Add widgets to main layout
    mainLayout->addWidget(toggleButton);
    mainLayout->addWidget(contentWidget);
    contentWidget->setVisible(false); // Initially hidden
    // Connect toggle button to expand/collapse
    int prevHeight = tabBar()->sizeHint().height();
    connect(toggleButton, &QPushButton::clicked,
            [this, toggleButton, contentWidget, prevHeight]() {
                // Toggle content visibility
                bool isExpanded = toggleButton->isChecked();
                contentWidget->setVisible(isExpanded);
                if (isExpanded) {
                    // Expanding
                    toggleButton->setText("▼ Options");
                    tabBar()->resize(tabBar()->sizeHint().width(),
                                     contentWidget->sizeHint().height() +
                                         tabBar()->sizeHint().height());
                    tabBar()->adjustSize();
                } else {
                    // Collapsing
                    toggleButton->setText("▶ Options");
                    tabBar()->setFixedHeight(prevHeight);
                }
                // QTimer::singleShot(0, tabBar(), &QWidget::adjustSize);
            });
    return tabWidget;
}
void DeviceTabWidget::onCloseTab(int index) { removeTab(index); }