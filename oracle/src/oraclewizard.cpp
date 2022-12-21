#include "oraclewizard.h"

#include "main.h"
#include "oracleimporter.h"
#include "settingscache.h"
#include "version_string.h"

#include <QAbstractButton>
#include <QBuffer>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QProgressBar>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollBar>
#include <QStandardPaths>
#include <QTextEdit>
#include <QtConcurrent>
#include <QtGui>

#define MTGJSON_VERSION_URL "https://www.mtgjson.com/api/v5/Meta.json"

#define ALLSETS_URL "https://static.krcg.org/data/vtes.json"

OracleWizard::OracleWizard(QWidget *parent) : QWizard(parent)
{
    // define a dummy context that will be used where needed
    QString dummy = QT_TRANSLATE_NOOP("i18n", "English");

    settings = new QSettings(SettingsCache::instance().getSettingsPath() + "global.ini", QSettings::IniFormat, this);
    connect(&SettingsCache::instance(), SIGNAL(langChanged()), this, SLOT(updateLanguage()));

    importer = new OracleImporter(SettingsCache::instance().getDataPath(), this);

    nam = new QNetworkAccessManager(this);

    if (!isSpoilersOnly) {
        addPage(new IntroPage);
        addPage(new LoadSetsPage);
        addPage(new SaveSetsPage);
        addPage(new OutroPage);
    } else {
        addPage(new OutroPage);
    }

    retranslateUi();
}

void OracleWizard::updateLanguage()
{
    qApp->removeTranslator(translator);
    installNewTranslator();
}

void OracleWizard::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }

    QDialog::changeEvent(event);
}

void OracleWizard::retranslateUi()
{
    setWindowTitle(tr("Oracle Importer"));
    for (int i = 0; i < pageIds().count(); i++) {
        dynamic_cast<OracleWizardPage *>(page(i))->retranslateUi();
    }
}

void OracleWizard::accept()
{
    QDialog::accept();
}

void OracleWizard::enableButtons()
{
    button(QWizard::NextButton)->setDisabled(false);
    button(QWizard::BackButton)->setDisabled(false);
}

void OracleWizard::disableButtons()
{
    button(QWizard::NextButton)->setDisabled(true);
    button(QWizard::BackButton)->setDisabled(true);
}

IntroPage::IntroPage(QWidget *parent) : OracleWizardPage(parent)
{
    label = new QLabel(this);
    label->setWordWrap(true);

    languageLabel = new QLabel(this);
    versionLabel = new QLabel(this);
    languageBox = new QComboBox(this);

    QStringList languageCodes = findQmFiles();
    for (const QString &code : languageCodes) {
        QString langName = languageName(code);
        languageBox->addItem(langName, code);
    }

    QString setLanguage = QCoreApplication::translate("i18n", DEFAULT_LANG_NAME);
    int index = languageBox->findText(setLanguage, Qt::MatchExactly);
    if (index == -1) {
        qWarning() << "could not find language" << setLanguage;
    } else {
        languageBox->setCurrentIndex(index);
    }

    connect(languageBox, SIGNAL(currentIndexChanged(int)), this, SLOT(languageBoxChanged(int)));

    auto *layout = new QGridLayout(this);
    layout->addWidget(label, 0, 0, 1, 2);
    layout->addWidget(languageLabel, 1, 0);
    layout->addWidget(languageBox, 1, 1);
    layout->addWidget(versionLabel, 2, 0, 1, 2);

    setLayout(layout);
}

QStringList IntroPage::findQmFiles()
{
    QDir dir(translationPath);
    QStringList fileNames = dir.entryList(QStringList(translationPrefix + "_*.qm"), QDir::Files, QDir::Name);
    fileNames.replaceInStrings(QRegularExpression(translationPrefix + "_(.*)\\.qm"), "\\1");
    return fileNames;
}

QString IntroPage::languageName(const QString &lang)
{
    QTranslator qTranslator;

    QString appNameHint = translationPrefix + "_" + lang;
    bool appTranslationLoaded = qTranslator.load(appNameHint, translationPath);
    if (!appTranslationLoaded) {
        qDebug() << "Unable to load" << translationPrefix << "translation" << appNameHint << "at" << translationPath;
    }

    return qTranslator.translate("i18n", DEFAULT_LANG_NAME);
}

void IntroPage::languageBoxChanged(int index)
{
    SettingsCache::instance().setLang(languageBox->itemData(index).toString());
}

void IntroPage::retranslateUi()
{
    setTitle(tr("Introduction"));
    label->setText(tr("This wizard will import the list of sets, cards, and tokens "
                      "that will be used by SchreckNet."));
    languageLabel->setText(tr("Interface language:"));
    versionLabel->setText(tr("Version:") + QString(" %1").arg(VERSION_STRING));
}

void OutroPage::retranslateUi()
{
    setTitle(tr("Finished"));
    setSubTitle(tr("The wizard has finished.") + "<br>" +
                tr("You can now start using SchreckNet with the newly updated cards.") + "<br><br>" +
                tr("If the card databases don't reload automatically, restart the SchreckNet client."));
}

LoadSetsPage::LoadSetsPage(QWidget *parent) : OracleWizardPage(parent)
{
    urlRadioButton = new QRadioButton(this);
    fileRadioButton = new QRadioButton(this);

    urlLineEdit = new QLineEdit(this);
    fileLineEdit = new QLineEdit(this);

    progressLabel = new QLabel(this);
    progressBar = new QProgressBar(this);

    urlRadioButton->setChecked(true);

    urlButton = new QPushButton(this);
    connect(urlButton, SIGNAL(clicked()), this, SLOT(actRestoreDefaultUrl()));

    fileButton = new QPushButton(this);
    connect(fileButton, SIGNAL(clicked()), this, SLOT(actLoadSetsFile()));

    auto *layout = new QGridLayout(this);
    layout->addWidget(urlRadioButton, 0, 0);
    layout->addWidget(urlLineEdit, 0, 1);
    layout->addWidget(urlButton, 1, 1, Qt::AlignRight);
    layout->addWidget(fileRadioButton, 2, 0);
    layout->addWidget(fileLineEdit, 2, 1);
    layout->addWidget(fileButton, 3, 1, Qt::AlignRight);
    layout->addWidget(progressLabel, 4, 0);
    layout->addWidget(progressBar, 4, 1);

    connect(&watcher, SIGNAL(finished()), this, SLOT(importFinished()));

    setLayout(layout);
}

void LoadSetsPage::initializePage()
{
    urlLineEdit->setText(wizard()->settings->value("allsetsurl", ALLSETS_URL).toString());

    progressLabel->hide();
    progressBar->hide();
}

void LoadSetsPage::retranslateUi()
{
    setTitle(tr("Source selection"));
    setSubTitle(tr("Please specify a compatible source for the list of sets and cards. "
                   "You can specify a URL address that will be downloaded or "
                   "use an existing file from your computer."));

    urlRadioButton->setText(tr("Download URL:"));
    fileRadioButton->setText(tr("Local file:"));
    urlButton->setText(tr("Restore default URL"));
    fileButton->setText(tr("Choose file..."));
}

void LoadSetsPage::actRestoreDefaultUrl()
{
    urlLineEdit->setText(ALLSETS_URL);
}

void LoadSetsPage::actLoadSetsFile()
{
    QFileDialog dialog(this, tr("Load sets file"));
    dialog.setFileMode(QFileDialog::ExistingFile);

    QString extensions = "*.json";
    dialog.setNameFilter(tr("Sets JSON file (%1)").arg(extensions));

    if (!fileLineEdit->text().isEmpty() && QFile::exists(fileLineEdit->text())) {
        dialog.selectFile(fileLineEdit->text());
    }

    if (!dialog.exec()) {
        return;
    }

    fileLineEdit->setText(dialog.selectedFiles().at(0));
}

bool LoadSetsPage::validatePage()
{
    // once the import is finished, we call next(); skip validation
    if (wizard()->importer->getAllCards().size() > 0) {
        return true;
    }

    // else, try to import sets
    if (urlRadioButton->isChecked()) {
        const auto url = QUrl::fromUserInput(urlLineEdit->text());

        if (!url.isValid()) {
            QMessageBox::critical(this, tr("Error"), tr("The provided URL is not valid."));
            return false;
        }

        progressLabel->setText(tr("Downloading (0MiB)"));
        // show an infinite progressbar
        progressBar->setMaximum(0);
        progressBar->setMinimum(0);
        progressBar->setValue(0);
        progressLabel->show();
        progressBar->show();

        wizard()->disableButtons();
        setEnabled(false);

        downloadSetsFile(url);
    } else if (fileRadioButton->isChecked()) {
        QFile setsFile(fileLineEdit->text());
        if (!setsFile.exists()) {
            QMessageBox::critical(this, tr("Error"), tr("Please choose a file."));
            return false;
        }

        if (!setsFile.open(QIODevice::ReadOnly)) {
            QMessageBox::critical(nullptr, tr("Error"), tr("Cannot open file '%1'.").arg(fileLineEdit->text()));
            return false;
        }

        wizard()->disableButtons();
        setEnabled(false);

        wizard()->setCardSourceUrl(setsFile.fileName());
        wizard()->setCardSourceVersion("unknown");

        qDebug() << "3";
        readJsonFromByteArray(setsFile.readAll());
    }

    return false;
}

#include <iostream>
void LoadSetsPage::downloadSetsFile(const QUrl &url)
{
    wizard()->setCardSourceVersion("unknown");

    const auto urlString = url.toString();
    if (urlString == ALLSETS_URL) {
        const auto versionUrl = QUrl::fromUserInput(MTGJSON_VERSION_URL);
        auto *versionReply = wizard()->nam->get(QNetworkRequest(versionUrl));
        connect(versionReply, &QNetworkReply::finished, [this, versionReply]() {
            if (versionReply->error() == QNetworkReply::NoError) {
                auto jsonData = versionReply->readAll();
                QJsonParseError jsonError{};
                auto jsonResponse = QJsonDocument::fromJson(jsonData, &jsonError);

                if (jsonError.error == QJsonParseError::NoError) {
                    const auto jsonMap = jsonResponse.toVariant().toMap();

                    auto versionString = jsonMap.value("meta").toMap().value("version").toString();
                    if (versionString.isEmpty()) {
                        versionString = "unknown";
                    }
                    wizard()->setCardSourceVersion(versionString);
                }
            }

            versionReply->deleteLater();
        });
    }

    wizard()->setCardSourceUrl(url.toString());

    auto *reply = wizard()->nam->get(QNetworkRequest(url));

    connect(reply, SIGNAL(finished()), this, SLOT(actDownloadFinishedSetsFile()));
    connect(reply, SIGNAL(downloadProgress(qint64, qint64)), this, SLOT(actDownloadProgressSetsFile(qint64, qint64)));
}

void LoadSetsPage::actDownloadProgressSetsFile(qint64 received, qint64 total)
{
    if (total > 0) {
        progressBar->setMaximum(static_cast<int>(total));
        progressBar->setValue(static_cast<int>(received));
    }
    progressLabel->setText(tr("Downloading (%1MiB)").arg((int)received / (1024 * 1024)));
}

void LoadSetsPage::actDownloadFinishedSetsFile()
{
    // check for a reply
    auto *reply = dynamic_cast<QNetworkReply *>(sender());
    auto errorCode = reply->error();
    if (errorCode != QNetworkReply::NoError) {
        QMessageBox::critical(this, tr("Error"), tr("Network error: %1.").arg(reply->errorString()));

        wizard()->enableButtons();
        setEnabled(true);

        reply->deleteLater();
        return;
    }

    auto statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (statusCode == 301 || statusCode == 302) {
        const auto redirectUrl = reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
        qDebug() << "following redirect url:" << redirectUrl.toString();
        downloadSetsFile(redirectUrl);
        reply->deleteLater();
        return;
    }

    progressLabel->hide();
    progressBar->hide();

    // save AllPrintings.json url, but only if the user customized it and download was successful
    if (urlLineEdit->text() != QString(ALLSETS_URL)) {
        wizard()->settings->setValue("allsetsurl", urlLineEdit->text());
    } else {
        wizard()->settings->remove("allsetsurl");
    }

    readJsonFromByteArray(reply->readAll());
    reply->deleteLater();
}

void LoadSetsPage::readJsonFromByteArray(QByteArray data)
{
    // show an infinite progressbar
    progressBar->setMaximum(0);
    progressBar->setMinimum(0);
    progressBar->setValue(0);
    progressLabel->setText(tr("Parsing file"));
    progressLabel->show();
    progressBar->show();

    // Start the computation.
    future = QtConcurrent::run([this, &data] {
        qDebug() << QString(data);
        return wizard()->importer->readJsonFromByteArray(data);
    });
    watcher.setFuture(future);
}

void LoadSetsPage::importFinished()
{
    wizard()->enableButtons();
    setEnabled(true);
    progressLabel->hide();
    progressBar->hide();

    if (watcher.future().result()) {
        wizard()->next();
    } else {
        QMessageBox::critical(this, tr("Error"),
                              tr("The file was retrieved successfully, but it does not contain any card data."));
    }
}

SaveSetsPage::SaveSetsPage(QWidget *parent) : OracleWizardPage(parent)
{
    pathLabel = new QLabel(this);
    saveLabel = new QLabel(this);

    defaultPathCheckBox = new QCheckBox(this);

    messageLog = new QTextEdit(this);
    messageLog->setReadOnly(true);

    auto *layout = new QGridLayout(this);
    layout->addWidget(messageLog, 0, 0);
    layout->addWidget(saveLabel, 1, 0);
    layout->addWidget(pathLabel, 2, 0);
    layout->addWidget(defaultPathCheckBox, 3, 0);

    setLayout(layout);
}

void SaveSetsPage::cleanupPage()
{
    wizard()->importer->clear();
    disconnect(wizard()->importer, SIGNAL(setIndexChanged(int, int, const QString &)), nullptr, nullptr);
}

void SaveSetsPage::initializePage()
{
    messageLog->clear();

    connect(wizard()->importer, SIGNAL(setIndexChanged(int, int, const QString &)), this,
            SLOT(updateTotalProgress(int, int, const QString &)));

    if (!wizard()->importer->startImport()) {
        QMessageBox::critical(this, tr("Error"), tr("No set has been imported."));
    }
}

void SaveSetsPage::retranslateUi()
{
    setTitle(tr("Sets imported"));
    setSubTitle(tr("The following sets have been found:"));

    saveLabel->setText(tr("Press \"Save\" to store the imported cards in the SchreckNet database."));
    pathLabel->setText(tr("The card database will be saved at the following location:") + "<br>" +
                       SettingsCache::instance().getCardDatabasePath());
    defaultPathCheckBox->setText(tr("Save to a custom path (not recommended)"));

    setButtonText(QWizard::NextButton, tr("&Save"));
}

void SaveSetsPage::updateTotalProgress(int cardsImported, int /* setIndex */, const QString &setName)
{
    if (setName.isEmpty()) {
        messageLog->append("<b>" + tr("Import finished: %1 cards.").arg(wizard()->importer->getCardList().size()) +
                           "</b>");
    } else {
        messageLog->append(tr("%1: %2 cards imported").arg(setName).arg(cardsImported));
    }

    messageLog->verticalScrollBar()->setValue(messageLog->verticalScrollBar()->maximum());
}

bool SaveSetsPage::validatePage()
{
    QString defaultPath = SettingsCache::instance().getCardDatabasePath();
    QString windowName = tr("Save card database");
    QString fileType = tr("XML; card database (*.xml)");

    QString fileName;
    if (defaultPathCheckBox->isChecked()) {
        fileName = QFileDialog::getSaveFileName(this, windowName, defaultPath, fileType);
    } else {
        fileName = defaultPath;
    }

    if (fileName.isEmpty()) {
        return false;
    }

    QFileInfo fi(fileName);
    QDir fileDir(fi.path());
    if (!fileDir.exists() && !fileDir.mkpath(fileDir.absolutePath())) {
        return false;
    }

    if (!wizard()->importer->saveToFile(fileName, wizard()->getCardSourceUrl(), wizard()->getCardSourceVersion())) {
        QMessageBox::critical(this, tr("Error"), tr("The file could not be saved to %1").arg(fileName));
        return false;
    }

    return true;
}
