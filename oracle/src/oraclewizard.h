#ifndef ORACLEWIZARD_H
#define ORACLEWIZARD_H

#include <QFuture>
#include <QFutureWatcher>
#include <QWizard>
#include <utility>

class QCheckBox;
class QGroupBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QRadioButton;
class QProgressBar;
class QNetworkAccessManager;
class QTextEdit;
class QVBoxLayout;
class OracleImporter;
class QSettings;

#include "pagetemplates.h"

class OracleWizard : public QWizard
{
    Q_OBJECT
public:
    explicit OracleWizard(QWidget *parent = nullptr);
    void accept() override;
    void enableButtons();
    void disableButtons();
    void retranslateUi();
    void setCardSourceUrl(const QString &sourceUrl)
    {
        cardSourceUrl = sourceUrl;
    }
    void setCardSourceVersion(const QString &sourceVersion)
    {
        cardSourceVersion = sourceVersion;
    }
    const QString &getCardSourceUrl() const
    {
        return cardSourceUrl;
    }
    const QString &getCardSourceVersion() const
    {
        return cardSourceVersion;
    }

public:
    OracleImporter *importer;
    QSettings *settings;
    QNetworkAccessManager *nam;

private slots:
    void updateLanguage();

private:
    QByteArray tokensData;
    QString cardSourceUrl;
    QString cardSourceVersion;

protected:
    void changeEvent(QEvent *event) override;
};

class IntroPage : public OracleWizardPage
{
    Q_OBJECT
public:
    explicit IntroPage(QWidget *parent = nullptr);
    void retranslateUi() override;

private:
    QStringList findQmFiles();
    QString languageName(const QString &lang);

private:
    QLabel *label, *languageLabel, *versionLabel;
    QComboBox *languageBox;

private slots:
    void languageBoxChanged(int index);
};

class OutroPage : public OracleWizardPage
{
    Q_OBJECT
public:
    explicit OutroPage(QWidget * = nullptr)
    {
    }
    void retranslateUi() override;
};

class LoadCardsPage : public OracleWizardPage
{
    Q_OBJECT
public:
    explicit LoadCardsPage(QWidget *parent = nullptr);
    void retranslateUi() override;

protected:
    void initializePage() override;
    bool validatePage() override;
    void readJsonFromByteArray(QByteArray data);
    void downloadSetsFile(const QUrl &url);

private:
    QRadioButton *urlRadioButton;
    QRadioButton *fileRadioButton;
    QLineEdit *urlLineEdit;
    QLineEdit *fileLineEdit;
    QPushButton *urlButton;
    QPushButton *fileButton;
    QLabel *progressLabel;
    QProgressBar *progressBar;

    QFutureWatcher<bool> watcher;
    QFuture<bool> future;

private slots:
    void actLoadSetsFile();
    void actRestoreDefaultUrl();
    void actDownloadProgressSetsFile(qint64 received, qint64 total);
    void actDownloadFinishedSetsFile();
    void importFinished();
};

class SaveCardsPage : public OracleWizardPage
{
    Q_OBJECT
public:
    explicit SaveCardsPage(QWidget *parent = nullptr);
    void retranslateUi() override;

private:
    QTextEdit *messageLog;
    QCheckBox *defaultPathCheckBox;
    QLabel *pathLabel;
    QLabel *saveLabel;

protected:
    void initializePage() override;
    void cleanupPage() override;
    bool validatePage() override;

private slots:
    void updateTotalProgress(int cardIndex, const QString &cardName);
};

#endif
