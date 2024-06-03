#include "stdafx.h"

#include <GWCA/Utilities/Hooker.h>
#include <GWCA/Utilities/Scanner.h>

#include <GWCA/GameEntities/Item.h>

#include <GWCA/Managers/ItemMgr.h>
#include <GWCA/Managers/Module.h>
#include <GWCA/Managers/TradeMgr.h>
#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Context/GameContext.h>
#include <GWCA/Context/TradeContext.h>

namespace GW {
    Module TradeModule = {
        "TradeModule",  // name
        NULL,           // param
        NULL,           // init_module
        NULL,           // exit_module
        NULL,           // enable_hooks
        NULL,           // disable_hooks
    };

    bool Trade::OpenTradeWindow(uint32_t agent_id) {
        return UI::SendUIMessage(GW::UI::UIMessage::kInitiateTrade, (void*)agent_id);
    }

    bool Trade::AcceptTrade() {
        const auto btn = UI::GetChildFrame(UI::GetFrameByLabel(L"DlgTrade"), 2);
        return btn && UI::ButtonClick(btn);
    }

    bool Trade::CancelTrade() {
        const auto btn = UI::GetChildFrame(UI::GetFrameByLabel(L"DlgTrade"), 1);
        return btn && UI::ButtonClick(btn);
    }

    bool Trade::ChangeOffer() {
        const auto btn = UI::GetChildFrame(UI::GetFrameByLabel(L"DlgTrade"), 0);
        return btn && UI::ButtonClick(btn);
    }

    bool Trade::SubmitOffer(uint32_t) {
        // TODO: Set gold amount
        return ChangeOffer();
    }
    bool Trade::RemoveItem(uint32_t item_id) {
        const auto frame = UI::GetFrameByLabel(L"CartPlayer");
        struct {
            uint32_t h0000 = 0;
            uint32_t h0004 = 9;
            uint32_t h0008 = 6;
            uint32_t h000c;
        } action;
        action.h000c = item_id;
        return frame && IsItemOffered(item_id) && UI::SendFrameUIMessage(frame, (UI::UIMessage)0x2e, &action);
    }

    TradeItem* Trade::IsItemOffered(uint32_t item_id) {
        const auto ctx = GW::GetTradeContext();
        if (!ctx)
            return nullptr;
        auto& items = ctx->player.items;
        for (size_t i = 0; i < items.size();i++) {
            if (items[i].item_id != item_id)
                continue;
            return &items[i];
        }
        return nullptr;
    }
    bool Trade::OfferItem(uint32_t item_id, uint32_t quantity) {
        const auto frame = UI::GetFrameByLabel(L"CartPlayer");
        struct {
            uint32_t h0000 = 0;
            uint32_t h0004 = 2;
            uint32_t h0008 = 6;
            uint32_t* h000c;
        } action;
        uint32_t item_id_and_qty[] = { item_id, quantity };
        action.h000c = item_id_and_qty;

        return frame && !IsItemOffered(item_id) && UI::SendFrameUIMessage(frame, (UI::UIMessage)0x2e, &action);
    }
} // namespace GW
