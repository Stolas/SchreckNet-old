#include "carddatabase.h"

#include "carddbparser/schrecknetxml.h"
#include "game_specific_terms.h"
#include "pictureloader.h"
#include "settingscache.h"
#include "spoilerbackgroundupdater.h"

#include <QCryptographicHash>
#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QMessageBox>
#include <QRegularExpression>
#include <algorithm>
#include <utility>

const char *CardDatabase::TOKENS_SETNAME = "TK";

CardSet::CardSet(const QString &_shortName,
                 const QString &_longName,
                 const QString &_setType,
                 const QDate &_releaseDate)
    : shortName(_shortName), longName(_longName), releaseDate(_releaseDate), setType(_setType)
{
    loadSetOptions();
}

CardSetPtr CardSet::newInstance(const QString &_shortName,
                                const QString &_longName,
                                const QString &_setType,
                                const QDate &_releaseDate)
{
    CardSetPtr ptr(new CardSet(_shortName, _longName, _setType, _releaseDate));
    // ptr->setSmartPointer(ptr);
    return ptr;
}

QString CardSet::getCorrectedShortName() const
{
    // For Windows machines.
    QSet<QString> invalidFileNames;
    invalidFileNames << "CON"
                     << "PRN"
                     << "AUX"
                     << "NUL"
                     << "COM1"
                     << "COM2"
                     << "COM3"
                     << "COM4"
                     << "COM5"
                     << "COM6"
                     << "COM7"
                     << "COM8"
                     << "COM9"
                     << "LPT1"
                     << "LPT2"
                     << "LPT3"
                     << "LPT4"
                     << "LPT5"
                     << "LPT6"
                     << "LPT7"
                     << "LPT8"
                     << "LPT9";

    return invalidFileNames.contains(shortName) ? shortName + "_" : shortName;
}

void CardSet::loadSetOptions()
{
    sortKey = SettingsCache::instance().cardDatabase().getSortKey(shortName);
    enabled = SettingsCache::instance().cardDatabase().isEnabled(shortName);
    isknown = SettingsCache::instance().cardDatabase().isKnown(shortName);
}

void CardSet::setSortKey(unsigned int _sortKey)
{
    sortKey = _sortKey;
    SettingsCache::instance().cardDatabase().setSortKey(shortName, _sortKey);
}

void CardSet::setEnabled(bool _enabled)
{
    enabled = _enabled;
    SettingsCache::instance().cardDatabase().setEnabled(shortName, _enabled);
}

void CardSet::setIsKnown(bool _isknown)
{
    isknown = _isknown;
    SettingsCache::instance().cardDatabase().setIsKnown(shortName, _isknown);
}

class SetList::KeyCompareFunctor
{
public:
    inline bool operator()(const CardSetPtr &a, const CardSetPtr &b) const
    {
        if (a.isNull() || b.isNull()) {
            qDebug() << "SetList::KeyCompareFunctor a or b is null";
            return false;
        }

        return a->getSortKey() < b->getSortKey();
    }
};

void SetList::sortByKey()
{
    std::sort(begin(), end(), KeyCompareFunctor());
}

int SetList::getEnabledSetsNum()
{
    int num = 0;
    for (int i = 0; i < size(); ++i) {
        CardSetPtr set = at(i);
        if (set && set->getEnabled()) {
            ++num;
        }
    }
    return num;
}

int SetList::getUnknownSetsNum()
{
    int num = 0;
    for (int i = 0; i < size(); ++i) {
        CardSetPtr set = at(i);
        if (set && !set->getIsKnown() && !set->getIsKnownIgnored()) {
            ++num;
        }
    }
    return num;
}

QStringList SetList::getUnknownSetsNames()
{
    QStringList sets = QStringList();
    for (int i = 0; i < size(); ++i) {
        CardSetPtr set = at(i);
        if (set && !set->getIsKnown() && !set->getIsKnownIgnored()) {
            sets << set->getShortName();
        }
    }
    return sets;
}

void SetList::enableAllUnknown()
{
    for (int i = 0; i < size(); ++i) {
        CardSetPtr set = at(i);
        if (set && !set->getIsKnown() && !set->getIsKnownIgnored()) {
            set->setIsKnown(true);
            set->setEnabled(true);
        } else if (set && set->getIsKnownIgnored() && !set->getEnabled()) {
            set->setEnabled(true);
        }
    }
}

void SetList::enableAll()
{
    for (int i = 0; i < size(); ++i) {
        CardSetPtr set = at(i);

        if (set == nullptr) {
            qDebug() << "enabledAll has null";
            continue;
        }

        if (!set->getIsKnownIgnored()) {
            set->setIsKnown(true);
        }

        set->setEnabled(true);
    }
}

void SetList::markAllAsKnown()
{
    for (int i = 0; i < size(); ++i) {
        CardSetPtr set = at(i);
        if (set && !set->getIsKnown() && !set->getIsKnownIgnored()) {
            set->setIsKnown(true);
            set->setEnabled(false);
        } else if (set && set->getIsKnownIgnored() && !set->getEnabled()) {
            set->setEnabled(true);
        }
    }
}

void SetList::guessSortKeys()
{
    // sort by release date DESC; invalid dates to the bottom.
    QDate distantFuture(2050, 1, 1);
    int aHundredYears = 36500;
    for (int i = 0; i < size(); ++i) {
        CardSetPtr set = at(i);
        if (set.isNull()) {
            qDebug() << "guessSortKeys set is null";
            continue;
        }

        QDate date = set->getReleaseDate();
        if (date.isNull()) {
            set->setSortKey(static_cast<unsigned int>(aHundredYears));
        } else {
            set->setSortKey(static_cast<unsigned int>(date.daysTo(distantFuture)));
        }
    }
}

CardInfoPerSet::CardInfoPerSet(const CardSetPtr &_set) : set(_set)
{
}

CardInfo::CardInfo(const QString &_name,
                   const QString &_text,
                   bool _isCrypt,
                   bool _isToken,
                   QVariantHash _properties,
                   CardInfoPerSetMap _sets,
                   int _tableRow )
    : name(_name), text(_text), isCrypt(_isCrypt), isToken(_isToken), properties(std::move(_properties)), 
      sets(std::move(_sets)), tableRow(_tableRow)
{
    pixmapCacheKey = QLatin1String("card_") + name;
    simpleName = CardInfo::simplifyName(name);

    refreshCachedSetNames();
}

CardInfo::~CardInfo()
{
    PictureLoader::clearPixmapCache(smartThis);
}

CardInfoPtr CardInfo::newInstance(const QString &_name,
                                  const QString &_text,
                                  bool _isCrypt,
                                  bool _isToken,
                                  QVariantHash _properties,
                                  CardInfoPerSetMap _sets,
                                  int _tableRow)
{
    CardInfoPtr ptr(new CardInfo(_name, _text, _isCrypt, _isToken, std::move(_properties), _sets, _tableRow));
    ptr->setSmartPointer(ptr);

    for (const CardInfoPerSet &set : _sets) {
        set.getPtr()->append(ptr);
    }

    return ptr;
}


QString CardInfo::getCorrectedName() const
{
    // Todo; Mithras needs to be filtered.
    return name;
    //QString result = name;
    // Fire // Ice, Circle of Protection: Red, "Ach! Hans, Run!", Who/What/When/Where/Why, Question Elemental?
    //return result.remove(" // ").remove(':').remove('"').remove('?').replace('/', ' ');
}

void CardInfo::addToSet(const CardSetPtr &_set, const CardInfoPerSet _info)
{
    _set->append(smartThis);
    sets.insert(_set->getShortName(), _info);

    refreshCachedSetNames();
}

void CardInfo::refreshCachedSetNames()
{
    QStringList setList;
    // update the cached list of set names
    for (const auto &set : sets) {
        if (set.getPtr()->getEnabled()) {
            setList << set.getPtr()->getShortName();
        }
    }
    setsNames = setList.join(", ");
}

QString CardInfo::simplifyName(const QString &name)
{
    static const QRegularExpression spaceOrSplit("(\\s+|\\/\\/.*)");
    static const QRegularExpression nonAlnum("[^a-z0-9]");

    QString simpleName = name.toLower();

    // remove spaces and right halves of split cards
    simpleName.remove(spaceOrSplit);

    // So Aetherling would work, but not Ætherling since 'Æ' would get replaced
    // with nothing.
    simpleName.replace("™", "(TM)");

    // Replace Jötun Grunt with Jotun Grunt.
    simpleName = simpleName.normalized(QString::NormalizationForm_KD);

    // remove all non alphanumeric characters from the name
    simpleName.remove(nonAlnum);
    return simpleName;
}

CardDatabase::CardDatabase(QObject *parent) : QObject(parent), loadStatus(NotLoaded)
{
    qRegisterMetaType<CardInfoPtr>("CardInfoPtr");
    qRegisterMetaType<CardInfoPtr>("CardSetPtr");

    // add new parsers here
    availableParsers << new SchrecknetParser;

    for (auto &parser : availableParsers) {
        connect(parser, SIGNAL(addCard(CardInfoPtr)), this, SLOT(addCard(CardInfoPtr)), Qt::DirectConnection);
        connect(parser, SIGNAL(addSet(CardSetPtr)), this, SLOT(addSet(CardSetPtr)), Qt::DirectConnection);
    }

    connect(&SettingsCache::instance(), SIGNAL(cardDatabasePathChanged()), this, SLOT(loadCardDatabases()));
}

CardDatabase::~CardDatabase()
{
    clear();
    qDeleteAll(availableParsers);
}

void CardDatabase::clear()
{
    clearDatabaseMutex->lock();

    QHashIterator<QString, CardInfoPtr> i(cards);
    while (i.hasNext()) {
        i.next();
        if (i.value()) {
            removeCard(i.value());
        }
    }

    cards.clear();
    simpleNameCards.clear();

    sets.clear();
    ICardDatabaseParser::clearSetlist();

    loadStatus = NotLoaded;

    clearDatabaseMutex->unlock();
}

void CardDatabase::addCard(CardInfoPtr card)
{
    if (card == nullptr) {
        qDebug() << "addCard(nullptr)";
        return;
    }

    // if card already exists just add the new set property
    if (cards.contains(card->getName())) {
        CardInfoPtr sameCard = cards[card->getName()];
        for (const CardInfoPerSet &set : card->getSets()) {
            sameCard->addToSet(set.getPtr(), set);
        }
        return;
    }

    addCardMutex->lock();
    cards.insert(card->getName(), card);
    simpleNameCards.insert(card->getSimpleName(), card);
    addCardMutex->unlock();
    emit cardAdded(card);
}

void CardDatabase::removeCard(CardInfoPtr card)
{
    if (card.isNull()) {
        qDebug() << "removeCard(nullptr)";
        return;
    }

    removeCardMutex->lock();
    cards.remove(card->getName());
    simpleNameCards.remove(card->getSimpleName());
    removeCardMutex->unlock();
    emit cardRemoved(card);
}

CardInfoPtr CardDatabase::getCard(const QString &cardName) const
{
    return getCardFromMap(cards, cardName);
}

QList<CardInfoPtr> CardDatabase::getCards(const QStringList &cardNames) const
{
    QList<CardInfoPtr> cardInfos;
    foreach (QString cardName, cardNames) {
        CardInfoPtr ptr = getCardFromMap(cards, cardName);
        if (ptr)
            cardInfos.append(ptr);
    }

    return cardInfos;
}

CardInfoPtr CardDatabase::getCardBySimpleName(const QString &cardName) const
{
    return getCardFromMap(simpleNameCards, CardInfo::simplifyName(cardName));
}

CardInfoPtr CardDatabase::guessCard(const QString &cardName) const
{
    CardInfoPtr temp = getCard(cardName);
    if (temp == nullptr) { // get card by simple name instead
        temp = getCardBySimpleName(cardName);
        if (temp == nullptr) { // still could not find the card, so simplify the cardName too
            QString simpleCardName = CardInfo::simplifyName(cardName);
            temp = getCardBySimpleName(simpleCardName);
        }
    }
    return temp; // returns nullptr if not found
}

CardSetPtr CardDatabase::getSet(const QString &setName)
{
    if (sets.contains(setName)) {
        return sets.value(setName);
    } else {
        CardSetPtr newSet = CardSet::newInstance(setName);
        sets.insert(setName, newSet);
        return newSet;
    }
}

void CardDatabase::addSet(CardSetPtr set)
{
    sets.insert(set->getShortName(), set);
}

SetList CardDatabase::getSetList() const
{
    SetList result;
    QHashIterator<QString, CardSetPtr> i(sets);
    while (i.hasNext()) {
        i.next();
        result << i.value();
    }
    return result;
}

CardInfoPtr CardDatabase::getCardFromMap(const CardNameMap &cardMap, const QString &cardName) const
{
    if (cardMap.contains(cardName))
        return cardMap.value(cardName);

    return {};
}

LoadStatus CardDatabase::loadFromFile(const QString &fileName)
{
    QFile file(fileName);
    file.open(QIODevice::ReadOnly);
    if (!file.isOpen()) {
        auto errStr = file.errorString();
        qDebug() << "Failed to load file: " << errStr;
        return FileError;
    }

    for (auto parser : availableParsers) {
        file.reset();
        if (parser->getCanParseFile(fileName, file)) {
            file.reset();
            parser->parseFile(file);
            return Ok;
        }
    }

    return Invalid;
}

LoadStatus CardDatabase::loadCardDatabase(const QString &path)
{
    auto startTime = QTime::currentTime();
    LoadStatus tempLoadStatus = NotLoaded;
    if (!path.isEmpty()) {
        loadFromFileMutex->lock();
        tempLoadStatus = loadFromFile(path);
        loadFromFileMutex->unlock();
    }

    int msecs = startTime.msecsTo(QTime::currentTime());
    qDebug() << "[CardDatabase] loadCardDatabase(): Path =" << path << "Status =" << tempLoadStatus
             << "Cards =" << cards.size() << "Sets =" << sets.size() << QString("%1ms").arg(msecs);

    return tempLoadStatus;
}

LoadStatus CardDatabase::loadCardDatabases()
{
    reloadDatabaseMutex->lock();

    qDebug() << "CardDatabase::loadCardDatabases start";

    clear(); // remove old db

    loadStatus = loadCardDatabase(SettingsCache::instance().getCardDatabasePath()); // load main card database
    loadCardDatabase(SettingsCache::instance().getTokenDatabasePath());             // load tokens database
    loadCardDatabase(SettingsCache::instance().getSpoilerCardDatabasePath());       // load spoilers database

    // find all custom card databases, recursively & following symlinks
    // then load them alphabetically
    QDirIterator customDatabaseIterator(SettingsCache::instance().getCustomCardDatabasePath(), QStringList() << "*.xml",
                                        QDir::Files, QDirIterator::Subdirectories | QDirIterator::FollowSymlinks);
    QStringList databasePaths;
    while (customDatabaseIterator.hasNext()) {
        customDatabaseIterator.next();
        databasePaths.push_back(customDatabaseIterator.filePath());
    }
    databasePaths.sort();

    for (auto i = 0; i < databasePaths.size(); ++i) {
        const auto &databasePath = databasePaths.at(i);
        qDebug() << "Loading Custom Set" << i << "(" << databasePath << ")";
        loadCardDatabase(databasePath);
    }

    // AFTER all the cards have been loaded


    if (loadStatus == Ok) {
        checkUnknownSets(); // update deck editors, etc
        qDebug() << "CardDatabase::loadCardDatabases success";
    } else {
        qDebug() << "CardDatabase::loadCardDatabases failed";
        emit cardDatabaseLoadingFailed(); // bring up the settings dialog
    }

    reloadDatabaseMutex->unlock();
    return loadStatus;
}

QStringList CardDatabase::getAllMainCardTypes() const
{
    QSet<QString> types;
    QHashIterator<QString, CardInfoPtr> cardIterator(cards);
    while (cardIterator.hasNext()) {
        auto card = cardIterator.next().value();
        for (auto &type : card->getPropertyList(VTES::CardTypes)) {
            types.insert(type);
        }
    }
    return types.values();
}

void CardDatabase::checkUnknownSets()
{
    /* Todo; Fix the sets stuff. */
    return;

    SetList sets = getSetList();

    if (sets.getEnabledSetsNum()) {
        // if some sets are first found on this run, ask the user
        int numUnknownSets = sets.getUnknownSetsNum();
        QStringList unknownSetNames = sets.getUnknownSetsNames();
        if (numUnknownSets > 0) {
            emit cardDatabaseNewSetsFound(numUnknownSets, unknownSetNames);
        } else {
            sets.markAllAsKnown();
        }
    } else {
        // No set enabled. Probably this is the first time running trice
        sets.guessSortKeys();
        sets.sortByKey();
        sets.enableAll();
        notifyEnabledSetsChanged();

        emit cardDatabaseAllNewSetsEnabled();
    }
}

void CardDatabase::enableAllUnknownSets()
{
    SetList sets = getSetList();
    sets.enableAllUnknown();
}

void CardDatabase::markAllSetsAsKnown()
{
    SetList sets = getSetList();
    sets.markAllAsKnown();
}

void CardDatabase::notifyEnabledSetsChanged()
{
    // refresh the list of cached set names
    for (const CardInfoPtr &card : cards)
        card->refreshCachedSetNames();

    // inform the carddatabasemodels that they need to re-check their list of cards
    emit cardDatabaseEnabledSetsChanged();
}

bool CardDatabase::saveCustomTokensToFile()
{
    QString fileName =
        SettingsCache::instance().getCustomCardDatabasePath() + "/" + CardDatabase::TOKENS_SETNAME + ".xml";

    SetNameMap tmpSets;
    CardSetPtr customTokensSet = getSet(CardDatabase::TOKENS_SETNAME);
    tmpSets.insert(CardDatabase::TOKENS_SETNAME, customTokensSet);

    CardNameMap tmpCards;
    for (const CardInfoPtr &card : cards) {
        if (card->getSets().contains(CardDatabase::TOKENS_SETNAME)) {
            tmpCards.insert(card->getName(), card);
        }
    }

    availableParsers.first()->saveToFile(tmpSets, tmpCards, fileName);
    return true;
}

// Back-compatibility methods. Remove ASAP -> Nah I like these for schrecknet
const QStringList CardInfo::getCardTypes() const
{
    return getPropertyList(VTES::CardTypes);
}
const QStringList CardInfo::getDisciplines() const
{
    return getPropertyList(VTES::Disciplines);
}
void CardInfo::setCardTypes(const QStringList& values)
{
    setPropertyList(VTES::CardTypes, values);
}
const QString CardInfo::getCapacity() const
{
    return getProperty(VTES::Capacity);
}
const QString CardInfo::getPool() const
{
    return getProperty(VTES::PoolCost);
}
const QString CardInfo::getBlood() const
{
    return getProperty(VTES::BloodCost);
}
const QStringList CardInfo::getClans() const
{
    return getPropertyList(VTES::Clans);
}
