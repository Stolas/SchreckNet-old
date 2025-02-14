#ifndef CARDDATABASE_H
#define CARDDATABASE_H

#include <QBasicMutex>
#include <QDate>
#include <QHash>
#include <QList>
#include <QMap>
#include <QMetaType>
#include <QSharedPointer>
#include <QStringList>
#include <QVariant>
#include <QVector>
#include <utility>
#include "game_specific_terms.h"

class CardDatabase;
class CardInfo;
class CardInfoPerSet;
class CardSet;
//class CardRelation;
class ICardDatabaseParser;

typedef QMap<QString, QString> QStringMap;
typedef QSharedPointer<CardInfo> CardInfoPtr;
typedef QSharedPointer<CardSet> CardSetPtr;
typedef QMap<QString, CardInfoPerSet> CardInfoPerSetMap;

Q_DECLARE_METATYPE(CardInfoPtr)

class CardSet : public QList<CardInfoPtr>
{
private:
    QString shortName, longName;
    unsigned int sortKey;
    QDate releaseDate;
    QString setType;
    bool enabled, isknown;

public:
    explicit CardSet(const QString &_shortName = QString(),
                     const QString &_longName = QString(),
                     const QString &_setType = QString(),
                     const QDate &_releaseDate = QDate());
    static CardSetPtr newInstance(const QString &_shortName = QString(),
                                  const QString &_longName = QString(),
                                  const QString &_setType = QString(),
                                  const QDate &_releaseDate = QDate());
    QString getCorrectedShortName() const;
    QString getShortName() const
    {
        return shortName;
    }
    QString getLongName() const
    {
        return longName;
    }
    QString getSetType() const
    {
        return setType;
    }
    QDate getReleaseDate() const
    {
        return releaseDate;
    }
    void setLongName(const QString &_longName)
    {
        longName = _longName;
    }
    void setSetType(const QString &_setType)
    {
        setType = _setType;
    }
    void setReleaseDate(const QDate &_releaseDate)
    {
        releaseDate = _releaseDate;
    }

    void loadSetOptions();
    int getSortKey() const
    {
        return sortKey;
    }
    void setSortKey(unsigned int _sortKey);
    bool getEnabled() const
    {
        return enabled;
    }
    void setEnabled(bool _enabled);
    bool getIsKnown() const
    {
        return isknown;
    }
    void setIsKnown(bool _isknown);

    // Determine incomplete sets.
    bool getIsKnownIgnored() const
    {
        return longName.length() + setType.length() + releaseDate.toString().length() == 0;
    }
};

class SetList : public QList<CardSetPtr>
{
private:
    class KeyCompareFunctor;

public:
    void sortByKey();
    void guessSortKeys();
    void enableAllUnknown();
    void enableAll();
    void markAllAsKnown();
    int getEnabledSetsNum();
    int getUnknownSetsNum();
    QStringList getUnknownSetsNames();
};

class CardInfoPerSet
{
public:
    explicit CardInfoPerSet(const CardSetPtr &_set = QSharedPointer<CardSet>(nullptr));
    ~CardInfoPerSet() = default;

private:
    CardSetPtr set;
    // per-set card properties;
    QVariantHash properties;

public:
    const CardSetPtr getPtr() const
    {
        return set;
    }
    const QStringList getProperties() const
    {
        return properties.keys();
    }
    const QString getProperty(const QString &propertyName) const
    {
        return properties.value(propertyName).toString();
    }
    void setProperty(const QString &_name, const QString &_value)
    {
        properties.insert(_name, _value);
    }
};

class CardInfo : public QObject
{
    Q_OBJECT
private:
    CardInfoPtr smartThis;
    // The card name
    QString name;
    // The name without punctuation or capitalization, for better card name recognition.
    QString simpleName;
    // The key used to identify this card in the cache
    QString pixmapCacheKey;
    // card text
    QString text;
    // Is this a crypt card 'yellow'
    bool isCrypt;
    // whether this is not a "real" card but a token
    bool isToken;
    // basic card properties; common for all the sets
    QVariantHash properties;
    // card sets
    CardInfoPerSetMap sets;
    // cached set names
    QString setsNames;
    // positioning properties; used by UI
    int tableRow;

public:
    explicit CardInfo(const QString &_name = QString(),
                      const QString &_text = QString(),
                      bool _isCrypt = false,
                      bool _isToken = false,
                      QVariantHash _properties = QVariantHash(),
                      CardInfoPerSetMap _sets = CardInfoPerSetMap(),
                      int _tableRow = 0);
    ~CardInfo() override;

    static CardInfoPtr newInstance(const QString &_name = QString(),
                                   const QString &_text = QString(),
                                   bool _isCrypt = false,
                                   bool _isToken = false,
                                   QVariantHash _properties = QVariantHash(),
                                   CardInfoPerSetMap _sets = CardInfoPerSetMap(),
                                   int _tableRow = 0 );

    void setSmartPointer(CardInfoPtr _ptr)
    {
        smartThis = std::move(_ptr);
    }

    // basic properties
    inline const QString &getName() const
    {
        return name;
    }
    const QString &getSimpleName() const
    {
        return simpleName;
    }
    const QString &getPixmapCacheKey() const
    {
        return pixmapCacheKey;
    }

    const QString &getText() const
    {
        return text;
    }
    void setText(const QString &_text)
    {
        text = _text;
        emit cardInfoChanged(smartThis);
    }

    bool getIsCrypt() const
    {
        return isCrypt;
    }

    bool getIsToken() const
    {
        return isToken;
    }

    QString getGroup() const
    {
        return getProperty(VTES::Group);
    }

    QString getId() const
    {
        return getProperty(VTES::Id);
    }

    const QStringList getProperties() const
    {
        return properties.keys();
    }
    const QString getProperty(const QString &propertyName) const
    {
        return properties.value(propertyName).toString();
    }
    void setProperty(const QString &_name, const QString &_value)
    {
        properties.insert(_name, _value);
        emit cardInfoChanged(smartThis);
    }
    const QStringList getPropertyList(const QString &propertyName) const
    {
        return properties.value(propertyName).toStringList();
    }
    void setPropertyList(const QString &_name, const QStringList &_values)
    {
        properties.insert(_name, _values);
        emit cardInfoChanged(smartThis);
    }


    bool hasProperty(const QString &propertyName) const
    {
        return properties.contains(propertyName);
    }
    const CardInfoPerSetMap &getSets() const
    {
        return sets;
    }
    const QString &getSetsNames() const
    {
        return setsNames;
    }
    const QString getSetProperty(const QString &setName, const QString &propertyName) const
    {
        if (!sets.contains(setName))
            return "";
        return sets[setName].getProperty(propertyName);
    }
    void setSetProperty(const QString &setName, const QString &_name, const QString &_value)
    {
        if (!sets.contains(setName))
            return;

        sets[setName].setProperty(_name, _value);
        emit cardInfoChanged(smartThis);
    }

    // positioning
    int getTableRow() const
    {
        return tableRow;
    }
    void setTableRow(int _tableRow)
    {
        tableRow = _tableRow;
    }

    // Back-compatibility methods. Remove ASAP
    const QStringList getCardTypes() const;
    void setCardTypes(const QStringList &values);
    const QStringList getDisciplines() const;
    const QString getCapacity() const;
    const QString getPool() const;
    const QString getBlood() const;
    const QString getMainCardType() const;
    const QStringList getClans() const;


    // methods using per-set properties
    QString getPicURL(const QString &set) const
    {
        //return "https://static.krcg.org/card/ubendeg4.jpg";
        if (set == nullptr) {
            return getProperty(VTES::PicUrl);
        }
        // Todo; this should be deadcode.
        qDebug() << "This should never be shown";
        return getSetProperty(set, VTES::PicUrl);
    }
    QString getCorrectedName() const;
    void addToSet(const CardSetPtr &_set, CardInfoPerSet _info = CardInfoPerSet());
    void emitPixmapUpdated()
    {
        emit pixmapUpdated();
    }
    void refreshCachedSetNames();

    /**
     * Simplify a name to have no punctuation and lowercase all letters, for
     * less strict name-matching.
     */
    static QString simplifyName(const QString &name);

signals:
    void pixmapUpdated();
    void cardInfoChanged(CardInfoPtr card);
};

enum LoadStatus
{
    Ok,
    VersionTooOld,
    Invalid,
    NotLoaded,
    FileError,
    NoCards
};

typedef QHash<QString, CardInfoPtr> CardNameMap;
typedef QHash<QString, CardSetPtr> SetNameMap;

class CardDatabase : public QObject
{
    Q_OBJECT
protected:
    /*
     * The cards, indexed by name.
     */
    CardNameMap cards;

    /**
     * The cards, indexed by their simple name.
     */
    CardNameMap simpleNameCards;

    /*
     * The sets, indexed by short name.
     */
    SetNameMap sets;

    LoadStatus loadStatus;

    QVector<ICardDatabaseParser *> availableParsers;

private:
    CardInfoPtr getCardFromMap(const CardNameMap &cardMap, const QString &cardName) const;
    void checkUnknownSets();
    void refreshCachedReverseRelatedCards();

    QBasicMutex *reloadDatabaseMutex = new QBasicMutex(), *clearDatabaseMutex = new QBasicMutex(),
                *loadFromFileMutex = new QBasicMutex(), *addCardMutex = new QBasicMutex(),
                *removeCardMutex = new QBasicMutex();

public:
    static const char *TOKENS_SETNAME;

    explicit CardDatabase(QObject *parent = nullptr);
    ~CardDatabase() override;
    void clear();
    void removeCard(CardInfoPtr card);
    CardInfoPtr getCard(const QString &cardName) const;
    QList<CardInfoPtr> getCards(const QStringList &cardNames) const;
    CardInfoPtr guessCard(const QString &cardName) const;

    /*
     * Get a card by its simple name. The name will be simplified in this
     * function, so you don't need to simplify it beforehand.
     */
    CardInfoPtr getCardBySimpleName(const QString &cardName) const;

    CardSetPtr getSet(const QString &setName);
    QList<CardInfoPtr> getCardList() const
    {
        return cards.values();
    }
    SetList getSetList() const;
    LoadStatus loadFromFile(const QString &fileName);
    bool saveCustomTokensToFile();
    QStringList getAllMainCardTypes() const;
    LoadStatus getLoadStatus() const
    {
        return loadStatus;
    }
    void enableAllUnknownSets();
    void markAllSetsAsKnown();
    void notifyEnabledSetsChanged();

public slots:
    LoadStatus loadCardDatabases();
    void addCard(CardInfoPtr card);
    void addSet(CardSetPtr set);
protected slots:
    LoadStatus loadCardDatabase(const QString &path);
signals:
    void cardDatabaseLoadingFailed();
    void cardDatabaseNewSetsFound(int numUnknownSets, QStringList unknownSetsNames);
    void cardDatabaseAllNewSetsEnabled();
    void cardDatabaseEnabledSetsChanged();
    void cardAdded(CardInfoPtr card);
    void cardRemoved(CardInfoPtr card);
};

/*class CardRelation : public QObject
{
    Q_OBJECT
private:
    QString name;
    bool doesAttach;
    bool isCreateAllExclusion;
    bool isVariableCount;
    int defaultCount;
    bool isPersistent;

public:
    explicit CardRelation(const QString &_name = QString(),
                          bool _doesAttach = false,
                          bool _isCreateAllExclusion = false,
                          bool _isVariableCount = false,
                          int _defaultCount = 1,
                          bool _isPersistent = false);

    inline const QString &getName() const
    {
        return name;
    }
    bool getDoesAttach() const
    {
        return doesAttach;
    }
    bool getCanCreateAnother() const
    {
        return !doesAttach;
    }
    bool getIsCreateAllExclusion() const
    {
        return isCreateAllExclusion;
    }
    bool getIsVariable() const
    {
        return isVariableCount;
    }
    int getDefaultCount() const
    {
        return defaultCount;
    }
    bool getIsPersistent() const
    {
        return isPersistent;
    }
};
*/

#endif
