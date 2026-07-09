// <copyright file="NewUIJewelBank.h" company="MUnique">
// Licensed under the MIT License. See LICENSE file in the project root for full license information.
// </copyright>
//
// Jewel bank window: shows the account-wide bankable items (jewels) and their banked counts,
// fed by the server's ItemBankBalances packet (code 0xD5). Deposit/withdraw are driven through
// the existing /bank chat command; this window only displays balances and issues those commands.

#pragma once

#include "UI/NewUI/NewUIBase.h"
#include "UI/NewUI/Widgets/NewUIButton.h"
#include "UI/NewUI/Widgets/NewUIScrollBar.h"
#include "UI/NewUI/Inventory/NewUIMyInventory.h"
#include "UI/NewUI/Dialogs/NewUIMessageBox.h"
#include "UI/NewUI/Dialogs/NewUICommonMessageBox.h" // CNewUIMessageBoxButton (full-width bevel buttons)
#include "UI/NewUI/Quests/NewUIMyQuestInfoWindow.h" // IMAGE_MYQUEST_LINE (section divider texture)
#include <vector>

namespace SEASON3B
{
    class CNewUIJewelBank : public CNewUIObj
    {
    public:
        // One bankable item as shown in the window; filled from the ItemBankBalances packet.
        struct JewelBankEntry
        {
            BYTE    Group;
            WORD    Number;
            DWORD   Count;
            wchar_t Alias[16];
        };

        enum
        {
            MAX_JEWELBANK_ENTRIES = 16,
            JEWELBANK_WIDTH = 190,
            JEWELBANK_HEIGHT = 340,
            AMOUNT_BUTTON_COUNT = 4, // -1 / -10 / -30 / All
        };

        // Reuse existing shared UI frame textures (same slots as the gatekeeper/inventory windows).
        enum IMAGE_LIST
        {
            IMAGE_JEWELBANK_BACK = CNewUIMessageBoxMng::IMAGE_MSGBOX_BACK,
            IMAGE_JEWELBANK_TOP = CNewUIMyInventory::IMAGE_INVENTORY_BACK_TOP,
            IMAGE_JEWELBANK_LEFT = CNewUIMyInventory::IMAGE_INVENTORY_BACK_LEFT,
            IMAGE_JEWELBANK_RIGHT = CNewUIMyInventory::IMAGE_INVENTORY_BACK_RIGHT,
            IMAGE_JEWELBANK_BOTTOM = CNewUIMyInventory::IMAGE_INVENTORY_BACK_BOTTOM,
            IMAGE_JEWELBANK_EXIT_BTN = CNewUIMyInventory::IMAGE_INVENTORY_EXIT_BTN,
            // Horizontal separator drawn between jewel sections (same texture the quest/trade
            // windows use). Owned/loaded by the quest window, like CNewUITrade aliases it.
            IMAGE_JEWELBANK_LINE = CNewUIMyQuestInfoWindow::IMAGE_MYQUEST_LINE,
        };

        CNewUIJewelBank();
        virtual ~CNewUIJewelBank();

        bool Create(CNewUIManager* pNewUIMng, int x, int y);
        void Release();

        void SetPos(int x, int y);

        // Replaces the shown balances (called by the network handler for packet 0xD5).
        void SetBalances(const std::vector<JewelBankEntry>& entries);

        // If (group, number) matches a configured bankable jewel, deposits all of that jewel from the
        // inventory (Ctrl+right-click shortcut). Returns true if it was a bankable jewel and handled.
        bool TryDepositAllOfItem(int group, int number);

        // Read access to the configured currency list (used by the shop currency selector strip).
        int GetEntryCount() const { return (int)m_Entries.size(); }
        const JewelBankEntry* GetEntry(int i) const
        {
            return (i >= 0 && i < (int)m_Entries.size()) ? &m_Entries[i] : nullptr;
        }

        bool UpdateMouseEvent() override;
        bool UpdateKeyEvent() override;
        bool Update() override;
        bool Render() override;

        float GetLayerDepth() override; // 4.0f

    private:
        void LoadImages();
        void UnloadImages();
        void RenderFrame();
        void RenderDividers();      // thin separator line between jewel sections (2D)
        void RenderRows();          // name + count text (2D)
        void RenderIcons();         // jewel icons (3D pass, mirrors CNewUIGoldBowmanLena)
        void LayoutButtons();       // position the withdraw-button widgets for the visible window
        bool HandleRowButtons();    // per-row -1/-10/-30/All withdraw click (via CNewUIButton)

        // Scroll bookkeeping: with more configured jewels than fit on screen a scrollbar appears
        // and the rows/icons/buttons render only the visible window [m_ScrollOffset, +visibleRows).
        void UpdateScroll();        // shows/hides the scrollbar and refreshes m_ScrollOffset
        int  VisibleRowOf(int entryIndex) const { return entryIndex - m_ScrollOffset; }

        // Sends "/bank <action> <alias> <amount>" then a quiet "/bankdata" to refresh the window.
        void SendBankCommand(const wchar_t* action, const wchar_t* alias, int amount);

        CNewUIManager* m_pNewUIMng;
        POINT m_Pos;
        CNewUIButton m_BtnExit;
        // Per-row withdraw buttons. CNewUIMessageBoxButton (the standard dialog button) draws the
        // FULL button texture width scaled to fit, so both left AND right borders render at any size;
        // CNewUIButton only samples texture[0..renderWidth], which dropped the right border on the
        // narrow buttons (the last "All" button in each row looked border-less).
        CNewUIMessageBoxButton m_BtnAmount[MAX_JEWELBANK_ENTRIES][AMOUNT_BUTTON_COUNT];
        CNewUIScrollBar m_ScrollBar;   // shown only when entries exceed the visible rows
        int  m_ScrollOffset;           // index of the first visible entry
        bool m_bScrollVisible;         // whether the scrollbar is currently shown
        std::vector<JewelBankEntry> m_Entries;
    };
}
