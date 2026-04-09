#include "zloadingwidget.h"

#include "qprocessindicator.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QStackedWidget>

ZLoadingWidget::ZLoadingWidget(bool retryEnabled, QWidget *parent)
    : QStackedWidget{parent}, m_retryEnabled(retryEnabled)
{
    QWidget *loadingWidget = new QWidget(this);

    m_loadingIndicator = new QProcessIndicator(loadingWidget);
    m_loadingIndicator->setType(QProcessIndicator::line_rotate);
    m_loadingIndicator->setFixedSize(64, 32);
    m_loadingIndicator->start();

    QHBoxLayout *loadingLayout = new QHBoxLayout(loadingWidget);
    loadingLayout->setSpacing(1);
    loadingLayout->addStretch();
    loadingLayout->addWidget(m_loadingIndicator);
    loadingLayout->addStretch();

    m_errorWidget = new ZLoadingErrorWidget(m_retryEnabled, this);
    connect(static_cast<ZLoadingErrorWidget *>(m_errorWidget),
            &ZLoadingErrorWidget::retryClicked, this,
            [this]() { emit retryClicked(); });

    addWidget(loadingWidget);
    addWidget(m_errorWidget);
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
    if (m_errorWidget) {
        m_errorWidget->deleteLater();
    }
    m_errorWidget = errorWidget;
    addWidget(m_errorWidget);
}

void ZLoadingWidget::setupErrorWidget(QLayout *errorLayout)
{
    if (m_errorWidget) {
        m_errorWidget->deleteLater();
    }
    m_errorWidget = new QWidget();
    m_errorWidget->setLayout(errorLayout);

    addWidget(m_errorWidget);
}

int ZLoadingWidget::setupAditionalWidget(QWidget *customWidget)
{
    return addWidget(customWidget);
}

void ZLoadingWidget::switchToWidget(QWidget *widget)
{
    int index = indexOf(widget);
    if (index != -1) {
        if (m_loadingIndicator) {
            m_loadingIndicator->stop();
        }
        setCurrentIndex(index);
    }
}

void ZLoadingWidget::stop(bool showContent)
{
    if (m_loadingIndicator) {
        m_loadingIndicator->stop();
    }
    if (showContent && m_contentWidget) {
        switchToWidget(m_contentWidget);
    }
}

void ZLoadingWidget::showError()
{
    m_loadingIndicator->stop();
    if (m_errorWidget) {
        setCurrentWidget(m_errorWidget);
    }
}

void ZLoadingWidget::showError(const QString &errorMessage)
{
    m_loadingIndicator->stop();
    if (m_errorWidget) {
        // FIXME: can be handled better
        // maybe subclass ZLoadingWidget for custom error widget?
        if (auto errorWidget =
                qobject_cast<ZLoadingErrorWidget *>(m_errorWidget)) {
            errorWidget->setText(errorMessage);
        }
        setCurrentWidget(m_errorWidget);
    }
}

void ZLoadingWidget::showLoading()
{
    if (m_loadingIndicator) {
        m_loadingIndicator->start();
    }
    setCurrentWidget(m_loadingIndicator->parentWidget());
}

ZLoadingWidget::~ZLoadingWidget()
{
    if (m_loadingIndicator) {
        m_loadingIndicator->stop();
    }
}