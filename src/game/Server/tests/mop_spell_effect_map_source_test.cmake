# Pins which SpellEffect.dbc row survives into the single-slot effect map.
#
# SpellEffect.dbc is keyed on (SpellID, EffectIndex, DifficultyID). sSpellEffectMap holds
# one row per (SpellID, EffectIndex) -- there is no room for the tier. 134760 rows collapse
# onto 127845 keys, so 6915 rows are dropped, and 3043 keys ship rows at more than one tier
# (2684 distinct spells; 2940 of those keys with genuinely different payloads).
#
# The fill was a plain assignment, so whichever row the loop reached LAST won. Two facts
# make that worse than it sounds, and BOTH are load-bearing for the fix:
#
#   * The loop walks ASCENDING Id via LookupEntry, not raw file order. So "last" meant
#     highest Id, and across the shipped DBC the highest-Id row of a colliding key is
#     overwhelmingly an instance tier. Open-world and normal-dungeon casts of those 2684
#     spells therefore used the heroic / raid / LFR payload.
#   * Iteration order was the ONLY thing that decided the winner. Nothing in the old fill
#     expressed an intent, so any change to the loop silently changed 2684 spells. After the fix
#     the loop still decides 41 keys across 40 spells -- the ones with no base row -- and nothing
#     else, because a base row wins from either direction.
#
# The fix keeps the first row installed (lowest Id) and lets a base-difficulty row jump the
# queue. Those are two separate mechanisms with very different reach, and conflating them
# has already produced one wrong changelog:
#
#   lowest-Id-wins   alters the survivor on 2967 keys   <-- the dominant mechanism
#   base-preferred   fires on 76 keys, changes 16       <-- on 60 the base row is also the
#                                                           highest Id, so the old fill already
#                                                           ended on it and the branch is inert
#
# so the arms below pin them SEPARATELY. An earlier revision led with base-preferred and
# cited spell 130078 "Instability" as its example, which is inert: its rows are 167016/d7,
# 167017/d6, 167018/d5, 167019/d0, so the base row already was the highest Id and the old
# fill landed on it anyway. Working examples: 135146 "Shatter" effect 0 (old Id 182109
# tier 7 -> new 176307 tier 0), 134691 "Impale" effect 0 (old 184928 tier 4 -> new 175542
# tier 0).
#
# This does NOT make the lookup difficulty-aware. That needs the tier threaded through
# GetSpellEffect's 176 call sites and is separate work; the point here is only that the
# choice is deterministic, expressed, and biased toward the tier most casts run at.
#
# Run:
#   cmake -DSOURCE_ROOT=<repo> -P mop_spell_effect_map_source_test.cmake

if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT must be set")
endif()

set(_dbc "${SOURCE_ROOT}/src/game/Server/DBCStores.cpp")
if(NOT EXISTS "${_dbc}")
    message(FATAL_ERROR "missing source: ${_dbc}")
endif()

# Both comment forms are stripped, so no assertion can be satisfied by prose that merely
# names the thing it checks for -- including the prose at the top of this file's subject.
# Takes the NAME of a variable, never the text: a macro substitutes textually, and a C++
# body pasted into CMake source is re-parsed as code.
macro(strip_comments _var)
    string(REGEX REPLACE "/\\*([^*]|\\*+[^*/])*\\*+/" "" ${_var} "${${_var}}")
    string(REGEX REPLACE "//[^\n]*" "" ${_var} "${${_var}}")
endmacro()

file(READ "${_dbc}" _dbc_src)
strip_comments(_dbc_src)

# ---------------------------------------------------------------------------
# Mutation arms. Each verifies it changed the text it targets and exits 0
# otherwise, so a dead arm surfaces as a WILL_FAIL failure rather than a pass.
# ---------------------------------------------------------------------------
set(_m_dbc "${_dbc_src}")
if(DEFINED MUTATION)
    if(MUTATION STREQUAL "last_row_wins")
        # The original behaviour: every later row overwrites, so the highest Id wins. This is
        # the arm that matters -- it is the defect, and it reaches 2967 keys.
        string(REPLACE "            else if (spellEffect->DifficultyID == 0 && slot->DifficultyID != 0)"
                       "            else if (true)" _m_dbc "${_dbc_src}")
    elseif(MUTATION STREQUAL "base_not_preferred")
        # Preference inverted: an instance row displaces an installed base row.
        string(REPLACE "spellEffect->DifficultyID == 0 && slot->DifficultyID != 0"
                       "slot->DifficultyID == 0 && spellEffect->DifficultyID != 0"
                       _m_dbc "${_dbc_src}")
    elseif(MUTATION STREQUAL "base_branch_no_assign")
        # Drop the assignment from the preference branch. The counter still increments, so the
        # startup diagnostic still reports 76 displacements, but no row is actually installed and
        # the map keeps the instance row. Silent: every other assertion here stays satisfied.
        #
        # REGEX, not a literal REPLACE. The line this targets carries a trailing `// base tier jumps
        # the queue`, and comments are stripped BEFORE mutation, so the literal text is not in
        # _dbc_src -- it has been replaced by an invisible run of trailing spaces. The first attempt
        # at this arm matched nothing and was caught by the dead-arm guard below. Anchor on
        # ++spellEffectBasePreferred instead: it is unique, whereas `slot = spellEffect;` at this
        # indentation also appears in the first-install branch.
        string(REGEX REPLACE "slot = spellEffect;[ \t]*\n([ \t]*)\\+\\+spellEffectBasePreferred;"
                             "\\1++spellEffectBasePreferred;" _m_dbc "${_dbc_src}")
    elseif(MUTATION STREQUAL "drop_branch_overwrites")
        # Put an assignment back into the drop branch. That is last-row-wins restored, with both
        # counters intact and the preference branch untouched.
        string(REPLACE "            else\n            {\n                ++spellEffectTierDropped;"
                       "            else\n            {\n                slot = spellEffect;\n                ++spellEffectTierDropped;"
                       _m_dbc "${_dbc_src}")
    elseif(MUTATION STREQUAL "descending_iteration")
        # Lowest-Id-wins is a statement about ITERATION ORDER, not about the rows -- but only
        # narrowly, and the arm must not claim more than that. Wherever a base row exists it wins
        # from either direction, so reversing the walk moves 41 keys across 40 spells: exactly the
        # 41 that ship no base row. (No key ships more than one base row, so base rows are never
        # ambiguous among themselves.) 41 keys is small and still worth pinning, because nothing at
        # the call site would reveal that this loop's direction decides them.
        string(REPLACE "    for(uint32 i = 1; i < sSpellEffectStore.GetNumRows(); ++i)\n    {\n        if (SpellEffectEntry const *spellEffect = sSpellEffectStore.LookupEntry(i))"
                       "    for(uint32 i = sSpellEffectStore.GetNumRows(); i > 0; --i)\n    {\n        if (SpellEffectEntry const *spellEffect = sSpellEffectStore.LookupEntry(i - 1))"
                       _m_dbc "${_dbc_src}")
    elseif(MUTATION STREQUAL "slot_by_value")
        # Bind the slot by value and the writes go nowhere: the map keeps whatever the first
        # row installed and both mechanisms become unreachable. This compiles.
        string(REPLACE "            SpellEffectEntry const*& slot ="
                       "            SpellEffectEntry const* slot =" _m_dbc "${_dbc_src}")
    elseif(MUTATION STREQUAL "diagnostic_undercounts")
        # The diagnostic must report the SUM. Reporting only the else-branch tally hides every
        # row displaced by a preferred base row, and the headline number stops reconciling with
        # 134760 - 127845.
        string(REPLACE "                       spellEffectTierDropped + spellEffectBasePreferred,"
                       "                       spellEffectTierDropped," _m_dbc "${_dbc_src}")
    else()
        message(FATAL_ERROR "unknown MUTATION '${MUTATION}'")
    endif()
    if(_m_dbc STREQUAL "${_dbc_src}")
        message(STATUS "MUTATION '${MUTATION}' changed nothing -- dead arm, exiting 0 so WILL_FAIL reports it")
        return()
    endif()
endif()
set(_dbc_src "${_m_dbc}")

# Whitespace-collapsed view, so an assertion is not hostage to the column a comment used to
# sit in. Derived AFTER mutation from the same text every other assertion reads, so there is
# one pipeline and no before/after skew.
string(REGEX REPLACE "[ \t\r\n]+" " " _dbc_flat "${_dbc_src}")

# The fill region only, so an absence check cannot be satisfied by an unrelated assignment
# elsewhere in a 3000-line file. Anchored on the counter declaration and the diagnostic that
# closes the region; both are pinned as present below, so this extraction cannot silently
# come back empty.
string(REGEX MATCH "uint32 oobSpellEffectIndex = 0;.*if \\(oobSpellEffectIndex\\)" _fill "${_dbc_src}")
if(_fill STREQUAL "")
    message(FATAL_ERROR "could not extract the spell-effect fill region -- anchors moved")
endif()
string(REGEX REPLACE "[ \t\r\n]+" " " _fill_flat "${_fill}")

# ---------------------------------------------------------------------------
# 1. The fill must not be an unconditional assignment.
#
#    This is the defect itself. Checked in the fill region, and as a statement
#    (trailing `;`) so the reference bind below does not satisfy it.
# ---------------------------------------------------------------------------
if(_fill_flat MATCHES "sSpellEffectMap\\[spellEffect->SpellID\\]\\.effects\\[spellEffect->EffectIndex\\] = spellEffect;")
    message(FATAL_ERROR
        "DBCStores.cpp assigns sSpellEffectMap unconditionally. That is last-row-wins: the "
        "highest-Id row of each of the 3043 multi-tier keys survives, which for 2684 spells "
        "means open-world casts read an instance-tier payload.")
endif()

# ---------------------------------------------------------------------------
# 2. The slot must be bound BY REFERENCE.
#
#    Every mechanism below writes through it. By value they all compile and all
#    do nothing.
# ---------------------------------------------------------------------------
if(NOT _fill_flat MATCHES "SpellEffectEntry const\\*& slot = sSpellEffectMap\\[spellEffect->SpellID\\]\\.effects\\[spellEffect->EffectIndex\\];")
    message(FATAL_ERROR
        "the spell-effect slot is not bound as `SpellEffectEntry const*& slot`. Bound by value, "
        "every assignment in the fill is discarded and the map keeps whichever row landed first.")
endif()

# ---------------------------------------------------------------------------
# 3. First-install, then the base preference, then drop -- in that order, and each with the
#    RIGHT BODY.
#
#    `!slot` must be tested first or the preference dereferences a null slot. The bodies matter
#    as much as the conditions and are pinned here as one contiguous pattern: an earlier version
#    stopped at the else-if condition, which let two silent breakages through the baseline while
#    all five mutation arms still failed. Deleting `slot = spellEffect;` from the preference
#    branch left the counter incrementing and the row not installed; adding it to the drop branch
#    restored last-row-wins. Neither was detectable. Hence the whole three-branch body, and the
#    two arms that reproduce exactly those breakages.
# ---------------------------------------------------------------------------
if(NOT _fill_flat MATCHES "if \\(!slot\\) \\{ slot = spellEffect; \\} else if \\(spellEffect->DifficultyID == 0 && slot->DifficultyID != 0\\) \\{ slot = spellEffect; \\+\\+spellEffectBasePreferred; \\} else \\{ \\+\\+spellEffectTierDropped; \\}")
    message(FATAL_ERROR
        "the spell-effect fill is not the exact three-branch form:\n\n"
        "    if (!slot)                { slot = spellEffect; }\n"
        "    else if (base, slot inst) { slot = spellEffect; ++spellEffectBasePreferred; }\n"
        "    else                      { ++spellEffectTierDropped; }\n\n"
        "The null test must come first (the preference dereferences slot); the preference must read "
        "`spellEffect->DifficultyID == 0 && slot->DifficultyID != 0` and must ACTUALLY ASSIGN; and "
        "the drop branch must only count. An assignment in the drop branch is last-row-wins again, "
        "and a missing assignment in the preference branch counts a displacement that never "
        "happened -- both leave every other assertion here satisfied.")
endif()

# ---------------------------------------------------------------------------
# 4. Iteration must stay ASCENDING through LookupEntry.
#
#    Lowest-Id-wins is a property of the walk, not of the rows: this fill states no
#    preference between two instance-tier rows, and 41 keys ship no base row at all. Reverse
#    the loop and 2684 spells change payload with nothing in this file edited. The old fill
#    had the same hidden dependency, which is how a prediction about the startup counter came
#    out backwards -- it was reasoned from raw file order.
# ---------------------------------------------------------------------------
if(NOT _dbc_flat MATCHES "for\\(uint32 i = 1; i < sSpellEffectStore\\.GetNumRows\\(\\); \\+\\+i\\) \\{ if \\(SpellEffectEntry const \\*spellEffect = sSpellEffectStore\\.LookupEntry\\(i\\)\\)")
    message(FATAL_ERROR
        "the SpellEffect.dbc loop is no longer an ascending LookupEntry walk. The surviving row "
        "on every key without a base row is decided by iteration order alone, so this loop is "
        "part of the fix and not incidental.")
endif()

# ---------------------------------------------------------------------------
# 5. Both counters exist and the diagnostic reports their SUM.
#
#    The sum is the only number that reconciles against the DBC: 134760 rows - 127845 keys
#    = 6915. Reporting the else-branch tally alone silently omits the displaced rows.
# ---------------------------------------------------------------------------
foreach(_c "spellEffectTierDropped" "spellEffectBasePreferred")
    if(NOT _dbc_flat MATCHES "uint32 ${_c} = 0;")
        message(FATAL_ERROR "counter ${_c} is not declared; the fill's reach is unreported at startup.")
    endif()
    if(NOT _fill_flat MATCHES "\\+\\+${_c};")
        message(FATAL_ERROR "counter ${_c} is declared but never incremented in the fill region.")
    endif()
endforeach()

if(NOT _dbc_flat MATCHES "spellEffectTierDropped \\+ spellEffectBasePreferred,")
    message(FATAL_ERROR
        "the startup diagnostic does not report spellEffectTierDropped + spellEffectBasePreferred. "
        "Only the sum reconciles with the DBC (134760 rows - 127845 keys = 6915); the else-branch "
        "tally alone omits every row a preferred base row displaced.")
endif()

message(STATUS "mop_spell_effect_map_source_test: all assertions hold")
