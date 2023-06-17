#include "carddatabaseparser.h"

SetNameMap ICardDatabaseParser::sets;

void ICardDatabaseParser::clearSetlist()
{
    sets.clear();
}

CardSetPtr ICardDatabaseParser::internalAddSet(const QString &setName,
                                               const QDate &releaseDate)
{
    if (sets.contains(setName)) {
        return sets.value(setName);
    }

    CardSetPtr newSet = CardSet::newInstance(setName);
    newSet->setReleaseDate(releaseDate);

    sets.insert(setName, newSet);
    emit addSet(newSet);
    return newSet;
}