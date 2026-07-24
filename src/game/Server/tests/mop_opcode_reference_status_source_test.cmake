# SPDX-License-Identifier: GPL-3.0-or-later

if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT is required")
endif()

set(opcodes_h "${SOURCE_ROOT}/src/game/Server/Opcodes.h")
set(opcodes_cpp "${SOURCE_ROOT}/src/game/Server/Opcodes.cpp")
set(opcode_register_inc "${SOURCE_ROOT}/src/game/Server/opcode_register.inc")
set(reference_h "${SOURCE_ROOT}/src/game/Server/Opcodes_reference.h")

foreach(required_file IN ITEMS "${opcodes_h}" "${opcodes_cpp}" "${opcode_register_inc}" "${reference_h}")
    if(NOT EXISTS "${required_file}")
        message(FATAL_ERROR "Required source file is missing: ${required_file}")
    endif()
endforeach()

function(opcode_decimal hex_value output_variable)
    math(EXPR decimal_value "${hex_value}")
    set(${output_variable} "${decimal_value}" PARENT_SCOPE)
endfunction()

function(append_unique list_variable value)
    set(values "${${list_variable}}")
    list(FIND values "${value}" value_index)
    if(value_index EQUAL -1)
        list(APPEND values "${value}")
    endif()
    set(${list_variable} "${values}" PARENT_SCOPE)
endfunction()

# Opcodes.h is the source of enum presence. Generic MSG_ values can occupy either
# wire direction; the reference section and DefC/DefS call disambiguate their use.
file(STRINGS "${opcodes_h}" opcode_lines)
set(enum_records)
set(enum_keys)
foreach(line IN LISTS opcode_lines)
    if(line MATCHES "^[ \t]*((CMSG|SMSG|MSG)_[A-Za-z0-9_]+)[ \t]*=[ \t]*(0x[0-9A-Fa-f]+)")
        set(opcode_name "${CMAKE_MATCH_1}")
        set(opcode_prefix "${CMAKE_MATCH_2}")
        opcode_decimal("${CMAKE_MATCH_3}" opcode_value)
        list(APPEND enum_records "${opcode_name}|${opcode_value}")

        if(opcode_prefix STREQUAL "CMSG" OR opcode_prefix STREQUAL "MSG")
            append_unique(enum_keys "C|${opcode_value}")
        endif()
        if(opcode_prefix STREQUAL "SMSG" OR opcode_prefix STREQUAL "MSG")
            append_unique(enum_keys "S|${opcode_value}")
        endif()
    endif()
endforeach()

if(NOT enum_records)
    message(FATAL_ERROR "No opcode enum constants were parsed from ${opcodes_h}")
endif()

# Parse only anchored, single-line production registrations. This deliberately
# rejects raw numeric bindings and cannot promote a commented-out DefC/DefS call.
file(STRINGS "${opcodes_cpp}" registration_call_lines
    REGEX "^[ \t]*Def[CS]\\(")
file(STRINGS "${opcode_register_inc}" registration_inc_call_lines
    REGEX "^[ \t]*Def[CS]\\(")
list(APPEND registration_call_lines ${registration_inc_call_lines})
set(registration_records)
foreach(registration_line IN LISTS registration_call_lines)
    if(registration_line STREQUAL "")
        continue()
    endif()
    if(NOT registration_line MATCHES
            "^[ \t]*Def([CS])\\([ \t]*((CMSG|SMSG|MSG)_[A-Za-z0-9_]+)[ \t]*,")
        message(FATAL_ERROR
            "DefC/DefS registrations must use a symbolic Opcodes.h constant: ${registration_line}")
    endif()
    list(APPEND registration_records "${CMAKE_MATCH_1}|${CMAKE_MATCH_2}")
endforeach()

set(active_keys)
set(active_binding_records)
foreach(registration_record IN LISTS registration_records)
    if(NOT registration_record MATCHES "^([CS])\\|(.+)$")
        message(FATAL_ERROR "Could not parse registration: ${registration_record}")
    endif()
    set(direction "${CMAKE_MATCH_1}")
    set(binding_name "${CMAKE_MATCH_2}")

    set(binding_value "")
    foreach(enum_record IN LISTS enum_records)
        if(enum_record MATCHES "^${binding_name}\\|([0-9]+)$")
            set(binding_value "${CMAKE_MATCH_1}")
            break()
        endif()
    endforeach()
    if(binding_value STREQUAL "")
        message(FATAL_ERROR "Registered opcode ${binding_name} has no hex enum value in Opcodes.h")
    endif()

    append_unique(active_keys "${direction}|${binding_value}")
    append_unique(active_binding_records "${direction}|${binding_value}|${binding_name}")
endforeach()

if(NOT active_binding_records)
    message(FATAL_ERROR "No DefC/DefS opcode registrations were parsed")
endif()

file(STRINGS "${reference_h}" reference_lines)
set(section "")
set(reference_keys)
set(status_errors)
set(binding_errors)
set(actual_summary_lines)
set(expected_total_active 0)
set(expected_total_doc 0)
set(expected_total_dormant 0)
set(expected_s_active 0)
set(expected_s_doc 0)
set(expected_s_dormant 0)
set(expected_c_active 0)
set(expected_c_doc 0)
set(expected_c_dormant 0)

foreach(line IN LISTS reference_lines)
    if(line MATCHES "==== SMSG")
        set(section "S")
    elseif(line MATCHES "==== CMSG")
        set(section "C")
    endif()

    if(line MATCHES "STATUS TOTALS:|^[ \t]*\\*[ \t]+SMSG: ACTIVE=|^[ \t]*\\*[ \t]+CMSG: ACTIVE=")
        list(APPEND actual_summary_lines "${line}")
    endif()

    if(NOT line MATCHES "^[ \t]*\\*[ \t]+((CMSG|SMSG|MSG)_[A-Za-z0-9_]+)[ \t]+(0x[0-9A-Fa-f]+)[ \t]+(ACTIVE|DOC|DORMANT)(.*)$")
        continue()
    endif()
    if(section STREQUAL "")
        message(FATAL_ERROR "Reference opcode row appears before an SMSG/CMSG section: ${line}")
    endif()

    set(reference_name "${CMAKE_MATCH_1}")
    set(reference_status "${CMAKE_MATCH_4}")
    set(reference_note "${CMAKE_MATCH_5}")
    opcode_decimal("${CMAKE_MATCH_3}" reference_value)
    set(reference_key "${section}|${reference_value}")
    append_unique(reference_keys "${reference_key}")

    list(FIND active_keys "${reference_key}" active_index)
    list(FIND enum_keys "${reference_key}" enum_index)
    if(NOT active_index EQUAL -1)
        set(expected_status "ACTIVE")
    elseif(NOT enum_index EQUAL -1)
        set(expected_status "DORMANT")
    else()
        set(expected_status "DOC")
    endif()

    string(TOLOWER "${section}" section_lower)
    if(expected_status STREQUAL "ACTIVE")
        math(EXPR expected_total_active "${expected_total_active} + 1")
        math(EXPR expected_${section_lower}_active "${expected_${section_lower}_active} + 1")
    elseif(expected_status STREQUAL "DOC")
        math(EXPR expected_total_doc "${expected_total_doc} + 1")
        math(EXPR expected_${section_lower}_doc "${expected_${section_lower}_doc} + 1")
    else()
        math(EXPR expected_total_dormant "${expected_total_dormant} + 1")
        math(EXPR expected_${section_lower}_dormant "${expected_${section_lower}_dormant} + 1")
    endif()

    if(NOT reference_status STREQUAL expected_status)
        list(APPEND status_errors
            "${section}MSG value ${CMAKE_MATCH_3}: ${reference_name} is ${reference_status}, expected ${expected_status}")
    endif()

    if(expected_status STREQUAL "ACTIVE")
        set(key_bindings)
        foreach(binding_record IN LISTS active_binding_records)
            if(binding_record MATCHES "^${section}\\|${reference_value}\\|(.+)$")
                list(APPEND key_bindings "${CMAKE_MATCH_1}")
            endif()
        endforeach()
        list(REMOVE_DUPLICATES key_bindings)
        list(SORT key_bindings)
        list(JOIN key_bindings "," expected_bindings)
        list(FIND key_bindings "${reference_name}" matching_label_index)
        if(matching_label_index EQUAL -1
                AND NOT reference_note MATCHES "(^|[ \t])server-binding=${expected_bindings}([ \t]|$)")
            list(APPEND binding_errors
                "${section}MSG value ${CMAKE_MATCH_3}: ${reference_name} requires server-binding=${expected_bindings}")
        endif()
    endif()
endforeach()

if(NOT reference_keys)
    message(FATAL_ERROR "No opcode rows were parsed from ${reference_h}")
endif()

# Any new registered slot missing from the client reference requires a conscious
# decision. CMSG_TUTORIAL_FLAG is the one reviewed direct-writer exception.
set(missing_reference_bindings)
foreach(binding_record IN LISTS active_binding_records)
    if(binding_record MATCHES "^([CS])\\|([0-9]+)\\|(.+)$")
        set(binding_key "${CMAKE_MATCH_1}|${CMAKE_MATCH_2}")
        set(binding_name "${CMAKE_MATCH_3}")
        list(FIND reference_keys "${binding_key}" reference_index)
        if(reference_index EQUAL -1)
            append_unique(missing_reference_bindings "${binding_name}")
        endif()
    endif()
endforeach()
list(SORT missing_reference_bindings)
set(expected_missing_reference_bindings "CMSG_TUTORIAL_FLAG")
if(NOT missing_reference_bindings STREQUAL expected_missing_reference_bindings)
    message(FATAL_ERROR
        "Registered bindings absent from Opcodes_reference.h changed.\n"
        "Expected: ${expected_missing_reference_bindings}\n"
        "Actual:   ${missing_reference_bindings}")
endif()

# Pin the three generic-MSG edge cases which motivated direction-plus-value
# matching instead of a name-only status overlay.
set(edge_case_expectations
    "C|498|MSG_MOVE_HEARTBEAT|ACTIVE"
    "C|4307|MSG_MOVE_SET_SWIM_SPEED_CHEAT|ACTIVE"
    "S|4312|MSG_MOVE_SET_TURN_RATE_CHEAT|DORMANT")
foreach(expectation IN LISTS edge_case_expectations)
    string(REPLACE "|" ";" fields "${expectation}")
    list(GET fields 0 edge_direction)
    list(GET fields 1 edge_value)
    list(GET fields 2 edge_name)
    list(GET fields 3 edge_status)
    set(edge_found FALSE)
    set(edge_section "")
    foreach(line IN LISTS reference_lines)
        if(line MATCHES "==== SMSG")
            set(edge_section "S")
        elseif(line MATCHES "==== CMSG")
            set(edge_section "C")
        endif()
        if(edge_section STREQUAL edge_direction
                AND line MATCHES "^[ \t]*\\*[ \t]+${edge_name}[ \t]+(0x[0-9A-Fa-f]+)[ \t]+${edge_status}([ \t]|$)")
            opcode_decimal("${CMAKE_MATCH_1}" parsed_edge_value)
            if(parsed_edge_value EQUAL edge_value)
                set(edge_found TRUE)
                break()
            endif()
        endif()
    endforeach()
    if(NOT edge_found)
        message(FATAL_ERROR
            "Reference regression: ${edge_name} must be ${edge_status} in the ${edge_direction}MSG section")
    endif()
endforeach()

set(expected_summary_lines
    " * STATUS TOTALS: ACTIVE=${expected_total_active}, DOC=${expected_total_doc}, DORMANT=${expected_total_dormant}"
    " *   SMSG: ACTIVE=${expected_s_active}, DOC=${expected_s_doc}, DORMANT=${expected_s_dormant}"
    " *   CMSG: ACTIVE=${expected_c_active}, DOC=${expected_c_doc}, DORMANT=${expected_c_dormant}")
if(NOT actual_summary_lines STREQUAL expected_summary_lines)
    message(FATAL_ERROR
        "Opcode reference summary is stale.\nExpected:\n${expected_summary_lines}\nActual:\n${actual_summary_lines}")
endif()

list(LENGTH status_errors status_error_count)
list(LENGTH binding_errors binding_error_count)
if(status_error_count GREATER 0 OR binding_error_count GREATER 0)
    if(status_error_count GREATER 0)
        list(SUBLIST status_errors 0 12 status_error_sample)
    else()
        set(status_error_sample "(none)")
    endif()
    if(binding_error_count GREATER 0)
        list(SUBLIST binding_errors 0 12 binding_error_sample)
    else()
        set(binding_error_sample "(none)")
    endif()
    message(FATAL_ERROR
        "Opcodes_reference.h source-state overlay is stale: "
        "${status_error_count} status mismatches, ${binding_error_count} binding annotations missing.\n"
        "Status sample: ${status_error_sample}\n"
        "Binding sample: ${binding_error_sample}")
endif()

message(STATUS
    "Opcode reference source-state overlay is current: "
    "ACTIVE=${expected_total_active}, DOC=${expected_total_doc}, DORMANT=${expected_total_dormant}")
