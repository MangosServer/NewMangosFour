set(auction_h_path "${SOURCE_ROOT}/src/game/Object/AuctionHouseMgr.h")
set(auction_cpp_path "${SOURCE_ROOT}/src/game/Object/AuctionHouseMgr.cpp")
set(handler_path "${SOURCE_ROOT}/src/game/WorldHandlers/AuctionHouseHandler.cpp")
set(opcode_h_path "${SOURCE_ROOT}/src/game/Server/Opcodes.h")
set(opcode_cpp_path "${SOURCE_ROOT}/src/game/Server/Opcodes.cpp")
set(session_h_path "${SOURCE_ROOT}/src/game/Server/WorldSession.h")
set(session_cpp_path "${SOURCE_ROOT}/src/game/Server/WorldSession.cpp")
set(eluna_path "${SOURCE_ROOT}/src/modules/Eluna/methods/Mangos/PlayerMethods.h")

foreach(path IN ITEMS auction_h_path auction_cpp_path handler_path opcode_h_path
        opcode_cpp_path session_h_path session_cpp_path eluna_path)
    if(NOT EXISTS "${${path}}")
        message(FATAL_ERROR "missing Wave 28 source: ${${path}}")
    endif()
endforeach()

file(READ "${auction_h_path}" auction_h)
file(READ "${auction_cpp_path}" auction_cpp)
file(READ "${handler_path}" handler)
file(READ "${opcode_h_path}" opcode_h)
file(READ "${opcode_cpp_path}" opcode_cpp)
file(READ "${session_h_path}" session_h)
file(READ "${session_cpp_path}" session_cpp)
file(READ "${eluna_path}" eluna)

function(strip_cpp_comments output source)
    string(REGEX REPLACE "/\\*([^*]|\\*[^/])*\\*/" "" text "${source}")
    string(REGEX REPLACE "//[^\r\n]*" "" text "${text}")
    string(REGEX REPLACE "[ \t\r\n]+" " " text "${text}")
    set(${output} "${text}" PARENT_SCOPE)
endfunction()

foreach(source IN ITEMS auction_h auction_cpp handler opcode_h opcode_cpp
        session_h session_cpp eluna)
    strip_cpp_comments(${source} "${${source}}")
endforeach()

foreach(required IN ITEMS
        "CMSG_AUCTION_HELLO = 0x0379"
        "SMSG_AUCTION_HELLO = 0x10A7"
        "SMSG_AUCTION_OWNER_NOTIFICATION = 0x1A8E")
    string(FIND "${opcode_h}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "missing exact Wave 28 opcode: ${required}")
    endif()
endforeach()

foreach(required IN ITEMS
        "DefC(CMSG_AUCTION_HELLO, \"CMSG_AUCTION_HELLO\", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleAuctionHelloOpcode);"
        "DefS(SMSG_AUCTION_HELLO, \"SMSG_AUCTION_HELLO\");"
        "DefS(SMSG_AUCTION_OWNER_NOTIFICATION, \"SMSG_AUCTION_OWNER_NOTIFICATION\");")
    string(FIND "${opcode_cpp}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "missing Wave 28 registration: ${required}")
    endif()
endforeach()

foreach(required IN ITEMS
        "namespace MopAuctionPackets"
        "ReadHelloRequest"
        "BuildHello"
        "BuildExpiredOrRemovedNotification"
        "out.Initialize(SMSG_AUCTION_OWNER_NOTIFICATION, 29)")
    string(FIND "${auction_h}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "auction owner codec is missing ${required}")
    endif()
endforeach()

foreach(required IN ITEMS
        "MopAuctionPackets::ReadHelloRequest(recv_data)"
        "MopAuctionPackets::BuildHello(data"
        "MopAuctionPackets::BuildExpiredOrRemovedNotification(data, notification)"
        "AuctionHouseEntry const* ahEntry = AuctionHouseMgr::GetAuctionHouseEntry(unit); if (!ahEntry) return;"
        "void WorldSession::SendAuctionExpiredNotification(AuctionEntry* auction)"
        "void WorldSession::SendAuctionRemovedNotification(AuctionEntry* auction)")
    string(FIND "${handler}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "auction production integration is missing ${required}")
    endif()
endforeach()

foreach(required IN ITEMS
        "void SendAuctionExpiredNotification(AuctionEntry* auction);"
        "void SendAuctionRemovedNotification(AuctionEntry* auction);")
    string(FIND "${session_h}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "auction session surface is missing ${required}")
    endif()
endforeach()

string(REGEX MATCHALL "SendAuctionExpiredNotification\\(auction\\)" expiry_calls "${auction_cpp}")
list(LENGTH expiry_calls expiry_call_count)
if(NOT expiry_call_count EQUAL 1)
    message(FATAL_ERROR "expired notification must be emitted only by the expiry path; found ${expiry_call_count} calls")
endif()

foreach(required IN ITEMS
        "case SMSG_AUCTION_HELLO:"
        "case SMSG_AUCTION_OWNER_NOTIFICATION:")
    string(FIND "${session_cpp}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "converted auction packet is not allowed through world suppression: ${required}")
    endif()
endforeach()

string(FIND "${eluna}" "#if defined(ELUNA_MANGOS) && ELUNA_EXPANSION == EXP_MISTS player->GetSession()->SendAuctionHello(unit); #else" eluna_route)
if(eluna_route EQUAL -1)
    message(FATAL_ERROR "active Mists Eluna auction menu does not route through WorldSession::SendAuctionHello")
endif()

foreach(forbidden IN ITEMS
        " MSG_AUCTION_HELLO ="
        "SMSG_AUCTION_REMOVED_NOTIFICATION =")
    string(FIND "${opcode_h}" "${forbidden}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR "obsolete Wave 28 opcode remains: ${forbidden}")
    endif()
endforeach()

set(live_sources "${handler} ${auction_cpp} ${session_h}")
foreach(forbidden IN ITEMS
        "WorldPacket data(MSG_AUCTION_HELLO"
        "WorldPacket data(SMSG_AUCTION_REMOVED_NOTIFICATION"
        "WorldPacket data(SMSG_AUCTION_OWNER_NOTIFICATION"
        "SendAuctionOwnerNotification")
    string(FIND "${live_sources}" "${forbidden}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR "legacy auction path remains live: ${forbidden}")
    endif()
endforeach()
