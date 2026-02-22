#include "zloadingwidget.h"

#include "qprocessindicator.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QStackedWidget>

ZLoadingWidget::ZLoadingWidget(bool start, QWidget *parent)
    : QStackedWidget{parent}, m_loadingIndicator(new QProcessIndicator())
{
    m_loadingIndicator->setType(QProcessIndicator::line_rotate);
    m_loadingIndicator->setFixedSize(64, 32);
    if (start) {
        m_loadingIndicator->start();
    }

    // Create a proper container widget for the loading indicator
    QWidget *loadingWidget = new QWidget(this);
    QHBoxLayout *loadingLayout = new QHBoxLayout(loadingWidget);
    loadingLayout->setSpacing(1);
    loadingLayout->addStretch();
    loadingLayout->addWidget(m_loadingIndicator);
    loadingLayout->addStretch();

    addWidget(loadingWidget); // Loading widget at index 0
}

void ZLoadingWidget::setupContentWidget(QWidget *contentWidget)
{
    m_contentWidget = contentWidget;
    addWidget(m_contentWidget);
}

void ZLoadingWidget::setupContentWidget(QLayout *contentLayout)
{
    m_contentWidget = new QWidget();
    m_contentWidget->setLayout(contentLayout);

    addWidget(m_contentWidget);
}

void ZLoadingWidget::setupErrorWidget(QWidget *errorWidget)
{
    m_errorWidget = errorWidget;
    addWidget(m_errorWidget);
}

void ZLoadingWidget::setupErrorWidget(QLayout *errorLayout)
{
    m_errorWidget = new QWidget();
    m_errorWidget->setLayout(errorLayout);

    addWidget(m_errorWidget);
}

void ZLoadingWidget::setupErrorWidget(const QString &errorMessage)
{
    m_errorWidget = new QWidget();
    QVBoxLayout *errorLayout = new QVBoxLayout(m_errorWidget);
    errorLayout->setAlignment(Qt::AlignCenter);

    QLabel *errorLabel = new QLabel(errorMessage);
    errorLabel->setAlignment(Qt::AlignCenter);
    errorLabel->setStyleSheet("QLabel { color: red; }");
    errorLayout->addWidget(errorLabel);

    addWidget(m_errorWidget);
}

void ZLoadingWidget::setupAditionalWidget(QWidget *customWidget)
{
    addWidget(customWidget);
}

void ZLoadingWidget::switchToWidget(QWidget *widget)
{
    int index = indexOf(widget);
    if (index != -1) {
        setCurrentIndex(index);
    }
}

void ZLoadingWidget::stop(bool showContent)
{
    if (m_loadingIndicator) {
        m_loadingIndicator->stop();
    }
    if (showContent) {
        // FIXME: dont use hardcoded index
        setCurrentIndex(1);
    }
}

void ZLoadingWidget::showError()
{
    m_loadingIndicator->stop();
    if (m_errorWidget) {
        setCurrentWidget(m_errorWidget);
    }
}

void ZLoadingWidget::showLoading()
{
    if (m_loadingIndicator) {
        m_loadingIndicator->start();
    }
    // FIXME: dont use hardcoded index
    setCurrentIndex(0);
}

ZLoadingWidget::~ZLoadingWidget()
{
    if (m_loadingIndicator) {
        m_loadingIndicator->stop();
        delete m_loadingIndicator;
        m_loadingIndicator = nullptr;
    }
}