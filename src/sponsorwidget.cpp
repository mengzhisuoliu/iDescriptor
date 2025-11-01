#include "sponsorwidget.h"
#include "sponsorappcard.h"
#include <QLabel>
#include <QVBoxLayout>

SponsorWidget::SponsorWidget(QWidget *parent) : QWidget(parent)
{
    setLayout(new QVBoxLayout(this));
    QLabel *sponsorTitle = new QLabel("Would you like to sponsor us?");
    sponsorTitle->setAlignment(Qt::AlignCenter);

    QLabel *sponsorDesc =
        new QLabel("This app is open-source and free to use. "
                   "And in order to keep it that way, we rely on donations. "
                   "Consider becoming a sponsor to support "
                   "and promote your app/brand here");
    sponsorDesc->setWordWrap(true);
    layout()->addWidget(sponsorTitle);
    layout()->addWidget(sponsorDesc);
    QLabel *sponsorIconLabel = new QLabel("Example:");
    layout()->addWidget(sponsorIconLabel);
    SponsorAppCard *card = new SponsorAppCard(this);
    layout()->addWidget(card);
    layout()->setAlignment(card, Qt::AlignCenter);
}
