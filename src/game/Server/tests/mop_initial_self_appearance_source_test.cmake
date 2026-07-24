file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/Map.cpp" map_source)

if(MUTATION STREQUAL "visible_item_feed")
    string(REPLACE
        "MopUpdateObject::ObserverVisibleItemSourceStart + i"
        "MopUpdateObject::SelfInventorySourceStart + i"
        map_source "${map_source}")
elseif(MUTATION STREQUAL "combined_builder")
    string(REPLACE
        "MopUpdateObject::AppendSelfPlayerValuesBlock"
        "MopUpdateObject::AppendSelfInventoryValuesBlock"
        map_source "${map_source}")
elseif(MUTATION STREQUAL "skill_feed")
    string(REPLACE
        "MopUpdateObject::SelfSkillSourceStart + i"
        "MopUpdateObject::SelfInventorySourceStart + i"
        map_source "${map_source}")
endif()

string(FIND "${map_source}" "void Map::SendInitSelf(Player* player)" self_start)
string(FIND "${map_source}" "void Map::SendInitTransports(Player* player)" next_start)
if(self_start EQUAL -1 OR next_start EQUAL -1 OR NOT self_start LESS next_start)
    message(FATAL_ERROR "could not isolate Map::SendInitSelf")
endif()
math(EXPR self_length "${next_start} - ${self_start}")
string(SUBSTRING "${map_source}" ${self_start} ${self_length} self_body)

function(require_once token context)
    string(REGEX MATCHALL "${token}" matches "${self_body}")
    list(LENGTH matches count)
    if(NOT count EQUAL 1)
        message(FATAL_ERROR
            "${context}: expected one active occurrence, found ${count}")
    endif()
endfunction()

require_once(
    "MopUpdateObject::ObserverVisibleItemSourceStart \\+ i"
    "initial self visible-item snapshot")
require_once(
    "MopUpdateObject::SelfInventorySourceStart \\+ i"
    "initial self inventory snapshot")
require_once(
    "MopUpdateObject::SelfSkillSourceStart \\+ i"
    "initial self skill snapshot")
require_once(
    "MopUpdateObject::AppendSelfPlayerValuesBlock"
    "combined initial self VALUES builder")

string(FIND "${self_body}"
    "MopUpdateObject::AppendSelfInventoryValuesBlock" inventory_only)
if(NOT inventory_only EQUAL -1)
    message(FATAL_ERROR
        "initial self snapshot regressed to the inventory-only VALUES builder")
endif()
