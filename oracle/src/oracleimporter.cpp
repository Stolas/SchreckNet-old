#include "oracleimporter.h"

#include "carddbparser/schrecknetxml.h"

#include <QtWidgets>
#include <algorithm>
#include <climits>

OracleImporter::OracleImporter(const QString &_dataDir, QObject *parent) : CardDatabase(parent), dataDir(_dataDir)
{
}

bool OracleImporter::readJsonFromByteArray(const QByteArray &data)
{
    auto jsonDoc = QJsonDocument::fromJson(data);
    if (jsonDoc.isNull()) {
        qDebug() << "error: failed to parse JSON";
        return false;
    }

    allCards = jsonDoc.array();
    qDebug() << "ok: created allCard list";
    return true;
}

QString OracleImporter::getMainCardType(const QStringList &typeList)
{
    if (typeList.isEmpty()) {
        return {};
    }

    for (const auto &type : mainCardTypes) {
        if (typeList.contains(type)) {
            return type;
        }
    }

    return typeList.first();
}

CardInfoPtr OracleImporter::addCard(QString name,
                                    QString text,
                                    bool isToken,
                                    QVariantHash properties,
                                    QList<CardRelation *> &relatedCards,
                                    CardInfoPerSet setInfo)
{
    // Workaround for card name weirdness
    qDebug() << "Importing card: " << name;
    name = name.replace("Æ", "AE");
    name = name.replace("’", "'");
    if (cards.contains(name)) {
        CardInfoPtr card = cards.value(name);
        card->addToSet(setInfo.getPtr(), setInfo);
        return card;
    }

    // Remove {} around mana costs, except if it's split cost
    QString manacost = properties.value("manacost").toString();
    if (!manacost.isEmpty()) {
        QStringList symbols = manacost.split("}");
        QString formattedCardCost;
        for (QString symbol : symbols) {
            if (symbol.contains(QRegularExpression("[0-9WUBGRP]/[0-9WUBGRP]"))) {
                symbol.append("}");
            } else {
                symbol.remove(QChar('{'));
            }
            formattedCardCost.append(symbol);
        }
        properties.insert("manacost", formattedCardCost);
    }

    // fix colors
    QString allColors = properties.value("colors").toString();
    if (allColors.size() > 1) {
        sortAndReduceColors(allColors);
        properties.insert("colors", allColors);
    }
    QString allColorIdent = properties.value("coloridentity").toString();
    if (allColorIdent.size() > 1) {
        sortAndReduceColors(allColorIdent);
        properties.insert("coloridentity", allColorIdent);
    }

    // Todo; DETECT CARD POSITIONING INFO

    // cards that enter the field tapped
    bool cipt = text.contains(" it enters the battlefield tapped") ||
                (text.contains(name + " enters the battlefield tapped") &&
                 !text.contains(name + " enters the battlefield tapped unless"));

    // table row
    int tableRow = 1;
    QString mainCardType = properties.value("maintype").toString();
    if ((mainCardType == "Land"))
        tableRow = 0;
    else if ((mainCardType == "Sorcery") || (mainCardType == "Instant"))
        tableRow = 3;
    else if (mainCardType == "Creature")
        tableRow = 2;

    // card side
    QString side = properties.value("side").toString() == "b" ? "back" : "front";
    properties.insert("side", side);

    // upsideDown (flip cards)
    QString layout = properties.value("layout").toString();
    bool upsideDown = layout == "flip" && side == "back";

    // insert the card and its properties
    QList<CardRelation *> reverseRelatedCards;
    CardInfoPerSetMap setsInfo;
    setsInfo.insert(setInfo.getPtr()->getShortName(), setInfo);
    CardInfoPtr newCard = CardInfo::newInstance(name, text, isToken, properties, relatedCards, reverseRelatedCards,
                                                setsInfo, cipt, tableRow, upsideDown);

    if (name.isEmpty()) {
        qDebug() << "warning: an empty card was added to set" << setInfo.getPtr()->getShortName();
    }
    cards.insert(name, newCard);

    return newCard;
}

QString OracleImporter::getStringPropertyFromMap(const QVariantMap &card, const QString &propertyName)
{
    return card.contains(propertyName) ? card.value(propertyName).toString() : QString("");
}

void OracleImporter::sortAndReduceColors(QString &colors)
{
    // sort
    const QHash<QChar, unsigned int> colorOrder{{'W', 0}, {'U', 1}, {'B', 2}, {'R', 3}, {'G', 4}};
    std::sort(colors.begin(), colors.end(), [&colorOrder](const QChar a, const QChar b) {
        return colorOrder.value(a, INT_MAX) < colorOrder.value(b, INT_MAX);
    });
    // reduce
    QChar lastChar = '\0';
    for (int i = 0; i < colors.size(); ++i) {
        if (colors.at(i) == lastChar)
            colors.remove(i, 1);
        else
            lastChar = colors.at(i);
    }
}

int OracleImporter::startImport()
{
    int numCards = 0;// setCards = 0, setIndex = 0;

    QVariantHash properties;
    QString id;
    QString name;
    QList<QVariant> sets;
    QString text;

    for (auto it : allCards) {
        //qDebug() << "Card " << it;
        auto obj = it.toObject();
        qDebug() << "Object " << obj;
        auto types = obj["types"].toArray();

        /* Uniform elements */
        qDebug() << "Types " << types;
        if (types.contains("vampire")) {
            /* Parse as Crypt Card */
            properties["clan"] = obj["clans"].toString();
            properties["capacity"] = obj["capacity"].toString();
            properties["disciplines"] = obj["disciplines"].toString();
            properties["group"] = obj["group"].toString();
        } else {
            /* Parse as Library Card */
            properties["pool_cost"] = obj["pool_cost"].toString();
        }
        id = obj["id"].toString();
        name = obj["_name"].toString();
        // name = obj["name"].toString();
        properties["picture_url"] = obj["url"].toString();
        // set = obj["_set"].toString();
        // sets = obj["sets"].toArray();
        qDebug() << "Todo; sets";
        // scans = obj["scans"].toString();
        text = obj["card_text"].toString();
        // printed_name = obj["printed_name"].toString();

        // Add to list.
        QList<CardRelation *> relatedCards;
        CardInfoPerSet setInfo;

        CardInfoPtr newCard = addCard(name, text, false, properties, relatedCards, setInfo);
        numCards++;
    }

    // Todo; Add 'the edge' token.
    // add an empty set for tokens
    // CardSetPtr tokenSet = CardSet::newInstance(TOKENS_SETNAME, tr("Dummy set containing tokens"), "Tokens");
    // sets.insert(TOKENS_SETNAME, tokenSet);
    qDebug() << "Added all cards, now add 'the Edge'.";


    return numCards;
}

bool OracleImporter::saveToFile(const QString &fileName, const QString &sourceUrl, const QString &sourceVersion)
{
    SchreckNetXmlParser parser;
    return parser.saveToFile(sets, cards, fileName, sourceUrl, sourceVersion);
}

void OracleImporter::clear()
{
    CardDatabase::clear();
}
