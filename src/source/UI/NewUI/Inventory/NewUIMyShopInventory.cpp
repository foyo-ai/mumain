// NewUIMyShopInventory.cpp: implementation of the CNewUIMyShopInventory class.
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "UI/NewUI/Inventory/NewUIMyShopInventory.h"
#include "Audio/DSPlaySound.h"
#include "UI/NewUI/NewUISystem.h"
#include "UI/NewUI/Dialogs/NewUICustomMessageBox.h"
#include "UI/NewUI/Bank/NewUIJewelBank.h"
#include "GameLogic/Items/PersonalShopTitleImp.h"
#include "Engine/Object/ZzzInventory.h"
#include "Network/Server/WSclient.h"
#include "Camera/CameraProjection.h"
#include "I18N/All.h"

const int iMAX_SHOPTITLE_MULTI = 26;

namespace
{
    void RenderText(const wchar_t* text, int x, int y, int sx, int sy, DWORD color, DWORD backcolor, int sort, HFONT hFont = g_hFont)
    {
        g_pRenderText->SetFont(hFont);

        DWORD backuptextcolor = g_pRenderText->GetTextColor();
        DWORD backuptextbackcolor = g_pRenderText->GetBgColor();

        g_pRenderText->SetTextColor(color);
        g_pRenderText->SetBgColor(backcolor);
        g_pRenderText->RenderText(x, y, text, sx, sy, sort);

        g_pRenderText->SetTextColor(backuptextcolor);
        g_pRenderText->SetBgColor(backuptextbackcolor);
    }

    // "Sell in" currency strip, placed under the store-name edit box (the item grid and the
    // warning text below are shifted down by kGridShift to make room).
    constexpr int kGridShift = 34;
    constexpr int kCurStripY = 96;   // slot top
    constexpr int kCurSlotWMax = 30;
    constexpr int kCurSlotH = 26;
    constexpr int kCurSlotGap = 3;
    constexpr int kCurStripLeft = 16;
    constexpr int kCurStripAvail = 190 - 2 * kCurStripLeft; // INVENTORY_WIDTH(190) minus side margins

    // Slot width shrinks so every configured currency fits inside the frame (no overflow past ~3 jewels).
    int CurSlotW(int count)
    {
        if (count < 1) count = 1;
        int w = (kCurStripAvail - (count - 1) * kCurSlotGap) / count;
        if (w > kCurSlotWMax) w = kCurSlotWMax;
        if (w < 12) w = 12;
        return w;
    }
    int CurSlotX(int k, int slotW) { return kCurStripLeft + k * (slotW + kCurSlotGap); }
};

using namespace SEASON3B;

SEASON3B::CNewUIMyShopInventory::CNewUIMyShopInventory() : m_SourceIndex(-1), m_TargetIndex(-1), m_EnablePersonalShop(false)
{
    m_pNewUIMng = NULL;
    m_pNewInventoryCtrl = NULL;
    m_Pos.x = m_Pos.y = 0;
    m_EditBox = NULL;
    m_Button = NULL;
    m_bIsEnableInputValueTextBox = false;
    m_bRequestedCurrency = false;
}

SEASON3B::CNewUIMyShopInventory::~CNewUIMyShopInventory()
{
    Release();
}

bool SEASON3B::CNewUIMyShopInventory::Create(CNewUIManager* pNewUIMng, int x, int y)
{
    if (NULL == pNewUIMng || NULL == g_pNewUI3DRenderMng || NULL == g_pNewItemMng)
        return false;

    m_pNewUIMng = pNewUIMng;
    m_pNewUIMng->AddUIObj(SEASON3B::INTERFACE_MYSHOP_INVENTORY, this);

    SetPos(x, y);

    LoadImages();

    m_pNewInventoryCtrl = new CNewUIInventoryCtrl;
    if (false == m_pNewInventoryCtrl->Create(STORAGE_TYPE::MYSHOP, g_pNewUI3DRenderMng, g_pNewItemMng, this, m_Pos.x + 16, m_Pos.y + 90 + kGridShift, 8, 4, MAX_MY_INVENTORY_EX_INDEX))
    {
        SAFE_DELETE(m_pNewInventoryCtrl);
        return false;
    }

    m_pNewInventoryCtrl->SetToolTipType(TOOLTIP_TYPE_MY_SHOP);

    m_Button = new CNewUIButton[MYSHOPINVENTORY_MAXBUTTONCOUNT];

    m_Button[MYSHOPINVENTORY_EXIT].ChangeButtonImgState(true, IMAGE_MYSHOPINVENTORY_EXIT_BTN, false);
    m_Button[MYSHOPINVENTORY_EXIT].ChangeButtonInfo(m_Pos.x + 13, m_Pos.y + 391, 36, 29);
    m_Button[MYSHOPINVENTORY_EXIT].ChangeToolTipText(&I18N::Game::Close388, true);

    m_Button[MYSHOPINVENTORY_OPEN].ChangeButtonImgState(true, IMAGE_MYSHOPINVENTORY_OPEN, false);
    m_Button[MYSHOPINVENTORY_OPEN].ChangeButtonInfo(m_Pos.x + 53, m_Pos.y + 391, 36, 29);
    m_Button[MYSHOPINVENTORY_OPEN].ChangeToolTipText(&I18N::Game::Open1107, true);

    m_Button[MYSHOPINVENTORY_CLOSE].ChangeButtonImgState(true, IMAGE_MYSHOPINVENTORY_CLOSE, false);
    m_Button[MYSHOPINVENTORY_CLOSE].ChangeButtonInfo(m_Pos.x + 93, m_Pos.y + 391, 36, 29);
    m_Button[MYSHOPINVENTORY_CLOSE].ChangeToolTipText(&I18N::Game::Closed, true);

    m_EditBox = new CUITextInputBox;

    m_EditBox->Init(g_hWnd, 200, 14, iMAX_SHOPTITLE_MULTI - 1);
    m_EditBox->SetPosition(m_Pos.x + 50, m_Pos.y + 55);
    m_EditBox->SetTextColor(255, 255, 230, 210);
    m_EditBox->SetBackColor(0, 0, 0, 25);
    m_EditBox->SetFont(g_hFont);

    ChangeEditBox(UISTATE_NORMAL);
    ChangePersonal(m_EnablePersonalShop);

    Show(false);

    return true;
}

void SEASON3B::CNewUIMyShopInventory::Release()
{
    SAFE_DELETE(m_pNewInventoryCtrl);
    SAFE_DELETE_ARRAY(m_Button);
    SAFE_DELETE(m_EditBox);

    if (m_pNewUIMng)
    {
        m_pNewUIMng->RemoveUIObj(this);
        m_pNewUIMng = NULL;
    }

    UnloadImages();
}

void SEASON3B::CNewUIMyShopInventory::LoadImages()
{
    LoadBitmap(L"Interface\\newui_msgbox_back.jpg", IMAGE_MYSHOPINVENTORY_BACK, GL_LINEAR);
    LoadBitmap(L"Interface\\newui_item_back01.tga", IMAGE_MYSHOPINVENTORY_TOP, GL_LINEAR);
    LoadBitmap(L"Interface\\newui_item_back02-L.tga", IMAGE_MYSHOPINVENTORY_LEFT, GL_LINEAR);
    LoadBitmap(L"Interface\\newui_item_back02-R.tga", IMAGE_MYSHOPINVENTORY_RIGHT, GL_LINEAR);
    LoadBitmap(L"Interface\\newui_item_back03.tga", IMAGE_MYSHOPINVENTORY_BOTTOM, GL_LINEAR);
    LoadBitmap(L"Interface\\newui_exit_00.tga", IMAGE_MYSHOPINVENTORY_EXIT_BTN, GL_LINEAR);
    LoadBitmap(L"Interface\\newui_Box_openTitle.tga", IMAGE_MYSHOPINVENTORY_EDIT, GL_LINEAR);
    LoadBitmap(L"Interface\\newui_Bt_openshop.tga", IMAGE_MYSHOPINVENTORY_OPEN, GL_LINEAR);
    LoadBitmap(L"Interface\\newui_Bt_closeshop.tga", IMAGE_MYSHOPINVENTORY_CLOSE, GL_LINEAR);
}

void SEASON3B::CNewUIMyShopInventory::UnloadImages()
{
    DeleteBitmap(IMAGE_MYSHOPINVENTORY_CLOSE);
    DeleteBitmap(IMAGE_MYSHOPINVENTORY_OPEN);
    DeleteBitmap(IMAGE_MYSHOPINVENTORY_EDIT);
    DeleteBitmap(IMAGE_MYSHOPINVENTORY_EXIT_BTN);
    DeleteBitmap(IMAGE_MYSHOPINVENTORY_BOTTOM);
    DeleteBitmap(IMAGE_MYSHOPINVENTORY_RIGHT);
    DeleteBitmap(IMAGE_MYSHOPINVENTORY_LEFT);
    DeleteBitmap(IMAGE_MYSHOPINVENTORY_TOP);
    DeleteBitmap(IMAGE_MYSHOPINVENTORY_BACK);
}

void SEASON3B::CNewUIMyShopInventory::SetPos(int x, int y)
{
    m_Pos.x = x;
    m_Pos.y = y;

    if (m_pNewInventoryCtrl)
    {
        m_pNewInventoryCtrl->SetPos(m_Pos.x + 16, m_Pos.y + 90 + kGridShift);
    }
    if (m_Button)
    {
        m_Button[MYSHOPINVENTORY_EXIT].SetPos(m_Pos.x + 13, m_Pos.y + 391);
        m_Button[MYSHOPINVENTORY_OPEN].SetPos(m_Pos.x + 53, m_Pos.y + 391);
        m_Button[MYSHOPINVENTORY_CLOSE].SetPos(m_Pos.x + 93, m_Pos.y + 391);
    }
}

void SEASON3B::CNewUIMyShopInventory::GetTitle(wchar_t* titletext)
{
     m_EditBox->GetText(titletext, iMAX_SHOPTITLE_MULTI);
}

void SEASON3B::CNewUIMyShopInventory::SetTitle(wchar_t* titletext)
{
    m_EditBox->SetText(titletext);
}

bool SEASON3B::CNewUIMyShopInventory::InsertItem(int iIndex, std::span<const BYTE> pbyItemPacket)
{
    if (m_pNewInventoryCtrl)
    {
        return m_pNewInventoryCtrl->AddItem(iIndex, pbyItemPacket);
    }

    return false;
}

void SEASON3B::CNewUIMyShopInventory::DeleteItem(int iIndex)
{
    if (m_pNewInventoryCtrl)
    {
        ITEM* pItem = m_pNewInventoryCtrl->FindItem(iIndex);
        if (pItem != NULL)
            m_pNewInventoryCtrl->RemoveItem(pItem);
    }
}

void SEASON3B::CNewUIMyShopInventory::DeleteAllItems()
{
    if (m_pNewInventoryCtrl)
    {
        m_pNewInventoryCtrl->RemoveAllItems();
    }
}

ITEM* SEASON3B::CNewUIMyShopInventory::FindItem(int iLinealPos)
{
    if (m_pNewInventoryCtrl)
        return m_pNewInventoryCtrl->FindItem(iLinealPos);
    return NULL;
}

void SEASON3B::CNewUIMyShopInventory::ChangePersonal(bool state)
{
    m_EnablePersonalShop = state;

    if (m_EnablePersonalShop)
    {
        m_Button[MYSHOPINVENTORY_OPEN].ChangeImgColor(BUTTON_STATE_UP, RGBA(255, 255, 255, 255));
        m_Button[MYSHOPINVENTORY_OPEN].ChangeTextColor(RGBA(255, 255, 255, 255));
        m_Button[MYSHOPINVENTORY_OPEN].UnLock();
        m_Button[MYSHOPINVENTORY_OPEN].ChangeToolTipText(&I18N::Game::Apply, true);
        m_Button[MYSHOPINVENTORY_CLOSE].ChangeImgColor(BUTTON_STATE_UP, RGBA(255, 255, 255, 255));
        m_Button[MYSHOPINVENTORY_CLOSE].ChangeTextColor(RGBA(255, 255, 255, 255));
        m_Button[MYSHOPINVENTORY_CLOSE].UnLock();
    }
    else
    {
        m_Button[MYSHOPINVENTORY_CLOSE].ChangeImgColor(BUTTON_STATE_UP, RGBA(100, 100, 100, 255));
        m_Button[MYSHOPINVENTORY_CLOSE].ChangeTextColor(RGBA(100, 100, 100, 255));
        m_Button[MYSHOPINVENTORY_CLOSE].Lock();
        m_Button[MYSHOPINVENTORY_OPEN].ChangeImgColor(BUTTON_STATE_UP, RGBA(255, 255, 255, 255));
        m_Button[MYSHOPINVENTORY_OPEN].ChangeTextColor(RGBA(255, 255, 255, 255));
        m_Button[MYSHOPINVENTORY_OPEN].UnLock();
        m_Button[MYSHOPINVENTORY_OPEN].ChangeToolTipText(&I18N::Game::Open1107, true);
    }
}

void SEASON3B::CNewUIMyShopInventory::OpenButtonLock()
{
    m_Button[MYSHOPINVENTORY_OPEN].ChangeImgColor(BUTTON_STATE_UP, RGBA(100, 100, 100, 255));
    m_Button[MYSHOPINVENTORY_OPEN].ChangeTextColor(RGBA(100, 100, 100, 255));
    m_Button[MYSHOPINVENTORY_OPEN].Lock();
    m_Button[MYSHOPINVENTORY_OPEN].ChangeToolTipText(&I18N::Game::Open1107, true);
}

void SEASON3B::CNewUIMyShopInventory::OpenButtonUnLock()
{
    m_Button[MYSHOPINVENTORY_OPEN].ChangeImgColor(BUTTON_STATE_UP, RGBA(255, 255, 255, 255));
    m_Button[MYSHOPINVENTORY_OPEN].ChangeTextColor(RGBA(255, 255, 255, 255));
    m_Button[MYSHOPINVENTORY_OPEN].UnLock();
    m_Button[MYSHOPINVENTORY_OPEN].ChangeToolTipText(&I18N::Game::Apply, true);
}

const bool SEASON3B::CNewUIMyShopInventory::IsEnablePersonalShop() const
{
    return m_EnablePersonalShop;
}

void SEASON3B::CNewUIMyShopInventory::ChangeEditBox(const UISTATES type)
{
    m_EditBox->SetState(type);

    if (type == UISTATE_NORMAL)
    {
        m_EditBox->GiveFocus();
    }

}

bool SEASON3B::CNewUIMyShopInventory::UpdateKeyEvent()
{
    if (g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_MYSHOP_INVENTORY) == true)
    {
        if (SEASON3B::IsPress(VK_ESCAPE) == true)
        {
            g_pNewUISystem->Hide(SEASON3B::INTERFACE_MYSHOP_INVENTORY);
            PlayBuffer(SOUND_CLICK01);
            return false;
        }
    }

    return true;
}

bool SEASON3B::CNewUIMyShopInventory::MyShopInventoryProcess()
{
    if (CheckMouseIn(m_Pos.x, m_Pos.y, INVENTORY_WIDTH, INVENTORY_HEIGHT) == false)
    {
        return false;
    }

    CNewUIPickedItem* pPickedItem = CNewUIInventoryCtrl::GetPickedItem();

    if (m_pNewInventoryCtrl && pPickedItem && IsRelease(VK_LBUTTON))
    {
        ITEM* pItemObj = pPickedItem->GetItem();
        int iSourceIndex = pPickedItem->GetSourceLinealPos();
        int iTargetIndex = pPickedItem->GetTargetLinealPos(m_pNewInventoryCtrl);

#ifndef KJH_FIX_CHANGE_ITEM_PRICE_IN_PERSONAL_SHOP				// #ifndef
        if (IsPersonalShopBan(pItemObj))
            m_pNewInventoryCtrl->SetSquareColorNormal(1.0f, 0.0f, 0.0f);
        else
            m_pNewInventoryCtrl->SetSquareColorNormal(0.1f, 0.4f, 0.8f);
#endif // KJH_FIX_CHANGE_ITEM_PRICE_IN_PERSONAL_SHOP

        if (iTargetIndex == -1)
        {
            return true;
        }

        if (pPickedItem->GetOwnerInventory() == g_pMyInventory->GetInventoryCtrl())
        {
            if (IsPersonalShopBan(pItemObj) == true)
            {
                g_pSystemLogBox->AddText(I18N::Game::ThisItemIsNotAllowedToUseThePrivateStore, SEASON3B::TYPE_ERROR_MESSAGE);
                return true;
            }

            if (m_pNewInventoryCtrl->CanMove(iTargetIndex, pItemObj))
            {
                ChangeSourceIndex(iSourceIndex);
                ChangeTargetIndex(iTargetIndex);

                CreateMessageBox(MSGBOX_LAYOUT_CLASS(CPersonalShopItemValueMsgBoxLayout));
                SetInputValueTextBox(true);

                pPickedItem->HidePickedItem();
                return true;
            }
        }
        else if (pPickedItem->GetOwnerInventory() == NULL)
        {
            if (IsPersonalShopBan(pItemObj) == true)
            {
                g_pSystemLogBox->AddText(I18N::Game::ThisItemIsNotAllowedToUseThePrivateStore, SEASON3B::TYPE_ERROR_MESSAGE);
                return true;
            }

            if (m_pNewInventoryCtrl->CanMove(iTargetIndex, pItemObj))
            {
                ChangeSourceIndex(iSourceIndex);
                ChangeTargetIndex(iTargetIndex);

                CreateMessageBox(MSGBOX_LAYOUT_CLASS(CPersonalShopItemValueMsgBoxLayout));
                SetInputValueTextBox(true);

                pPickedItem->HidePickedItem();
                return true;
            }
        }
        else if (pPickedItem->GetOwnerInventory() == m_pNewInventoryCtrl)
        {
            if (m_pNewInventoryCtrl->CanMove(iTargetIndex, pItemObj))
            {
                ChangeSourceIndex(iSourceIndex);
                ChangeTargetIndex(iTargetIndex);
                SendRequestEquipmentItem(STORAGE_TYPE::MYSHOP, iSourceIndex, pItemObj,
                    STORAGE_TYPE::MYSHOP, iTargetIndex);
                return true;
            }
        }
    }
    else if (m_pNewInventoryCtrl && !pPickedItem && IsPress(VK_RBUTTON))
    {
        MouseRButton = false;
        MouseRButtonPop = false;
        MouseRButtonPush = false;

        int iCurSquareIndex = m_pNewInventoryCtrl->GetIndexAtPt(MouseX, MouseY);

        if (iCurSquareIndex != -1)
        {
            ITEM* pItem = g_pMyShopInventory->FindItem(iCurSquareIndex);

            if(pItem)
            {
                ChangeSourceIndex(iCurSquareIndex);
                ChangeTargetIndex(-1);
                CreateMessageBox(MSGBOX_LAYOUT_CLASS(CPersonalShopItemValueMsgBoxLayout));
                SetInputValueTextBox(true);
            }
            return true;
        }
    }

    return false;
}

bool SEASON3B::CNewUIMyShopInventory::UpdateMouseEvent()
{
    if (m_pNewInventoryCtrl && false == m_pNewInventoryCtrl->UpdateMouseEvent())
    {
        return false;
    }

    if (HandleCurrencyStrip())
    {
        return false;
    }

    if (CheckMouseIn(m_Pos.x, m_Pos.y, INVENTORY_WIDTH, INVENTORY_HEIGHT))
    {
        if (MyShopInventoryProcess() == true)
        {
            return false;
        }

        POINT ptExitBtn1 = { m_Pos.x + 169, m_Pos.y + 7 };

        if (SEASON3B::IsPress(VK_LBUTTON) && CheckMouseIn(ptExitBtn1.x, ptExitBtn1.y, 13, 12))
        {
            g_pNewUISystem->Hide(SEASON3B::INTERFACE_MYSHOP_INVENTORY);
            return false;
        }

        if (SEASON3B::IsRelease(VK_LBUTTON)
            && CheckMouseIn(m_EditBox->GetPosition_x(), m_EditBox->GetPosition_y(), m_EditBox->GetWidth(), m_EditBox->GetHeight()))
        {
            ChangeEditBox(UISTATE_NORMAL);
        }

        if (SEASON3B::IsRelease(VK_LBUTTON)
            && CheckMouseIn(m_EditBox->GetPosition_x(), m_EditBox->GetPosition_y(), m_EditBox->GetWidth(), m_EditBox->GetHeight()) == false)
        {
            SetFocus(g_hWnd);
            CUITextInputBox::ReleaseFocus();
        }
    }

    m_EditBox->DoAction();

    for (int i = 0; i < MYSHOPINVENTORY_MAXBUTTONCOUNT; ++i)
    {
        if (m_Button[i].UpdateMouseEvent())
        {
            switch (i)
            {
            case 0:
            {
                g_pNewUISystem->Hide(SEASON3B::INTERFACE_MYSHOP_INVENTORY);
            }
            return false;
            case 1:
            {
                wchar_t shopTitle[MAX_SHOPTITLE + 1]{};
                g_pMyShopInventory->GetTitle(shopTitle);
                if (IsExistUndecidedPrice() == false && wcslen(shopTitle) > 0)
                {
                    if (m_EnablePersonalShop == false)
                    {
                        SEASON3B::CreateMessageBox(MSGBOX_LAYOUT_CLASS(SEASON3B::CPersonalshopCreateMsgBoxLayout));
                    }
                    else
                    {
                        wcscpy(g_szPersonalShopTitle, shopTitle);
                        SocketClient->ToGameServer()->SendPlayerShopOpen(shopTitle);

                        g_pNewUISystem->Hide(SEASON3B::INTERFACE_MYSHOP_INVENTORY);
                        g_pNewUISystem->Hide(SEASON3B::INTERFACE_INVENTORY);
                        g_pNewUISystem->Hide(SEASON3B::INTERFACE_INVENTORY_EXT);
                    }
                }
                else
                {
                    g_pSystemLogBox->AddText(I18N::Game::ThereSNoStoreNameOrItemPrice, SEASON3B::TYPE_ERROR_MESSAGE);
                }
            }
            return false;
            case 2:
            {
                SocketClient->ToGameServer()->SendPlayerShopClose();

                g_pNewUISystem->Hide(SEASON3B::INTERFACE_MYSHOP_INVENTORY);
                g_pNewUISystem->Hide(SEASON3B::INTERFACE_INVENTORY);
                g_pNewUISystem->Hide(SEASON3B::INTERFACE_INVENTORY_EXT);
            }
            return false;
            }
        }
    }

    if (CheckMouseIn(m_Pos.x, m_Pos.y, INVENTORY_WIDTH, INVENTORY_HEIGHT))
    {
        if (SEASON3B::IsPress(VK_RBUTTON))
        {
            MouseRButton = false;
            MouseRButtonPop = false;
            MouseRButtonPush = false;
            return false;
        }

        if (SEASON3B::IsNone(VK_LBUTTON) == false)
        {
            return false;
        }
    }

    return true;
}

bool SEASON3B::CNewUIMyShopInventory::Update()
{
    if (g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_MYSHOP_INVENTORY))
    {
        if (!m_bRequestedCurrency)
        {
            RequestCurrentCurrency(); // pull the current store currency once per open
            m_bRequestedCurrency = true;
        }
    }
    else
    {
        m_bRequestedCurrency = false; // re-request next time it opens
    }

    if (m_pNewInventoryCtrl && false == m_pNewInventoryCtrl->Update())
    {
        return false;
    }

    return true;
}

void SEASON3B::CNewUIMyShopInventory::RenderFrame()
{
    RenderImage(IMAGE_MYSHOPINVENTORY_BACK, m_Pos.x, m_Pos.y, INVENTORY_WIDTH, INVENTORY_HEIGHT);
    RenderImage(IMAGE_MYSHOPINVENTORY_TOP, m_Pos.x, m_Pos.y, INVENTORY_WIDTH, 64.f);
    RenderImage(IMAGE_MYSHOPINVENTORY_LEFT, m_Pos.x, m_Pos.y + 64, 21.f, 320.f);
    RenderImage(IMAGE_MYSHOPINVENTORY_RIGHT, m_Pos.x + INVENTORY_WIDTH - 21, m_Pos.y + 64, 21.f, 320.f);
    RenderImage(IMAGE_MYSHOPINVENTORY_BOTTOM, m_Pos.x, m_Pos.y + INVENTORY_HEIGHT - 45, INVENTORY_WIDTH, 45.f);
    RenderImage(IMAGE_MYSHOPINVENTORY_EDIT, m_Pos.x + 12, m_Pos.y + 49, 169.f, 26.f);

    wchar_t Text[100] = {};
    mu_swprintf(Text, I18N::Game::PersonalStore);
    RenderText(Text, m_Pos.x, m_Pos.y + 15, INVENTORY_WIDTH, 0, 0xFF49B0FF, 0x00000000, RT3_SORT_CENTER);
}

void SEASON3B::CNewUIMyShopInventory::RenderTextInfo()
{
    wchar_t Text[100];

    if (m_EnablePersonalShop)
    {
        RenderText(I18N::Game::StillOpening, m_Pos.x, m_Pos.y + 200 + kGridShift, INVENTORY_WIDTH, 0, RGBA(215, 138, 0, 255), 0x00000000, RT3_SORT_CENTER, g_hFontBold);
    }

    memset(&Text, 0, sizeof(wchar_t) * 100);
    mu_swprintf(Text, I18N::Game::Warning);
    RenderText(Text, m_Pos.x + 30, m_Pos.y + 230 + kGridShift, 0, 0, RGBA(255, 45, 47, 255), 0x00000000, RT3_SORT_LEFT, g_hFontBold);

    memset(&Text, 0, sizeof(wchar_t) * 100);
    mu_swprintf(Text, I18N::Game::SellingPriceWhenOpeningTheStore);
    RenderText(Text, m_Pos.x + 30, m_Pos.y + 250 + kGridShift, 0, 0, RGBA(247, 206, 77, 255), 0x00000000, RT3_SORT_LEFT);

    memset(&Text, 0, sizeof(wchar_t) * 100);
    mu_swprintf(Text, I18N::Game::PleaseVerify);
    RenderText(Text, m_Pos.x + 30, m_Pos.y + 262 + kGridShift, 0, 0, RGBA(247, 206, 77, 255), 0x00000000, RT3_SORT_LEFT);

    memset(&Text, 0, sizeof(wchar_t) * 100);
    mu_swprintf(Text, I18N::Game::AlreadyInThePersonalStore);
    RenderText(Text, m_Pos.x + 30, m_Pos.y + 274 + kGridShift, 0, 0, RGBA(247, 206, 77, 255), 0x00000000, RT3_SORT_LEFT);

    memset(&Text, 0, sizeof(wchar_t) * 100);
    mu_swprintf(Text, I18N::Game::CancelSoldItem);
    RenderText(Text, m_Pos.x + 30, m_Pos.y + 286 + kGridShift, 0, 0, RGBA(247, 206, 77, 255), 0x00000000, RT3_SORT_LEFT);

    memset(&Text, 0, sizeof(wchar_t) * 100);
    mu_swprintf(Text, I18N::Game::CanTBeReturned);
    RenderText(Text, m_Pos.x + 30, m_Pos.y + 298 + kGridShift, 0, 0, RGBA(247, 206, 77, 255), 0x00000000, RT3_SORT_LEFT);

    memset(&Text, 0, sizeof(wchar_t) * 100);
    mu_swprintf(Text, I18N::Game::AllItemTrading);
    RenderText(Text, m_Pos.x + 30, m_Pos.y + 320 + kGridShift, 0, 0, RGBA(255, 45, 47, 255), 0x00000000, RT3_SORT_LEFT, g_hFontBold);

    memset(&Text, 0, sizeof(wchar_t) * 100);
    mu_swprintf(Text, I18N::Game::CanOnlyBeDoneUsingZen);
    RenderText(Text, m_Pos.x + 30, m_Pos.y + 332 + kGridShift, 0, 0, RGBA(255, 45, 47, 255), 0x00000000, RT3_SORT_LEFT, g_hFontBold);
}

int SEASON3B::CNewUIMyShopInventory::CurrencySlotCount() const
{
    const int jewels = g_pJewelBank ? g_pJewelBank->GetEntryCount() : 0;
    return 1 + jewels; // slot 0 = Zen
}

bool SEASON3B::CNewUIMyShopInventory::IsSlotSelected(int k) const
{
    if (!g_ShopCurrency.Valid)
    {
        return k == 0; // default: highlight Zen until the server tells us
    }
    if (k == 0)
    {
        return g_ShopCurrency.IsZen;
    }
    if (g_ShopCurrency.IsZen || !g_pJewelBank)
    {
        return false;
    }
    const CNewUIJewelBank::JewelBankEntry* e = g_pJewelBank->GetEntry(k - 1);
    return e && e->Group == g_ShopCurrency.Group && e->Number == g_ShopCurrency.Number;
}

void SEASON3B::CNewUIMyShopInventory::RequestCurrentCurrency()
{
    if (SocketClient == NULL || Hero == NULL)
    {
        return;
    }
    // /bankdata populates the configured jewel list the strip lists (in case the bank window
    // was never opened this session); /shopcurdata gives the currently-selected currency.
    SocketClient->ToGameServer()->SendPublicChatMessage(Hero->ID, L"/bankdata");
    SocketClient->ToGameServer()->SendPublicChatMessage(Hero->ID, L"/shopcurdata");
}

void SEASON3B::CNewUIMyShopInventory::RenderCurrencyStrip()
{
    const bool enabled = (m_EnablePersonalShop == false); // only changeable while the store is closed

    RenderText(L"Sell in", m_Pos.x + kCurStripLeft, m_Pos.y + kCurStripY - 13, 0, 0,
        RGBA(150, 140, 90, 255), 0x00000000, RT3_SORT_LEFT);

    const int slots = CurrencySlotCount();
    const int slotW = CurSlotW(slots);
    for (int k = 0; k < slots; ++k)
    {
        const int ix = m_Pos.x + CurSlotX(k, slotW);
        const int iy = m_Pos.y + kCurStripY;
        const bool sel = IsSlotSelected(k);

        // Selected slot: a green underline bar (kept while disabled but greyed, so the current
        // currency stays visible while the store is open). EndRenderColor restores texturing.
        if (sel)
        {
            if (enabled) glColor4f(0.35f, 0.9f, 0.35f, 1.f);
            else         glColor4f(0.35f, 0.5f, 0.35f, 1.f);
            RenderColor((float)ix, (float)(iy + kCurSlotH - 2), (float)slotW, 3.f, 0.f, 0);
            EndRenderColor();
        }

        // Slot 0 is Zen (text). Jewel slots (1..N) get their icon in the 3D pass (RenderCurrencyIcons).
        if (k == 0)
        {
            DWORD color = !enabled ? RGBA(120, 120, 120, 255)
                        : sel      ? RGBA(120, 255, 120, 255)
                                   : RGBA(255, 220, 130, 255);
            RenderText(L"Zen", ix, iy + 8, slotW, 0, color, 0x00000000, RT3_SORT_CENTER);
        }
    }
}

void SEASON3B::CNewUIMyShopInventory::RenderCurrencyIcons()
{
    if (!g_pJewelBank || g_pJewelBank->GetEntryCount() <= 0)
    {
        return;
    }

    EndBitmap();
    glMatrixMode(GL_PROJECTION);
    SaveCameraPerspective();
    glPushMatrix();
    glLoadIdentity();
    glViewport2(0, 0, WindowWidth, WindowHeight);
    gluPerspective2(1.f, (float)(WindowWidth) / (float)(WindowHeight), RENDER_ITEMVIEW_NEAR, RENDER_ITEMVIEW_FAR);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    CameraProjection::GetOpenGLMatrix(g_Camera.Matrix);
    EnableDepthTest();
    EnableDepthMask();

    const float alpha = m_EnablePersonalShop ? 0.5f : 1.f; // dim while the store is open (not clickable)
    const int slots = CurrencySlotCount();
    const int slotW = CurSlotW(slots);
    const float iconSize = (float)(slotW - 6 < 20 ? slotW - 6 : 20);
    for (int k = 1; k < slots; ++k) // slot 0 is Zen (text); 1..N are jewels
    {
        const CNewUIJewelBank::JewelBankEntry* e = g_pJewelBank->GetEntry(k - 1);
        if (!e)
        {
            continue;
        }
        const int itemType = e->Group * MAX_ITEM_INDEX + e->Number;
        const float ix = (float)(m_Pos.x + CurSlotX(k, slotW)) + (slotW - iconSize) * 0.5f;
        const float iy = (float)(m_Pos.y + kCurStripY);
        glColor4f(1.f, 1.f, 1.f, alpha);
        RenderItem3D(ix, iy, iconSize, iconSize, itemType, 0, 0, 0, false);
    }
    glColor4f(1.f, 1.f, 1.f, 1.f);

    UpdateMousePositionn();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    RestoreCameraPerspective();
    BeginBitmap();
}

bool SEASON3B::CNewUIMyShopInventory::HandleCurrencyStrip()
{
    if (!SEASON3B::IsPress(VK_LBUTTON) || m_EnablePersonalShop)
    {
        return false; // ignore while the store is open (server rejects changes anyway)
    }

    const int slots = CurrencySlotCount();
    const int slotW = CurSlotW(slots);
    for (int k = 0; k < slots; ++k)
    {
        const int ix = m_Pos.x + CurSlotX(k, slotW);
        const int iy = m_Pos.y + kCurStripY;
        if (!CheckMouseIn(ix, iy, slotW, kCurSlotH))
        {
            continue;
        }

        wchar_t cmd[64] = { 0, };
        if (k == 0)
        {
            wcscpy(cmd, L"/shopcurrency zen");
        }
        else if (g_pJewelBank)
        {
            const CNewUIJewelBank::JewelBankEntry* e = g_pJewelBank->GetEntry(k - 1);
            if (!e || e->Alias[0] == L'\0')
            {
                return true;
            }
            mu_swprintf(cmd, L"/shopcurrency %ls", e->Alias);
        }
        else
        {
            return true;
        }

        if (SocketClient && Hero)
        {
            SocketClient->ToGameServer()->SendPublicChatMessage(Hero->ID, cmd);
        }
        PlayBuffer(SOUND_CLICK01);
        return true; // server replies with ShopCurrency -> highlight updates
    }
    return false;
}

bool SEASON3B::CNewUIMyShopInventory::Render()
{
    EnableAlphaTest();
    glColor4f(1.f, 1.f, 1.f, 1.f);

    RenderFrame();

    RenderTextInfo();

    RenderCurrencyStrip();

    if (m_EditBox)
    {
        m_EditBox->Render();
    }

    if (m_pNewInventoryCtrl)
    {
        m_pNewInventoryCtrl->Render();
    }

    for (int i = 0; i < MYSHOPINVENTORY_MAXBUTTONCOUNT; ++i)
    {
        m_Button[i].Render();
    }

    DisableAlphaBlend();

    RenderCurrencyIcons();

    return true;
}

void SEASON3B::CNewUIMyShopInventory::ClosingProcess()
{
    CNewUIInventoryCtrl::BackupPickedItem();
    g_pMyInventory->ChangeMyShopButtonStateOpen();
    SetFocus(g_hWnd);
    CUITextInputBox::ReleaseFocus();
}

int SEASON3B::CNewUIMyShopInventory::GetPointedItemIndex()
{
    return m_pNewInventoryCtrl->GetPointedSquareIndex();
}

int SEASON3B::CNewUIMyShopInventory::GetItemInventoryIndex(ITEM* pItem)
{
    return m_pNewInventoryCtrl->GetIndexByItem(pItem);
}

void SEASON3B::CNewUIMyShopInventory::ResetSubject()
{
    if (m_EditBox)
    {
        m_EditBox->SetText(NULL);
    }
}

bool SEASON3B::CNewUIMyShopInventory::IsEnableInputValueTextBox()
{
    return m_bIsEnableInputValueTextBox;
}

void SEASON3B::CNewUIMyShopInventory::SetInputValueTextBox(bool bIsEnable)
{
    m_bIsEnableInputValueTextBox = bIsEnable;
}