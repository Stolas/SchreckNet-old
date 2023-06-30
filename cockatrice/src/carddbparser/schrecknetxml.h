#ifndef SCHRECKNET_XML_H
#define SCHRECKNET_XML_H

#include "carddatabaseparser.h"

#include <QXmlStreamReader>

class SchrecknetParser : public ICardDatabaseParser
{
    Q_OBJECT
    Q_INTERFACES(ICardDatabaseParser)
public:
    SchrecknetParser() = default;
    ~SchrecknetParser() override = default;
    bool getCanParseFile(const QString &name, QIODevice &device) override;
    void parseFile(QIODevice &device) override;
    bool saveToFile(SetNameMap sets,
                    CardNameMap cards,
                    const QString &fileName,
                    const QString &sourceUrl = "unknown",
                    const QString &sourceVersion = "unknown") override;

private:
    void loadCardsFromXml(QXmlStreamReader &xml, bool isCrypt);
    void loadSetsFromXml(QXmlStreamReader &xml);
    void loadTokensFromXml(QXmlStreamReader &xml);
    QString getMainCardType(QString &type);
signals:
    void addCard(CardInfoPtr card) override;
    void addSet(CardSetPtr set) override;
};

#endif
