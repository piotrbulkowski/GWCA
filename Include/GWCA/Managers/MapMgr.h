#pragma once

#include <GWCA/GameContainers/GamePos.h>

#include <GWCA/Utilities/Export.h>
#include <GWCA/GameContainers/Array.h>

namespace GW {

    struct AreaInfo;
    struct PathingMap;
    struct MissionMapIcon;

    typedef Array<PathingMap> PathingMapArray;
    typedef Array<MissionMapIcon> MissionMapIconArray;

    namespace Constants {
        enum class MapID : uint32_t;
        enum class District;
        enum class InstanceType;
        enum class Language;
        enum class ServerRegion;
    }

    enum class RegionType : uint32_t;
    struct MapTypeInstanceInfo {
        uint32_t request_instance_map_type; // Used for auth server
        bool is_outpost;
        RegionType map_region_type;
    };
    struct Module;
    extern Module MapModule;

    namespace Map {
        GWCA_API float QueryAltitude(const GamePos& pos, float radius, float& alt, Vec3f* terrain_normal = nullptr);

        GWCA_API bool GetIsMapLoaded();

        // Get current map ID.
        GWCA_API Constants::MapID GetMapID();

        // Get current region you are in.
        GWCA_API bool GetIsMapUnlocked(Constants::MapID map_id);

        // Get current region you are in.
        GWCA_API GW::Constants::ServerRegion GetRegion();

        // Can be used to get the instance type for auth server request
        GWCA_API MapTypeInstanceInfo* GetMapTypeInstanceInfo(RegionType map_type);

        // Get current language you are in.
        GWCA_API GW::Constants::Language GetLanguage();

        // Get whether current character is observing a match
        GWCA_API bool GetIsObserving();

        // Get the district number you are in.
        GWCA_API int GetDistrict();

        // Get time, in ms, since the instance you are residing in has been created.
        GWCA_API uint32_t GetInstanceTime();

        // Get the instance type (Outpost, Explorable or Loading)
        GWCA_API Constants::InstanceType GetInstanceType();

        // Travel to specified outpost.
        GWCA_API bool Travel(Constants::MapID map_id, GW::Constants::ServerRegion region, int district_number = 0, GW::Constants::Language language = (GW::Constants::Language)0);

        GWCA_API bool Travel(Constants::MapID map_id, Constants::District district = Constants::District::Current, int district_number = 0);

        GWCA_API Constants::ServerRegion RegionFromDistrict(const GW::Constants::District _district);

        GWCA_API Constants::Language LanguageFromDistrict(const GW::Constants::District _district);

        // Returns array of icons (res shrines, quarries, traders, etc) on mission map.
        // Look at MissionMapIcon struct for more info.
        GWCA_API MissionMapIconArray* GetMissionMapIconArray();

        // Returns pointer of collision trapezoid array.
        GWCA_API PathingMapArray* GetPathingMap();

        GWCA_API uint32_t GetFoesKilled();
        GWCA_API uint32_t GetFoesToKill();

        GWCA_API AreaInfo *GetMapInfo(Constants::MapID map_id = (Constants::MapID)0);

        inline AreaInfo *GetCurrentMapInfo() {
            Constants::MapID map_id = GetMapID();
            return GetMapInfo(map_id);
        }

        GWCA_API bool GetIsInCinematic();
        GWCA_API bool SkipCinematic();

        GWCA_API bool EnterChallenge();
        GWCA_API bool CancelEnterChallenge();
    };
}
