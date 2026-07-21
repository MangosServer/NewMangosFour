/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2025 MaNGOS <https://www.getmangos.eu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

/**
 * This is part of the code that takes care of the Auction House and all that can happen with it,
 * it takes care of adding new items to it, bidding/buyouting them etc. Also handles the errors
 * that can happen, ie: you don't have enough money, your account isn't paid for (won't really
 * happen on these servers), the item you are trying to buy doesn't exist etc.
 *
 * This is also what is partly used by the \ref AuctionHouseBot as an interface to what it needs
 * for performing the usual operations such as checking what has been bidded on etc.
 *
 * \todo Add more info about how the auction house system works.
 */


#ifndef MANGOS_H_AUCTION_HOUSE_MGR
#define MANGOS_H_AUCTION_HOUSE_MGR

#include "Common.h"
#include "DBCStructure.h"
#include "Opcodes.h"
#include "World.h"
#include "WorldPacket.h"

/** \addtogroup auctionhouse
 * @{
 * \file
 */

class Item;
class Player;
class Unit;

namespace MopAuctionPackets
{
    struct SoldOrExpired
    {
        int32 randomPropertyId = 0;
        uint32 auctionId = 0;
        uint32 itemEntry = 0;
        uint32 itemNameOverrideId = 0;
        float delay = 0.0f;
        uint64 amount = 0;
        bool sold = false;
    };

    struct Won
    {
        uint64 contextGuid = 0;
        uint32 itemEntry = 0;
        uint32 context = 0;
        int32 randomPropertyId = 0;
        uint32 itemNameOverrideId = 0;
        uint32 auctionId = 0;
    };

    struct Outbid
    {
        uint32 auctionId = 0;
        uint32 auctionHouseId = 0;
        uint32 itemNameOverrideId = 0;
        uint64 bid = 0;
        uint32 itemEntry = 0;
        int32 randomPropertyId = 0;
        uint64 minimumIncrement = 0;
        uint64 bidderGuid = 0;
    };

    struct BidUpdate
    {
        uint64 bid = 0;
        uint32 context52 = 0;
        uint32 context48 = 0;
        uint32 context44 = 0;
        uint32 auctionId = 0;
        uint64 minimumIncrement = 0;
        uint64 bidderGuid = 0;
    };

    inline uint8 GuidByte(uint64 guid, uint8 index)
    {
        return uint8(guid >> (index * 8));
    }

    inline uint64 ReadHelloRequest(WorldPacket& in)
    {
        uint8 const maskOrder[] = { 1, 5, 2, 0, 3, 6, 4, 7 };
        uint8 const byteOrder[] = { 2, 7, 1, 3, 5, 0, 4, 6 };
        uint8 guidBytes[8] = {};

        for (uint8 index : maskOrder)
            guidBytes[index] = in.ReadBit();
        for (uint8 index : byteOrder)
            in.ReadByteSeq(guidBytes[index]);

        uint64 guid = 0;
        for (uint8 index = 0; index < 8; ++index)
            guid |= uint64(guidBytes[index]) << (index * 8);
        return guid;
    }

    inline void BuildHello(WorldPacket& out, uint64 auctioneerGuid,
        uint32 auctionHouseId, bool enabled)
    {
        out.Initialize(SMSG_AUCTION_HELLO, 15);
        for (uint8 index : { uint8(6), uint8(7), uint8(3) })
            out.WriteBit(GuidByte(auctioneerGuid, index) != 0);
        out.WriteBit(enabled);
        for (uint8 index : { uint8(4), uint8(2), uint8(5), uint8(0), uint8(1) })
            out.WriteBit(GuidByte(auctioneerGuid, index) != 0);
        out.FlushBits();

        out.WriteByteSeq(GuidByte(auctioneerGuid, 3));
        out << auctionHouseId;
        for (uint8 index : { uint8(4), uint8(2), uint8(7), uint8(1),
                uint8(0), uint8(6), uint8(5) })
            out.WriteByteSeq(GuidByte(auctioneerGuid, index));
    }

    inline void BuildSoldOrExpiredNotification(WorldPacket& out,
        SoldOrExpired const& notification)
    {
        out.Initialize(SMSG_AUCTION_OWNER_NOTIFICATION, 29);
        out << notification.randomPropertyId;
        out << notification.auctionId;
        out << notification.itemEntry;
        out << notification.itemNameOverrideId;
        out << notification.delay;
        out << notification.amount;
        out.WriteBit(notification.sold);
        out.FlushBits();
    }

    inline void BuildWonNotification(WorldPacket& out, Won const& notification)
    {
        uint8 const maskOrder[] = { 5, 4, 7, 6, 0, 1, 2, 3 };
        out.Initialize(SMSG_AUCTION_WON_NOTIFICATION, 29);
        for (uint8 index : maskOrder)
            out.WriteBit(GuidByte(notification.contextGuid, index) != 0);
        out.FlushBits();

        out.WriteByteSeq(GuidByte(notification.contextGuid, 7));
        out.WriteByteSeq(GuidByte(notification.contextGuid, 3));
        out << notification.itemEntry;
        out.WriteByteSeq(GuidByte(notification.contextGuid, 1));
        out.WriteByteSeq(GuidByte(notification.contextGuid, 2));
        out << notification.context;
        out.WriteByteSeq(GuidByte(notification.contextGuid, 0));
        out << notification.randomPropertyId;
        out.WriteByteSeq(GuidByte(notification.contextGuid, 5));
        out.WriteByteSeq(GuidByte(notification.contextGuid, 4));
        out.WriteByteSeq(GuidByte(notification.contextGuid, 6));
        out << notification.itemNameOverrideId;
        out << notification.auctionId;
    }

    inline void BuildOutbidNotification(WorldPacket& out, Outbid const& notification)
    {
        uint8 const maskOrder[] = { 2, 5, 0, 1, 4, 6, 3, 7 };
        uint8 const byteOrder[] = { 4, 7, 3, 0, 1, 6, 2, 5 };
        out.Initialize(SMSG_AUCTION_OUTBID_NOTIFICATION, 45);
        out << notification.auctionId;
        out << notification.auctionHouseId;
        out << notification.itemNameOverrideId;
        out << notification.bid;
        out << notification.itemEntry;
        out << notification.randomPropertyId;
        out << notification.minimumIncrement;
        for (uint8 index : maskOrder)
            out.WriteBit(GuidByte(notification.bidderGuid, index) != 0);
        out.FlushBits();
        for (uint8 index : byteOrder)
            out.WriteByteSeq(GuidByte(notification.bidderGuid, index));
    }

    inline void BuildBidUpdateNotification(WorldPacket& out,
        BidUpdate const& notification)
    {
        uint8 const maskOrder[] = { 3, 1, 5, 0, 7, 4, 2, 6 };
        uint8 const byteOrder[] = { 5, 1, 0, 7, 2, 6, 3, 4 };
        out.Initialize(SMSG_AUCTION_BID_UPDATE_NOTIFICATION, 41);
        out << notification.bid;
        out << notification.context52;
        out << notification.context48;
        out << notification.context44;
        out << notification.auctionId;
        out << notification.minimumIncrement;
        for (uint8 index : maskOrder)
            out.WriteBit(GuidByte(notification.bidderGuid, index) != 0);
        out.FlushBits();
        for (uint8 index : byteOrder)
            out.WriteByteSeq(GuidByte(notification.bidderGuid, index));
    }
}

#define MIN_AUCTION_TIME (12*HOUR)
#define MAX_AUCTION_SORT 12
#define AUCTION_SORT_REVERSED 0x10

/**
 * Documentation for this taken directly from comments in source
 * \todo Needs real documentation of what these values mean and where they are sent etc.
 */
enum AuctionError
{
    AUCTION_OK                          = 0,                ///< depends on enum AuctionAction
    AUCTION_ERR_INVENTORY               = 1,                ///< depends on enum InventoryChangeResult
    AUCTION_ERR_DATABASE                = 2,                ///< ERR_AUCTION_DATABASE_ERROR (default)
    AUCTION_ERR_NOT_ENOUGH_MONEY        = 3,                ///< ERR_NOT_ENOUGH_MONEY
    AUCTION_ERR_ITEM_NOT_FOUND          = 4,                ///< ERR_ITEM_NOT_FOUND
    AUCTION_ERR_HIGHER_BID              = 5,                ///< ERR_AUCTION_HIGHER_BID
    AUCTION_ERR_BID_INCREMENT           = 7,                ///< ERR_AUCTION_BID_INCREMENT
    AUCTION_ERR_BID_OWN                 = 10,               ///< ERR_AUCTION_BID_OWN
    AUCTION_ERR_RESTRICTED_ACCOUNT      = 13                ///< ERR_RESTRICTED_ACCOUNT
};

enum AuctionAction
{
    AUCTION_STARTED     = 0,                                ///< ERR_AUCTION_STARTED
    AUCTION_REMOVED     = 1,                                ///< ERR_AUCTION_REMOVED
    AUCTION_BID_PLACED  = 2                                 ///< ERR_AUCTION_BID_PLACED
};

struct AuctionEntry
{
    uint32 Id;
    uint32 itemGuidLow;                                     // can be 0 after send won mail with item
    uint32 itemTemplate;
    uint32 itemCount;
    int32 itemRandomPropertyId;
    uint32 owner;                                           // player low guid, can be 0 for server generated auction
    std::wstring ownerName;                                 // cache name for sorting
    uint64 startbid;                                        // start minimal bid value
    uint64 bid;                                             // current bid, =0 meaning no bids
    uint64 buyout;
    time_t expireTime;
    time_t moneyDeliveryTime;
    uint32 bidder;                                          // current bidder player lowguid, can be 0 if bid generated by server, use 'bid'!=0 for check bid existance
    uint64 deposit;                                         // deposit can be calculated only when creating auction
    AuctionHouseEntry const* auctionHouseEntry;             // in AuctionHouse.dbc

    // helpers
    uint32 GetHouseId() const { return auctionHouseEntry->ID; }
    uint32 GetHouseFaction() const { return auctionHouseEntry->FactionID; }
    uint64 GetAuctionCut() const;
    uint64 GetAuctionOutBid() const;
    bool BuildAuctionInfo(WorldPacket& data) const;
    void DeleteFromDB() const;
    void SaveToDB() const;
    void AuctionBidWinning(Player* bidder = NULL);

    // -1,0,+1 order result
    int CompareAuctionEntry(uint32 column, const AuctionEntry* auc, Player* viewPlayer) const;

    bool UpdateBid(uint64 newbid, Player* newbidder = NULL);// true if normal bid, false if buyout, bidder==NULL for generated bid
};

// this class is used as auctionhouse instance
class AuctionHouseObject
{
    public:
        AuctionHouseObject() {}
        ~AuctionHouseObject()
        {
            for (AuctionEntryMap::const_iterator itr = AuctionsMap.begin(); itr != AuctionsMap.end(); ++itr)
            {
                delete itr->second;
            }
        }

        typedef std::map<uint32, AuctionEntry*> AuctionEntryMap;
        typedef std::pair<AuctionEntryMap::const_iterator, AuctionEntryMap::const_iterator> AuctionEntryMapBounds;

        uint32 GetCount() { return AuctionsMap.size(); }

        AuctionEntryMap const& GetAuctions() const { return AuctionsMap; }
        AuctionEntryMapBounds GetAuctionsBounds() const {return AuctionEntryMapBounds(AuctionsMap.begin(), AuctionsMap.end()); }

        void AddAuction(AuctionEntry* ah)
        {
            MANGOS_ASSERT(ah);
            AuctionsMap[ah->Id] = ah;
        }

        AuctionEntry* GetAuction(uint32 id) const
        {
            AuctionEntryMap::const_iterator itr = AuctionsMap.find(id);
            return itr != AuctionsMap.end() ? itr->second : NULL;
        }

        bool RemoveAuction(uint32 id)
        {
            return AuctionsMap.erase(id);
        }

        void Update();

        void BuildListBidderItems(WorldPacket& data, Player* player, uint32& count, uint32& totalcount);
        void BuildListOwnerItems(WorldPacket& data, Player* player, uint32& count, uint32& totalcount);
        void BuildListPendingSales(WorldPacket& data, Player* player, uint32& count);

        AuctionEntry* AddAuction(AuctionHouseEntry const* auctionHouseEntry, Item* newItem, uint32 etime, uint64 bid, uint64 buyout = 0, uint64 deposit = 0, Player* pl = NULL);
    private:
        AuctionEntryMap AuctionsMap;
};

class AuctionSorter
{
    public:
        AuctionSorter(AuctionSorter const& sorter) : m_sort(sorter.m_sort), m_viewPlayer(sorter.m_viewPlayer) {}
        AuctionSorter(uint8* sort, Player* viewPlayer) : m_sort(sort), m_viewPlayer(viewPlayer) {}
        bool operator()(const AuctionEntry* auc1, const AuctionEntry* auc2) const;

    private:
        uint8* m_sort;
        Player* m_viewPlayer;
};

/**
 * This describes the type of auction house that we are dealing with, they can be either:
 * - neutral (anyone can do their shopping there)
 * - alliance/horde (only the respective faction can shop there)
 */
enum AuctionHouseType
{
    AUCTION_HOUSE_ALLIANCE  = 0, ///< Alliance only auction house
    AUCTION_HOUSE_HORDE     = 1, ///< Horde only auction house
    AUCTION_HOUSE_NEUTRAL   = 2  ///< Neutral auction house, anyone can do business here
};

#define MAX_AUCTION_HOUSE_TYPE 3

class AuctionHouseMgr
{
    public:
        AuctionHouseMgr();
        ~AuctionHouseMgr();

        typedef std::unordered_map<uint32, Item*> ItemMap;

        AuctionHouseObject* GetAuctionsMap(AuctionHouseType houseType) { return &mAuctions[houseType]; }
        AuctionHouseObject* GetAuctionsMap(AuctionHouseEntry const* house);

        Item* GetAItem(uint32 id)
        {
            ItemMap::const_iterator itr = mAitems.find(id);
            if (itr != mAitems.end())
            {
                return itr->second;
            }
            return NULL;
        }

        // auction messages
        void SendAuctionWonMail(AuctionEntry* auction);
        void SendAuctionSuccessfulMail(AuctionEntry* auction);
        void SendAuctionExpiredMail(AuctionEntry* auction);
        static uint64 GetAuctionDeposit(AuctionHouseEntry const* entry, uint32 time, Item* pItem);

        static uint32 GetAuctionHouseTeam(AuctionHouseEntry const* house);
        static AuctionHouseEntry const* GetAuctionHouseEntry(Unit* unit);

    public:
        // load first auction items, because of check if item exists, when loading
        void LoadAuctionItems();
        void LoadAuctions();

        void AddAItem(Item* it);
        bool RemoveAItem(uint32 id);

        void Update();

    private:
        AuctionHouseObject  mAuctions[MAX_AUCTION_HOUSE_TYPE];

        ItemMap             mAitems;
};

/// Convenience define to access the singleton object for the Auction House Manager
#define sAuctionMgr MaNGOS::Singleton<AuctionHouseMgr>::Instance()

/** @} */

#endif
