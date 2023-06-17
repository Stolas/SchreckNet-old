#include "schrecknet.h"

#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QXmlStreamReader>
#include <version_string.h>

#define SCHRECKNET_XML_TAGNAME "schrecknetxml"
#define SCHRECKNET_XML_TAGVER 1
#define SCHRECKNET_XML_SCHEMALOCATION                                                                                 \
    "https://raw.githubusercontent.com/Cockatrice/Cockatrice/master/doc/carddatabase_v3/cards.xsd"

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
                if (name == "sets") {
                    loadSetsFromXml(xml);
                } else if (name == "crypt_cards") {
                    loadCryptCardsFromXml(xml);
                } else if (name == "library_cards") {
                    loadLibraryCardsFromXml(xml);
                } else if (name == "tokens") {
                    loadTokensCardsFromXml(xml);
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
            auto attributes = xml.attributes();
            QString name;
            QDate releaseDate;

            name = attributes.value("name");
            release_date = QDate::fromString(attributes.value("release_date"), Qt::ISODate);

            internalAddSet(name, releaseDate); // Todo; find and refactor internalAddSet
        }
    }
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

void SchrecknetParser::loadCardsFromXml(QXmlStreamReader &xml, bool isCrypt)
{
    while (!xml.atEnd()) {
        if (xml.readNext() == QXmlStreamReader::EndElement) {
            break;
        }

        auto xmlName = xml.name().toString();
        if (xmlName == "card") {
            QString id = QString("");
            QString name = QString("");
            QString text = QString("");
            QVariantHash properties = QVariantHash();
            CardInfoPerSetMap sets = CardInfoPerSetMap();
            int tableRow = 0; // Todo; Determine which is good for Crypt and which is good for lib

            auto attrs = xml.attributes();
            id = attrs.value("id").toString();
            name = attributes.value("name").toString();
            name = attributes.value("picture").toString();

            while (!xml.atEnd()) {
                if (xml.readNext() == QXmlStreamReader::EndElement) {
                    break;
                }
                xmlName = xml.name().toString();

                // variable - assigned properties
                if (xmlName == "text") {
                    text = xml.readElementText(QXmlStreamReader::IncludeChildElements);
                } else if (xmlName == "properties") {
                    attrs = xml.attributes();

                    if (isCrypt) {
                        capacity = attrs.value("capacity").toInt()
                        group = attrs.value("group").toInt()
                    } else {
                        auto pool = attrs.value("pool").toString()
                        auto blood = attrs.value("blood").toString()
                    }

                    while (!xml.atEnd()) {
                        if (xml.readNext() == QXmlStreamReader::EndElement) {
                            break;
                        }
                        xmlName = xml.name().toString();

                        if (xmlName == "types") {
                            QString type = xml.readElementText(QXmlStreamReader::IncludeChildElements);
                            properties.insert("type", type);
                            properties.insert("maintype", getMainCardType(type));
                        } else if (xmlName == "clans") {
                        } else if (xmlName == "disciplines") {
                            if (!isCrypt) {
                                qDebug() << "[SchrecknetParser] Library card has " << xmlName
                                         << ", trying to continue anyway";
                                xml.skipCurrentElement();
                            }
                        } else if (xmlName == "sets") {
                            QString setName = "Final Nights";
                            CardInfoPerSet setInfo(internalAddSet(setName));
                            sets.insert(setName, setInfo);
                        } else if (!xmlName.isEmpty()) {
                            qDebug() << "[SchrecknetParser] Unknown card property" << xmlName
                                     << ", trying to continue anyway";
                        }
                    }
                }
            CardInfoPtr newCard = CardInfo::newInstance(name, text, false, properties, sets, tableRow, true);
            emit addCard(newCard);
        }
    }
}

void SchrecknetParser::loadCryptCardsFromXml(QXmlStreamReader &xml)
    return loadCryptCardsFromXml(xml, true);
}

void SchrecknetParser::loadLibraryCardsFromXml(QXmlStreamReader &xml)
    return loadCryptCardsFromXml(xml, false);
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

    xml.writeStartElement("card");

    // variable - assigned properties
    xml.writeTextElement("name", info->getName());
    xml.writeTextElement("text", info->getText());
    if (info->getIsToken()) {
        xml.writeTextElement("token", "1");
    }

    // generic properties
    xml.writeTextElement("manacost", info->getProperty("manacost"));
    xml.writeTextElement("cmc", info->getProperty("cmc"));
    xml.writeTextElement("type", info->getProperty("type"));

    int colorSize = info->getColors().size();
    for (int i = 0; i < colorSize; ++i) {
        xml.writeTextElement("color", info->getColors().at(i));
    }

    tmpString = info->getProperty("pt");
    if (!tmpString.isEmpty()) {
        xml.writeTextElement("pt", tmpString);
    }

    tmpString = info->getProperty("loyalty");
    if (!tmpString.isEmpty()) {
        xml.writeTextElement("loyalty", tmpString);
    }

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

    // related cards
    const QList<CardRelation *> related = info->getRelatedCards();
    for (auto i : related) {
        xml.writeStartElement("related");
        if (i->getDoesAttach()) {
            xml.writeAttribute("attach", "attach");
        }
        if (i->getIsCreateAllExclusion()) {
            xml.writeAttribute("exclude", "exclude");
        }

        if (i->getIsVariable()) {
            if (1 == i->getDefaultCount()) {
                xml.writeAttribute("count", "x");
            } else {
                xml.writeAttribute("count", "x=" + QString::number(i->getDefaultCount()));
            }
        } else if (1 != i->getDefaultCount()) {
            xml.writeAttribute("count", QString::number(i->getDefaultCount()));
        }
        xml.writeCharacters(i->getName());
        xml.writeEndElement();
    }
    const QList<CardRelation *> reverseRelated = info->getReverseRelatedCards();
    for (auto i : reverseRelated) {
        xml.writeStartElement("reverse-related");
        if (i->getDoesAttach()) {
            xml.writeAttribute("attach", "attach");
        }

        if (i->getIsCreateAllExclusion()) {
            xml.writeAttribute("exclude", "exclude");
        }

        if (i->getIsVariable()) {
            if (1 == i->getDefaultCount()) {
                xml.writeAttribute("count", "x");
            } else {
                xml.writeAttribute("count", "x=" + QString::number(i->getDefaultCount()));
            }
        } else if (1 != i->getDefaultCount()) {
            xml.writeAttribute("count", QString::number(i->getDefaultCount()));
        }
        xml.writeCharacters(i->getName());
        xml.writeEndElement();
    }

    // positioning
    xml.writeTextElement("tablerow", QString::number(info->getTableRow()));
    if (info->getCipt()) {
        xml.writeTextElement("cipt", "1");
    }
    if (info->getUpsideDownArt()) {
        xml.writeTextElement("upsidedown", "1");
    }

    xml.writeEndElement(); // card

    return xml;
}

bool SchrecknetParser::saveToFile(SetNameMap sets,
                                      CardNameMap cards,
                                      const QString &fileName,
                                      const QString &sourceUrl,
                                      const QString &sourceVersion)
{
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

    if (cards.count() > 0) {
        xml.writeStartElement("cards");
        for (CardInfoPtr card : cards) {
            xml << card;
        }
        xml.writeEndElement();
    }

    xml.writeEndElement(); // cockatrice_carddatabase
    xml.writeEndDocument();

    return true;
}
