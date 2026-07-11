// NewUIPurchaseShopInventory.cpp: implementation of the CNewUIPurchaseShopInventory class.
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "UI/NewUI/Inventory/NewUIPurchaseShopInventory.h"
#include "UI/NewUI/NewUISystem.h"
#include "UI/NewUI/Dialogs/NewUICustomMessageBox.h"
#include "I18N/All.h"

#include "GameLogic/Items/PersonalShopTitleImp.h"
#include "Engine/Object/ZzzInventory.h"
#include "Network/Server/WSclient.h"
#include "Camera/CameraProjection.h"

namespace
{
    // "Sells in: [icon] <currency>" header, shown under the shop title so the buyer sees the
    // store's currency without hovering each item. All offsets are relative to the window origin.
    // The item grid and the warning block are pushed down by kGridShift so the title, this header
    // and the grid each get some breathing room instead of being stacked tight.
    constexpr int kGridShift    = 26;  // item grid + warning text shift down to make room for the header
    constexpr int kCurHeaderY   = 84;  // currency row (below the name box at +49..75, above the grid)
    constexpr int kCurLabelX    = 18;  // "Sells in:" label
    constexpr int kCurIconX     = 66;  // jewel icon (skipped for Zen)
    constexpr int kCurIconY     = 83;
    constexpr int kCurIconSize  = 16;
    constexpr int kCurNameX     = 86;  // currency name, after the icon
    constexpr int kCurNameZenX  = 66;  // currency name for Zen (no icon, sits right after the label)

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
};

using namespace SEASON3B;

SEASON3B::CNewUIPurchaseShopInventory::CNewUIPurchaseShopInventory() : m_pNewUIMng(NULL), m_pNewInventoryCtrl(NULL)
{
    m_Pos.x = m_Pos.y = 0;
    m_ShopCharacterIndex = -1;
}

SEASON3B::CNewUIPurchaseShopInventory::~CNewUIPurchaseShopInventory()
{
    Release();
}

bool SEASON3B::CNewUIPurchaseShopInventory::Create(CNewUIManager* pNewUIMng, int x, int y)
{
    if (NULL == pNewUIMng || NULL == g_pNewUI3DRenderMng || NULL == g_pNewItemMng)
        return false;

    LoadImages();

    SetPos(x, y);

    m_pNewUIMng = pNewUIMng;
    m_pNewUIMng->AddUIObj(SEASON3B::INTERFACE_PURCHASESHOP_INVENTORY, this);

    m_pNewInventoryCtrl = new CNewUIInventoryCtrl;
    if (false == m_pNewInventoryCtrl->Create(STORAGE_TYPE::UNDEFINED, g_pNewUI3DRenderMng, g_pNewItemMng, this, m_Pos.x + 16, m_Pos.y + 90 + kGridShift, 8, 4, MAX_MY_INVENTORY_EX_INDEX))
    {
        SAFE_DELETE(m_pNewInventoryCtrl);
        return false;
    }

    m_pNewInventoryCtrl->SetToolTipType(TOOLTIP_TYPE_PURCHASE_SHOP);
    m_pNewInventoryCtrl->LockInventory();

    m_Button = new CNewUIButton;
    m_Button->ChangeButtonImgState(true, IMAGE_INVENTORY_EXIT_BTN, false);
    m_Button->ChangeButtonInfo(m_Pos.x + 13, m_Pos.y + 391, 36, 29);

    Show(false);

    return true;
}

void SEASON3B::CNewUIPurchaseShopInventory::Release()
{
    SAFE_DELETE(m_Button);

    SAFE_DELETE(m_pNewInventoryCtrl);

    if (m_pNewUIMng)
    {
        m_pNewUIMng->RemoveUIObj(this);
        m_pNewUIMng = NULL;
    }

    UnloadImages();
}

bool SEASON3B::CNewUIPurchaseShopInventory::InsertItem(int iIndex, std::span<const BYTE> pbyItemPacket)
{
    if (m_pNewInventoryCtrl)
    {
        return m_pNewInventoryCtrl->AddItem(iIndex, pbyItemPacket);
    }

    return false;
}

void SEASON3B::CNewUIPurchaseShopInventory::DeleteItem(int iIndex)
{
    if (m_pNewInventoryCtrl)
    {
        ITEM* pItem = m_pNewInventoryCtrl->FindItem(iIndex);

        if (pItem != NULL)
        {
            m_pNewInventoryCtrl->RemoveItem(pItem);
        }
    }
}

ITEM* SEASON3B::CNewUIPurchaseShopInventory::FindItem(int iLinealPos)
{
    if (m_pNewInventoryCtrl)
    {
        return m_pNewInventoryCtrl->FindItem(iLinealPos);
    }

    return NULL;
}

int SEASON3B::CNewUIPurchaseShopInventory::GetItemInventoryIndex(ITEM* pItem)
{
    if (m_pNewInventoryCtrl && pItem)
    {
        return m_pNewInventoryCtrl->GetIndexByItem(pItem);
    }

    return -1;
}

void SEASON3B::CNewUIPurchaseShopInventory::LoadImages()
{
    LoadBitmap(L"Interface\\newui_msgbox_back.jpg", IMAGE_MSGBOX_BACK, GL_LINEAR);
    LoadBitmap(L"Interface\\newui_item_back01.tga", IMAGE_INVENTORY_BACK_TOP, GL_LINEAR);
    LoadBitmap(L"Interface\\newui_item_back02-L.tga", IMAGE_INVENTORY_BACK_LEFT, GL_LINEAR);
    LoadBitmap(L"Interface\\newui_item_back02-R.tga", IMAGE_INVENTORY_BACK_RIGHT, GL_LINEAR);
    LoadBitmap(L"Interface\\newui_item_back03.tga", IMAGE_INVENTORY_BACK_BOTTOM, GL_LINEAR);
    LoadBitmap(L"Interface\\newui_exit_00.tga", IMAGE_INVENTORY_EXIT_BTN, GL_LINEAR);
    LoadBitmap(L"Interface\\newui_Box_openTitle.tga", IMAGE_MYSHOPINVENTORY_EDIT, GL_LINEAR);
}

void SEASON3B::CNewUIPurchaseShopInventory::UnloadImages()
{
    DeleteBitmap(IMAGE_MYSHOPINVENTORY_EDIT);
    DeleteBitmap(IMAGE_INVENTORY_EXIT_BTN);
    DeleteBitmap(IMAGE_INVENTORY_BACK_BOTTOM);
    DeleteBitmap(IMAGE_INVENTORY_BACK_RIGHT);
    DeleteBitmap(IMAGE_INVENTORY_BACK_LEFT);
    DeleteBitmap(IMAGE_INVENTORY_BACK_TOP);
    DeleteBitmap(IMAGE_MSGBOX_BACK);
}

bool SEASON3B::CNewUIPurchaseShopInventory::UpdateMouseEvent()
{
    POINT ptExitBtn1 = { m_Pos.x + 169, m_Pos.y + 7 };
    if (SEASON3B::IsRelease(VK_LBUTTON) && CheckMouseIn(ptExitBtn1.x, ptExitBtn1.y, 13, 12))
    {
        g_pNewUISystem->Hide(SEASON3B::INTERFACE_PURCHASESHOP_INVENTORY);
        return false;
    }
    if (m_Button->UpdateMouseEvent())
    {
        g_pNewUISystem->Hide(SEASON3B::INTERFACE_PURCHASESHOP_INVENTORY);
        return false;
    }

    if (m_pNewInventoryCtrl)
    {
        if (false == m_pNewInventoryCtrl->UpdateMouseEvent())
        {
            return false;
        }

        if (PurchaseShopInventoryProcess())
        {
            return false;
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

bool SEASON3B::CNewUIPurchaseShopInventory::UpdateKeyEvent()
{
    return true;
}

bool SEASON3B::CNewUIPurchaseShopInventory::PurchaseShopInventoryProcess()
{
    if (m_pNewInventoryCtrl && IsPress(VK_LBUTTON))
    {
        int iCurSquareIndex = m_pNewInventoryCtrl->GetIndexAtPt(MouseX, MouseY);
        if (iCurSquareIndex != -1 && m_pNewInventoryCtrl->FindItem(iCurSquareIndex) != nullptr)
        {
            ChangeSourceIndex(iCurSquareIndex);
            CreateMessageBox(MSGBOX_LAYOUT_CLASS(SEASON3B::CPersonalShopItemBuyMsgBoxLayout));
        }

        return true;
    }

    return false;
}

bool SEASON3B::CNewUIPurchaseShopInventory::Update()
{
    if (m_pNewInventoryCtrl && false == m_pNewInventoryCtrl->Update())
    {
        return false;
    }
    return true;
}

void SEASON3B::CNewUIPurchaseShopInventory::RenderFrame()
{
    RenderImage(IMAGE_MSGBOX_BACK, m_Pos.x, m_Pos.y, 190.f, 429.f);
    RenderImage(IMAGE_INVENTORY_BACK_TOP, m_Pos.x, m_Pos.y, 190.f, 64.f);
    RenderImage(IMAGE_INVENTORY_BACK_LEFT, m_Pos.x, m_Pos.y + 64, 21.f, 320.f);
    RenderImage(IMAGE_INVENTORY_BACK_RIGHT, m_Pos.x + 190 - 21, m_Pos.y + 64, 21.f, 320.f);
    RenderImage(IMAGE_INVENTORY_BACK_BOTTOM, m_Pos.x, m_Pos.y + 429 - 45, 190.f, 45.f);
    RenderImage(IMAGE_MYSHOPINVENTORY_EDIT, m_Pos.x + 12, m_Pos.y + 49, 169.f, 26.f);
}

void SEASON3B::CNewUIPurchaseShopInventory::RenderTextInfo()
{
    RenderText(I18N::Game::PersonalStore, m_Pos.x, m_Pos.y + 15, 190, 0, 0xFF49B0FF, 0x00000000, RT3_SORT_CENTER);
    RenderText(m_TitleText.c_str(), m_Pos.x, m_Pos.y + 58, 190, 0, RGBA(0, 255, 0, 255), 0x00000000, RT3_SORT_CENTER, g_hFontBold);

    RenderCurrencyHeader();

    wchar_t Text[100];

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
    mu_swprintf(Text, I18N::Game::CancelPurchasedItem);
    RenderText(Text, m_Pos.x + 30, m_Pos.y + 286 + kGridShift, 0, 0, RGBA(247, 206, 77, 255), 0x00000000, RT3_SORT_LEFT);

    memset(&Text, 0, sizeof(wchar_t) * 100);
    mu_swprintf(Text, I18N::Game::CanTBeReturned);
    RenderText(Text, m_Pos.x + 30, m_Pos.y + 298 + kGridShift, 0, 0, RGBA(247, 206, 77, 255), 0x00000000, RT3_SORT_LEFT);

    memset(&Text, 0, sizeof(wchar_t) * 100);
    mu_swprintf(Text, I18N::Game::AllItemTrading);
    RenderText(Text, m_Pos.x + 30, m_Pos.y + 320 + kGridShift, 0, 0, RGBA(255, 45, 47, 255), 0x00000000, RT3_SORT_LEFT, g_hFontBold);

    // A store is single-currency (shown in the "Sells in:" header above); keep the line short
    // to fit the 190px window.
    memset(&Text, 0, sizeof(wchar_t) * 100);
    mu_swprintf(Text, I18N::Game::UsesTheStoreCurrency);
    RenderText(Text, m_Pos.x + 30, m_Pos.y + 332 + kGridShift, 0, 0, RGBA(255, 45, 47, 255), 0x00000000, RT3_SORT_LEFT, g_hFontBold);
}

// "Sells in: [icon] <currency>" 2D text. The jewel icon itself is drawn in RenderCurrencyIcon()
// (a 3D pass); here we lay out the "Sells in:" label and the currency name around it.
void SEASON3B::CNewUIPurchaseShopInventory::RenderCurrencyHeader()
{
    if (!g_PurchaseShopCurrency.Valid)
    {
        return; // no item list received yet
    }

    RenderText(I18N::Game::SellsInColon, m_Pos.x + kCurLabelX, m_Pos.y + kCurHeaderY, 0, 0,
        RGBA(150, 140, 90, 255), 0x00000000, RT3_SORT_LEFT);

    if (g_PurchaseShopCurrency.IsZen)
    {
        RenderText(I18N::Game::Zen, m_Pos.x + kCurNameZenX, m_Pos.y + kCurHeaderY, 0, 0,
            RGBA(255, 220, 130, 255), 0x00000000, RT3_SORT_LEFT);
    }
    else
    {
        wchar_t name[64] = { 0, };
        GetItemName(g_PurchaseShopCurrency.Group * MAX_ITEM_INDEX + g_PurchaseShopCurrency.Number, 0, name);
        RenderText(name, m_Pos.x + kCurNameX, m_Pos.y + kCurHeaderY, 0, 0,
            RGBA(120, 255, 120, 255), 0x00000000, RT3_SORT_LEFT);
    }
}

// Draws the store-currency jewel icon in a 3D perspective pass (RenderItem3D needs the depth/
// perspective state set up; mirrors CNewUIMyShopInventory::RenderCurrencyIcons). Zen has no icon.
void SEASON3B::CNewUIPurchaseShopInventory::RenderCurrencyIcon()
{
    if (!g_PurchaseShopCurrency.Valid || g_PurchaseShopCurrency.IsZen)
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

    const int itemType = g_PurchaseShopCurrency.Group * MAX_ITEM_INDEX + g_PurchaseShopCurrency.Number;
    glColor4f(1.f, 1.f, 1.f, 1.f);
    RenderItem3D((float)(m_Pos.x + kCurIconX), (float)(m_Pos.y + kCurIconY),
        (float)kCurIconSize, (float)kCurIconSize, itemType, 0, 0, 0, false);

    UpdateMousePositionn();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    RestoreCameraPerspective();
    BeginBitmap();
}

bool SEASON3B::CNewUIPurchaseShopInventory::Render()
{
    EnableAlphaTest();
    glColor4f(1.f, 1.f, 1.f, 1.f);

    RenderFrame();

    RenderTextInfo();

    if (m_pNewInventoryCtrl)
    {
        m_pNewInventoryCtrl->Render();
    }

    m_Button->Render();

    DisableAlphaBlend();

    RenderCurrencyIcon(); // 3D pass for the store-currency jewel icon in the "Sells in:" header

    return true;
}

void SEASON3B::CNewUIPurchaseShopInventory::ClosingProcess()
{
    if (m_pNewInventoryCtrl)
    {
        m_pNewInventoryCtrl->RemoveAllItems();
        g_ErrorReport.Write(L"@ [Notice] CNewUIPurchaseShopInventory::ClosingProcess():m_pNewInventoryCtrl->RemoveAllItems(); )\n");
    }

    m_ShopCharacterIndex = -1;
    g_PurchaseShopCurrency.Valid = false;

    g_pMyInventory->ChangeMyShopButtonStateOpen();
}

int SEASON3B::CNewUIPurchaseShopInventory::GetPointedItemIndex()
{
    return m_pNewInventoryCtrl->GetPointedSquareIndex();
}