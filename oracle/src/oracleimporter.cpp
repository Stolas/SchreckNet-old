#include "oracleimporter.h"
#include "game_specific_terms.h"

#include "carddbparser/schrecknetxml.h"

#include <QtWidgets>
#include <algorithm>
#include <climits>

OracleImporter::OracleImporter(const QString &_dataDir, QObject *parent) : CardDatabase(parent), dataDir(_dataDir)
{
}

bool OracleImporter::readJsonFromByteArray(const QByteArray &data)
{
    qDebug() << QString(*data);
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

CardInfoPtr OracleImporter::addCard(QString id,
                                    bool isCrypt,
                                    QString name,
                                    QString picture_url,
                                    QString text,
                                    QVariantHash properties,
                                    bool isToken)
{
    // Workaround for card name weirdness
    qDebug() << "Importing card: " << name;
    name = name.replace("(TM)", "™");

    // table row
    int tableRow = 1;
    // Todo; Add row location for cards. [Crypt = 0, Master = 2 ?]


    tableRow = 3;
    if (isCrypt) {
        tableRow = 0;
    }
    /*
        Phoenix (He): The layout usually goes:
        top: ready minions (vampires, allies)
        top/bottom: master / event permanents
        bottom: uncontrolled (face-down) and torpor (face-up)
    */


    // for (auto &types : properties.value(VTES::CardTypes).toString()) {
    //     if (type == "")
    // 
    // }
    // if ((mainCardType == "Land"))
    //     tableRow = 0;
    // else if ((mainCardType == "Sorcery") || (mainCardType == "Instant"))
    //     tableRow = 3;
    // else if (mainCardType == "Creature")
    //     tableRow = 2;


    CardInfoPerSetMap setsInfo;// = CardInfoPerSetMap();
    //setsInfo.insert(setInfo.getPtr()->getShortName(), setInfo);
    auto sets = properties.value(VTES::Sets).toStringList();
    // qDebug() << sets;
    // // CardSet setInfo = CardSet::newInstance();
    // // setInfo.
    // //setInfo->
    // for (const auto &set : sets) {
    //     auto instance = CardSet::newInstance(set, set);
    //     setsInfo.insert(set, instance);
    // }

    CardInfoPtr newCard = CardInfo::newInstance(name, text, isCrypt, isToken, properties,
                                                setsInfo, tableRow);
    newCard->setProperty(VTES::Id, id);
    newCard->setProperty(VTES::PicUrl, picture_url);

    // if (name.isEmpty()) {
    //     qDebug() << "warning: an empty card was added to set" << setInfo.getPtr()->getShortName();
    // }
    cards.insert(name, newCard);
    emit cardIndexChanged(id.toInt(), name);

    return newCard;
}

QString OracleImporter::getStringPropertyFromMap(const QVariantMap &card, const QString &propertyName)
{
    return card.contains(propertyName) ? card.value(propertyName).toString() : QString("");
}

int OracleImporter::startImport()
{
    int numCards = 0;// setCards = 0, setIndex = 0;

    QVariantHash properties;
    QString id;
    QString name;
    QString text;
    QString picture_url;

    for (auto it : allCards) {
        //qDebug() << "Card " << it;
        auto obj = it.toObject();
        // qDebug() << "Object " << obj;
        auto types = obj["types"].toArray();
        bool isCrypt;

        /* Uniform elements */
        if (types.contains("Vampire") || types.contains("Imbued")) {
            /* Parse as Crypt Card */
            isCrypt = true;
            properties[VTES::Capacity] = QString::number(obj["capacity"].toInt());
            properties[VTES::Disciplines] = obj["disciplines"].toVariant().toStringList();
            properties[VTES::Group] = obj["group"].toString();
            // properties["title"] = obj["title"].toString();
            properties[VTES::Advanced] = obj["adv"].toBool();
        } else {
            /* Parse as Library Card */
            isCrypt = false;
            properties[VTES::PoolCost] = obj["pool_cost"].toString();
            properties[VTES::BloodCost] = obj["blood_cost"].toString();
        }

        id = QString::number(obj["id"].toInt()); // todo; nullptr
        name = obj["_name"].toString();
        text = obj["card_text"].toString();
        picture_url = obj["url"].toString();

        properties[VTES::Clans] = QJsonArray();
        if (obj.contains("clans")) {
            properties[VTES::Clans] = obj["clans"].toVariant().toStringList();
        }

        // QJsonObject({"_name":"Hester Reed","_set":"BH:U2, POD:DTC","adv":null,"artists":["Rebecca Guay"],
        //              "capacity":3,"card_text":"Sabbat.","clans":["Lasombra"],"disciplines":["obt","pot"],
        //              "group":"3","id":200595,"name":"Hester Reed (G3)","name_variants":["Hester Reed"],
        //              "ordered_sets":["Black Hand"],"printed_name":"Hester Reed",
        //              "scans":{"Black Hand":"https://static.krcg.org/card/set/black-hand/hesterreedg3.jpg",
        //              "Print on Demand":"https://static.krcg.org/card/set/print-on-demand/hesterreedg3.jpg"},
        //              "sets":{"Black Hand":[{"frequency":2,"rarity":"Uncommon","release_date":"2003-11-17"}],
        //               "Print on Demand":[{"copies":1,"precon":"DriveThruCards"}]},
        //               "types":["Vampire"],"url":"https://static.krcg.org/card/hesterreedg3.jpg"})

        properties[VTES::CardTypes] = obj["types"].toVariant().toStringList();
         //.toVariant().toStringList();

        QStringList setList = QStringList();
        auto setsObj = obj["sets"].toObject();
        for (auto &key : setsObj.keys()) {
            setList.append(key);
        }
        properties[VTES::Sets] = setList;
        CardInfoPtr newCard = addCard(id, isCrypt, name, picture_url, text, properties, false);
        numCards++;
    }

    // Todo; set picture_url for the Edge.
    addCard(id, false, "The Edge", picture_url,
            "Gain control of the Edge after a successful bleed\nBurn the Edge during a referendum to gain 1 vote\nIf "
            "you control the Edge during your unlock phase, you may gain 1 pool from the Blood Bank.\n",
            QVariantHash(), true);


    return numCards;
}

bool OracleImporter::saveToFile(const QString &fileName, const QString &sourceUrl, const QString &sourceVersion)
{
    SchrecknetParser parser;
    return parser.saveToFile(sets, cards, fileName, sourceUrl, sourceVersion);
}

void OracleImporter::clear()
{
    CardDatabase::clear();
}
