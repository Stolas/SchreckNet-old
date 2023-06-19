#ifndef GAME_SPECIFIC_TERMS_H
#define GAME_SPECIFIC_TERMS_H

#include <QCoreApplication>
#include <QString>


namespace VTES
{
QString const CardTypes("types");
QString const Capacity("cap");
QString const PoolCost("pool");
QString const BloodCost("blood");
QString const Group("group");
QString const Clans("clans");
QString const Advanced("advanced");
QString const Disciplines("disciplines");
QString const Sets("sets");

inline static const QString getNicePropertyName(QString key)
{
    if (key == CardTypes)
        return QCoreApplication::translate("VTES", "Card Types");
    if (key == Capacity)
        return QCoreApplication::translate("VTES", "Capacity");
    if (key == PoolCost)
        return QCoreApplication::translate("VTES", "Pool Cost");
    if (key == BloodCost)
        return QCoreApplication::translate("VTES", "Blood Cose");
    if (key == Group)
        return QCoreApplication::translate("VTES", "Group");
    if (key == Clans)
        return QCoreApplication::translate("VTES", "Clans");
    if (key == Advanced)
        return QCoreApplication::translate("VTES", "Advanced");
    if (key == Disciplines)
        return QCoreApplication::translate("VTES", "Disciplines");
    if (key == Sets)
        return QCoreApplication::translate("VTES", "Sets");
    return key;
}
}; // namespace VTES

#endif
