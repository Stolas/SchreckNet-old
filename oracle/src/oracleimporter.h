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
    CardInfoPtr addCard(QString name,
                        QString text,
                        bool isToken,
                        QVariantHash properties,
                        QList<CardRelation *> &relatedCards,
                        CardInfoPerSet setInfo);
signals:
    void setIndexChanged(int cardsImported, int setIndex, const QString &setName);
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
    void sortAndReduceColors(QString &colors);
};

