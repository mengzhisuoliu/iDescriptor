#ifndef RELEASECHANGELOG_H
#define RELEASECHANGELOG_H

#include <QDialog>
#include <QJsonArray>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

class ReleaseChangelogDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ReleaseChangelogDialog(QJsonDocument data,
                                    QWidget *parent = nullptr);

    ~ReleaseChangelogDialog();
signals:

private:
    void setupUI(const QJsonDocument &data);

    QVBoxLayout *m_mainLayout = nullptr;
    QPushButton *m_skipButton = nullptr;
    QPushButton *m_donateButton = nullptr;
    QLabel *m_titleLabel = nullptr;
    QLabel *m_descriptionLabel = nullptr;

    void onDonateClicked();
    void onSkipButtonClicked();
};

#endif // RELEASECHANGELOG_H
