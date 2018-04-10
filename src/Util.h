#pragma once

#include "Common.h"
#include "UnitType.h"

class CCBot;
class Unit;

namespace Util
{
    struct IsUnit 
    {
        sc2::UNIT_TYPEID m_type;

        IsUnit(sc2::UNIT_TYPEID type);
        bool operator()(const sc2::Unit * unit, const sc2::ObservationInterface*);
    };

    float GetAttackRangeForTarget(const sc2::Unit * unit, const sc2::Unit * target, CCBot & bot);
    float GetMaxAttackRangeForTargets(const sc2::Unit * unit, const std::vector<const sc2::Unit *> & targets, CCBot & bot);
    float GetAttackDamageForTarget(const sc2::Unit * unit, const sc2::Unit * target, CCBot & bot);
    float GetDpsForTarget(const sc2::Unit * unit, const sc2::Unit * target, CCBot & bot);
    
    std::string     GetStringFromRace(const sc2::Race & race);
    sc2::Race       GetRaceFromString(const std::string & race);
    sc2::Point2D    CalcCenter(const std::vector<const sc2::Unit *> & units);
    float GetNorm(sc2::Point2D vector);
    void            Normalize(sc2::Point2D& point);
    sc2::Point2D    Normalized(const sc2::Point2D& point);
    float           GetDotProduct(const sc2::Point2D& v1, const sc2::Point2D& v2);
    sc2::UnitTypeData GetUnitTypeDataFromUnitTypeId(const sc2::UnitTypeID unitTypeId, CCBot & bot);

    sc2::UnitTypeID GetUnitTypeIDFromName(const std::string & name, CCBot & bot);
    sc2::UpgradeID  GetUpgradeIDFromName(const std::string & name, CCBot & bot);

    sc2::Point2D CalcLinearRegression(const std::vector<const sc2::Unit *> & units);
    sc2::Point2D CalcPerpendicularVector(const sc2::Point2D & vector);
    
    // Kevin-provided helper functions
    void    VisualizeGrids(const sc2::ObservationInterface* obs, sc2::DebugInterface* debug);
    float   TerainHeight(const sc2::GameInfo& info, const sc2::Point2D& point);
    bool    Placement(const sc2::GameInfo& info, const sc2::Point2D& point);
    bool    Pathable(const sc2::GameInfo& info, const sc2::Point2D& point);









    CCRace          GetRaceFromString(const std::string & str);
    CCTilePosition  GetTilePosition(const CCPosition & pos);
    CCPosition      GetPosition(const CCTilePosition & tile);
    std::string     GetStringFromRace(const CCRace & race);
    bool            UnitCanMetaTypeNow(const Unit & unit, const UnitType & type, CCBot & m_bot);
    UnitType        GetTownHall(const CCRace & race, CCBot & bot);
    UnitType        GetRefinery(const CCRace & race, CCBot & bot);
    UnitType        GetSupplyProvider(const CCRace & race, CCBot & bot);
    CCPosition      CalcCenter(const std::vector<Unit> & units);
    bool            IsZerg(const CCRace & race);
    bool            IsProtoss(const CCRace & race);
    bool            IsTerran(const CCRace & race);
    CCPositionType  TileToPosition(float tile);

#ifdef SC2API
    sc2::BuffID     GetBuffFromName(const std::string & name, CCBot & bot);
    sc2::AbilityID  GetAbilityFromName(const std::string & name, CCBot & bot);
    sc2::Point2DI   ConvertWorldToCamera(const sc2::GameInfo& game_info, const sc2::Point2D camera_world, const sc2::Point2D& world);
#endif

    float Dist(const Unit & unit, const CCPosition & p2);
    float Dist(const Unit & unit1, const Unit & unit2);
    float Dist(const CCPosition & p1, const CCPosition & p2);
    CCPositionType DistSq(const CCPosition & p1, const CCPosition & p2);
};
