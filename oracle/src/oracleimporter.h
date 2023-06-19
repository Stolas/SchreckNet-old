#pragma once

#include <QMap>
#include <QVariant>
#include <QJsonArray>
#include <carddatabase.h>
#include <utility>

class OracleImporter : public CardDatabase
{
    Q_OBJECT
private:
    const QStringList mainCardTypes = {"Retainer", "Imbued",     "Equipment",        "Reaction",        "Vampire",
                                       "Ally",     "Power",      "Political Action", "Action Modifier", "Action",
                                       "Event",    "Conviction", "Combat",           "Master"};
    QJsonArray allCards;
    QVariantMap setsMap;
    QString dataDir;

    QString getMainCardType(const QStringList &typeList);
    CardInfoPtr addCard(QString id, bool isCrypt, QString name, QString picture_url,
                        QString text, QVariantHash properties, bool isToken);
signals:
    void cardIndexChanged(int cardIndex, const QString &cardName);
    void dataReadProgress(int bytesRead, int totalBytes);

public:
    explicit OracleImporter(const QString &_dataDir, QObject *parent = nullptr);
    bool readJsonFromByteArray(const QByteArray &data);
    int startImport();
    bool saveToFile(const QString &fileName, const QString &sourceUrl, const QString &sourceVersion);
    const QString &getDataDir() const
    {
        return dataDir;
    }
    const QJsonArray getAllCards() const
    {
        return allCards;
    }
    void clear();

protected:
    inline QString getStringPropertyFromMap(const QVariantMap &card, const QString &propertyName);
    //void sortAndReduceColors(QString &colors);
};

