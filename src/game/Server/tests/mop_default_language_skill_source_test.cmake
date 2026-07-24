file(READ "${SOURCE_ROOT}/src/game/Object/PlayerLearn.cpp" player_learn)

if(MUTATION STREQUAL "repair")
    string(REPLACE
        "SetSkill(language.skill_id, 300, 300, GetSkillStep(language.skill_id));"
        "/* removed persisted default-language skill repair */"
        player_learn "${player_learn}")
endif()

string(FIND "${player_learn}" "void Player::learnDefaultSpells()" function_start)
string(FIND "${player_learn}" "void Player::learnQuestRewardedSpells" next_start)
if(function_start EQUAL -1 OR next_start EQUAL -1 OR
        NOT function_start LESS next_start)
    message(FATAL_ERROR "could not isolate Player::learnDefaultSpells")
endif()
math(EXPR function_length "${next_start} - ${function_start}")
string(SUBSTRING "${player_learn}" ${function_start} ${function_length} function_body)

foreach(token IN ITEMS
        "language.spell_id == tspell"
        "HasSpell(tspell)"
        "!HasSkill(language.skill_id)"
        "SetSkill(language.skill_id, 300, 300, GetSkillStep(language.skill_id))")
    string(FIND "${function_body}" "${token}" match)
    if(match EQUAL -1)
        message(FATAL_ERROR "missing persisted default-language repair: ${token}")
    endif()
endforeach()
