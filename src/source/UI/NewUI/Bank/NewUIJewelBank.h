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
#include "UI/NewUI/Inventory/NewUIMyInventory.h"
#include "UI/NewUI/Dialogs/NewUIMessageBox.h"
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
            JEWELBANK_WIDTH = 200,
            JEWELBANK_HEIGHT = 320,
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
        };

        CNewUIJewelBank();
        virtual ~CNewUIJewelBank();

        bool Create(CNewUIManager* pNewUIMng, int x, int y);
        void Release();

        void SetPos(int x, int y);

        // Replaces the shown balances (called by the network handler for packet 0xD5).
        void SetBalances(const std::vector<JewelBankEntry>& entries);

        bool UpdateMouseEvent() override;
        bool UpdateKeyEvent() override;
        bool Update() override;
        bool Render() override;

        float GetLayerDepth() override; // 4.0f

    private:
        void LoadImages();
        void RenderFrame();
        void RenderBalances();

        CNewUIManager* m_pNewUIMng;
        POINT m_Pos;
        CNewUIButton m_BtnExit;
        std::vector<JewelBankEntry> m_Entries;
    };
}
