#include "schrecknetxml.h"

#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QXmlStreamReader>
#include <version_string.h>

#define SCHRECKNET_XML_TAGNAME "schrecknetxml"
#define SCHRECKNET_XML_TAGVER 1
#define SCHRECKNET_XML_SCHEMALOCATION "https://raw.githubusercontent.com/Stolas/SchreckNet/master/doc/schrecknet.xsd"

bool SchrecknetParser::getCanParseFile(const QString &fileName, QIODevice &device)
{
    qDebug() << "[SchrecknetParser] Trying to parse: " << fileName;

    if (!fileName.endsWith(".xml", Qt::CaseInsensitive)) {
        qDebug() << "[SchrecknetParser] Parsing failed: wrong extension";
        return false;
    }

    QXmlStreamReader xml(&device);
    while (!xml.atEnd()) {
        if (xml.readNext() == QXmlStreamReader::StartElement) {
            if (xml.name().toString() == SCHRECKNET_XML_TAGNAME) {
                int version = xml.attributes().value("version").toString().toInt();
                if (version == SCHRECKNET_XML_TAGVER) {
                    return true;
                } else {
                    qDebug() << "[SchrecknetParser] Parsing failed: wrong version" << version;
                    return false;
                }

            } else {
                qDebug() << "[SchrecknetParser] Parsing failed: wrong element tag" << xml.name();
                return false;
            }
        }
    }

    return true;
}

void SchrecknetParser::parseFile(QIODevice &device)
{
    QXmlStreamReader xml(&device);
    while (!xml.atEnd()) {
        if (xml.readNext() == QXmlStreamReader::StartElement) {
            while (!xml.atEnd()) {
                if (xml.readNext() == QXmlStreamReader::EndElement) {
                    break;
                }

                auto name = xml.name().toString();
                //qDebug() << name;
                if (name == "sets") {
                    loadSetsFromXml(xml);
                } else if (name == "crypt_cards") {
                    loadCryptCardsFromXml(xml);
                } else if (name == "library_cards") {
                    loadLibraryCardsFromXml(xml);
                } else if (name == "tokens") {
                    loadTokensFromXml(xml);
                } else if (!name.isEmpty()) {
                    qDebug() << "[SchrecknetParser] Unknown item" << name << ", trying to continue anyway";
                    xml.skipCurrentElement();
                }
            }
        }
    }
}

void SchrecknetParser::loadSetsFromXml(QXmlStreamReader &xml)
{
    while (!xml.atEnd()) {
        if (xml.readNext() == QXmlStreamReader::EndElement) {
            break;
        }

        auto name = xml.name().toString();
        if (name == "set") {
            auto attrs = xml.attributes();
            QString name;
            QDate releaseDate;

            name = attrs.value("name").toString();
            releaseDate = QDate::fromString(attrs.value("release_date").toString(), Qt::ISODate);

            internalAddSet(name, releaseDate); // Todo; find and refactor internalAddSet
        }
    }
}
void SchrecknetParser::loadTokensFromXml(QXmlStreamReader &xml)
{
}

QString SchrecknetParser::getMainCardType(QString &type)
{
    qDebug() << "[SchreckNetLoader] The get MainCardType is still mtg style, got type: " << type;
    QString result = type;
    /*
    Legendary Artifact Creature - Golem
    Instant // Instant
    */

    int pos;
    if ((pos = result.indexOf('-')) != -1) {
        result.remove(pos, result.length());
    }

    if ((pos = result.indexOf("—")) != -1) {
        result.remove(pos, result.length());
    }

    if ((pos = result.indexOf("//")) != -1) {
        result.remove(pos, result.length());
    }

    result = result.simplified();
    /*
    Legendary Artifact Creature
    Instant
    */

    if ((pos = result.lastIndexOf(' ')) != -1) {
        result = result.mid(pos + 1);
    }
    /*
    Creature
    Instant
    */

    return result;
}

void SchrecknetParser::loadCardsFromXml(QXmlStreamReader &xml, bool isCrypt) {
    /* Todo; this is buggy. */
    while (!xml.atEnd()) {
        if (xml.readNext() == QXmlStreamReader::EndElement) {
            break;
        }

        auto xmlName = xml.name().toString();
        if (xmlName == nullptr) { continue; }

        if (xmlName == "card") {
            QString id = QString("");
            QString name = QString("");
            QString text = QString("");
            QVariantHash properties = QVariantHash();
            CardInfoPerSetMap sets = CardInfoPerSetMap();
            int tableRow = 0;

            auto attrs = xml.attributes();
            id = attrs.value("id").toString();
            name = attrs.value("name").toString();
            auto picURL = attrs.value("picurl").toString();
            //xml.readNext();

            while (!xml.atEnd()) {
                if (xml.readNext() == QXmlStreamReader::EndElement) {
                    break;
                }
                // variable - assigned properties
                xmlName = xml.name().toString();
                if (xmlName == nullptr) { continue; }


                if (xmlName == "text") {
                    text = xml.readElementText(QXmlStreamReader::IncludeChildElements);
                } else if (xmlName == "tablerow") {
                    tableRow = xml.readElementText(QXmlStreamReader::IncludeChildElements).toInt();
                } else if (xmlName == "properties") {
                    attrs = xml.attributes();
                    if (isCrypt) {
                        properties.insert(VTES::Capacity, attrs.value("capacity").toString());
                        properties.insert(VTES::Group, attrs.value("group").toString());
                    } else {
                        properties.insert(VTES::PoolCost, attrs.value("pool").toString());
                        properties.insert(VTES::BloodCost, attrs.value("blood").toString());
                    }

                    /* Properties */
                    while (!xml.atEnd()) {
                        if (xml.readNext() == QXmlStreamReader::EndElement) {
                            break;
                        }
                        xmlName = xml.name().toString();
                        if (xmlName == nullptr) { continue; }

                        if (xmlName == "types") {
                            QString type = xml.readElementText(QXmlStreamReader::IncludeChildElements);
                            properties.insert(VTES::CardTypes, type);
                        } else if (xmlName == "clans") {
                            // Todo; Something like this. This is buggy as a stack value is stored in properties.
                            // QStringList clanList;
                            // while (!xml.atEnd()) {
                            //     if (xml.readNext() == QXmlStreamReader::EndElement) {
                            //         break;
                            //     }
                            //     xmlName = xml.name().toString();
                            //     if (xmlName == "clan") {
                            //         clanList.append(xml.readElementText(QXmlStreamReader::IncludeChildElements));
                            //     }
                            // }
                            // qDebug() << "Todo; load Clans";
                            // properties.insert(VTES::Clans, clanList);
                        } else if (xmlName == "disciplines") {
                            if (!isCrypt) {
                                qDebug() << "[SchrecknetParser] Library card has " << xmlName
                                         << ", trying to continue anyway";
                                xml.skipCurrentElement();
                                continue;
                            }
                            QString clans = xml.readElementText(QXmlStreamReader::IncludeChildElements);

                        } else if (xmlName == "sets") {
                            QString setName = "Final Nights";
                            CardInfoPerSet setInfo(internalAddSet(setName));
                            sets.insert(setName, setInfo);
                        } else if (!xmlName.isEmpty()) {
                            qDebug() << "[SchrecknetParser] Unknown card property" << xmlName
                                     << ", trying to continue anyway";
                        }
                    } /* End of Properties */
                }
            }
            //qDebug() << properties;
            CardInfoPtr newCard = CardInfo::newInstance(name, text, false, false, properties, sets, tableRow);
            emit addCard(newCard);
        }
    }
}


void SchrecknetParser::loadCryptCardsFromXml(QXmlStreamReader & xml)
{
    return loadCardsFromXml(xml, true);
}

void SchrecknetParser::loadLibraryCardsFromXml(QXmlStreamReader & xml) {
    return loadCardsFromXml(xml, false);
}

static QXmlStreamWriter &operator<<(QXmlStreamWriter &xml, const CardSetPtr &set)
{
    if (set.isNull()) {
        qDebug() << "&operator<< set is nullptr";
        return xml;
    }

    xml.writeStartElement("set");
    xml.writeTextElement("name", set->getShortName());
    xml.writeTextElement("longname", set->getLongName());
    xml.writeTextElement("settype", set->getSetType());
    xml.writeTextElement("releasedate", set->getReleaseDate().toString(Qt::ISODate));
    xml.writeEndElement();

    return xml;
}

static QXmlStreamWriter &operator<<(QXmlStreamWriter &xml, const CardInfoPtr &info)
{
    if (info.isNull()) {
        qDebug() << "operator<< info is nullptr";
        return xml;
    }

    QString tmpString;


    // info->getIsCrypt();

    xml.writeStartElement("card");
    xml.writeAttribute(QXmlStreamAttribute("id", info->getProperty("id")));
    xml.writeAttribute(QXmlStreamAttribute("name", info->getName()));
    xml.writeAttribute(QXmlStreamAttribute("picurl", info->getPicURL(nullptr)));

    // variable - assigned properties
    xml.writeTextElement("text", info->getText());

    // generic properties
    xml.writeStartElement("properties");
    if (info->getIsCrypt()) {
        xml.writeAttribute(QXmlStreamAttribute("capacity", info->getCapacity()));
        xml.writeAttribute(QXmlStreamAttribute("group", info->getGroup()));
    } else {
        xml.writeAttribute(QXmlStreamAttribute("pool", info->getBlood()));
        xml.writeAttribute(QXmlStreamAttribute("blood", info->getPool()));
    }

    xml.writeStartElement("types");
    for (auto &type : info->getCardTypes()) {
        xml.writeStartElement("type");
        qDebug() << info->getName() << type;
        xml.writeAttribute(QXmlStreamAttribute("name", type));
        xml.writeEndElement(); /* type */
    }
    xml.writeEndElement(); /* types */

    xml.writeStartElement("clans");
    for (auto &clan : info->getClans()) {
        xml.writeTextElement("clan", clan);
    }
    xml.writeEndElement(); /* clans */


    xml.writeEndElement(); /* properites */


    // sets
    const CardInfoPerSetMap sets = info->getSets();
    for (CardInfoPerSet set : sets) {
        xml.writeStartElement("set");
        xml.writeAttribute("rarity", set.getProperty("rarity"));
        xml.writeAttribute("muId", set.getProperty("muid"));
        xml.writeAttribute("uuId", set.getProperty("uuid"));

        tmpString = set.getProperty("num");
        if (!tmpString.isEmpty()) {
            xml.writeAttribute("num", tmpString);
        }

        tmpString = set.getProperty("picurl");
        if (!tmpString.isEmpty()) {
            xml.writeAttribute("picURL", tmpString);
        }

        xml.writeCharacters(set.getPtr()->getShortName());
        xml.writeEndElement();
    }

    // positioning
    xml.writeTextElement("tablerow", QString::number(info->getTableRow()));

    xml.writeEndElement(); // card

    return xml;
}

bool SchrecknetParser::saveToFile(SetNameMap sets,
                                      CardNameMap cards,
                                      const QString &fileName,
                                      const QString &sourceUrl,
                                      const QString &sourceVersion)
{
    qDebug() << fileName;
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }

    QXmlStreamWriter xml(&file);

    xml.setAutoFormatting(true);
    xml.writeStartDocument();
    xml.writeStartElement(SCHRECKNET_XML_TAGNAME);
    xml.writeAttribute("version", QString::number(SCHRECKNET_XML_TAGVER));
    xml.writeAttribute("xmlns:xsi", COCKATRICE_XML_XSI_NAMESPACE);
    xml.writeAttribute("xsi:schemaLocation", SCHRECKNET_XML_SCHEMALOCATION);

    xml.writeStartElement("info");
    xml.writeTextElement("author", QCoreApplication::applicationName() + QString(" %1").arg(VERSION_STRING));
    xml.writeTextElement("createdAt", QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    xml.writeTextElement("sourceUrl", sourceUrl);
    xml.writeTextElement("sourceVersion", sourceVersion);
    xml.writeEndElement();

    if (sets.count() > 0) {
        xml.writeStartElement("sets");
        for (CardSetPtr set : sets) {
            xml << set;
        }
        xml.writeEndElement();
    }

    /* Note; yes this is slow, yes this should be optimized, no I dont care. */
    if (cards.count() > 0) {
        xml.writeStartElement("crypt_cards");
        for (CardInfoPtr card : cards) {
            if (card->getIsCrypt() && !card->getIsToken()) {
                xml << card;
            }
        }
        xml.writeEndElement();
    }
    if (cards.count() > 0) {
        xml.writeStartElement("library_cards");
        for (CardInfoPtr card : cards) {
            if (!card->getIsCrypt() && !card->getIsToken()) {
                xml << card;
            }
        }
        xml.writeEndElement();
    }

    if (cards.count() > 0) {
        xml.writeStartElement("tokens");
        for (CardInfoPtr card : cards) {
            if (card->getIsToken()) {
                xml << card;
            }
        }
        xml.writeEndElement();
    }

    xml.writeEndElement(); // cockatrice_carddatabase
    xml.writeEndDocument();

    return true;
}
