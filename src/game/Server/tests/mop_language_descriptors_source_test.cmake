file(READ "${SOURCE_ROOT}/src/game/Object/ObjectMgr.cpp" object_mgr)

foreach(descriptor IN ITEMS
        "LANG_PANDAREN[^\\n]*108127[^\\n]*SKILL_PANDAREN_NEUTRAL"
        "LANG_PANDAREN_ALLI[^\\n]*108130[^\\n]*SKILL_PANDAREN_ALLIANCE"
        "LANG_PANDAREN_HORDE[^\\n]*108131[^\\n]*SKILL_PANDAREN_HORDE"
        "LANG_RIKKITUN[^\\n]*0[^\\n]*0")
    string(REGEX MATCH "${descriptor}" match "${object_mgr}")
    if(NOT match)
        message(FATAL_ERROR "missing MoP language descriptor: ${descriptor}")
    endif()
endforeach()
