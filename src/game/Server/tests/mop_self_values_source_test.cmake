file(READ "${SOURCE_ROOT}/src/game/Object/ObjectUpdate.cpp" object_update)

if(MUTATION STREQUAL "inventory_only")
    string(REPLACE "MopUpdateObject::AppendSelfPlayerValuesBlock"
        "MopUpdateObject::AppendSelfInventoryValuesBlock"
        object_update "${object_update}")
elseif(MUTATION STREQUAL "health_feed")
    string(REPLACE "addIfChanged(UNIT_FIELD_HEALTH);"
        "/* removed self health feed */"
        object_update "${object_update}")
elseif(MUTATION STREQUAL "progression_feed")
    string(REPLACE "addIfChanged(PLAYER_XP);"
        "/* removed self XP feed */"
        object_update "${object_update}")
elseif(MUTATION STREQUAL "visible_item_feed")
    string(REPLACE
        "for (uint16 i = MopUpdateObject::ObserverVisibleItemSourceStart;"
        "for (uint16 i = 0; /* removed self visible-item feed */"
        object_update "${object_update}")
endif()

string(FIND "${object_update}"
    "if (target == static_cast<Player const*>(this))" self_start)
string(FIND "${object_update}" "        fields.reserve(50);" observer_start)
if(self_start EQUAL -1 OR observer_start EQUAL -1 OR
        NOT self_start LESS observer_start)
    message(FATAL_ERROR "could not isolate self-player VALUES branch")
endif()
math(EXPR self_length "${observer_start} - ${self_start}")
string(SUBSTRING "${object_update}" ${self_start} ${self_length} self_branch)

function(require_once token context)
    string(REGEX MATCHALL "${token}" matches "${self_branch}")
    list(LENGTH matches count)
    if(NOT count EQUAL 1)
        message(FATAL_ERROR
            "${context}: expected one active occurrence, found ${count}")
    endif()
endfunction()

require_once("MopUpdateObject::AppendSelfPlayerValuesBlock"
    "combined self-player VALUES builder")
require_once("addIfChanged\\(UNIT_FIELD_BYTES_0\\)"
    "packed identity and display-power feed")
require_once("addIfChanged\\(UNIT_FIELD_HEALTH\\)"
    "self health feed")
require_once("UNIT_FIELD_POWER1 \\+ i"
    "five-slot self power feed")
require_once("addIfChanged\\(UNIT_FIELD_MAXHEALTH\\)"
    "self max-health feed")
require_once("UNIT_FIELD_MAXPOWER1 \\+ i"
    "five-slot self max-power feed")
require_once("addIfChanged\\(UNIT_FIELD_LEVEL\\)"
    "self level feed")
require_once("for \\(uint16 i = MopUpdateObject::SelfInventorySourceStart"
    "self inventory feed")
require_once("for \\(uint16 i = MopUpdateObject::ObserverVisibleItemSourceStart"
    "self visible-item feed")
require_once("addIfChanged\\(PLAYER_FIELD_COINAGE\\)"
    "self coinage low-word feed")
require_once("addIfChanged\\(PLAYER_FIELD_COINAGE \\+ 1\\)"
    "self coinage high-word feed")
require_once("addIfChanged\\(PLAYER_XP\\)"
    "self XP feed")
require_once("addIfChanged\\(PLAYER_NEXT_LEVEL_XP\\)"
    "self next-level XP feed")
require_once("data->AddUpdateBlock\\(\\)"
    "single merged update block")

string(FIND "${self_branch}"
    "MopUpdateObject::AppendSelfInventoryValuesBlock" inventory_only)
if(NOT inventory_only EQUAL -1)
    message(FATAL_ERROR
        "self-player branch regressed to the inventory-only VALUES builder")
endif()
