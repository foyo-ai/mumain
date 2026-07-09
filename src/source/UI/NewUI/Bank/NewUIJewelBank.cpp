// <copyright file="NewUIJewelBank.cpp" company="MUnique">
// Licensed under the MIT License. See LICENSE file in the project root for full license information.
// </copyright>

#include "stdafx.h"
#include "UI/NewUI/Bank/NewUIJewelBank.h"
#include "UI/NewUI/NewUISystem.h"
#include "Engine/Object/ZzzInventory.h"
#include "Engine/Object/ZzzInterface.h"
#include "Engine/Object/ZzzCharacter.h"
#include "Network/Server/WSclient.h"
#include "Audio/DSPlaySound.h"
#include "Camera/CameraProjection.h"
#include "I18N/All.h"

using namespace SEASON3B;

extern int MouseWheel; // global wheel delta (>0 up, <0 down); reset to 0 after handling

namespace
{
    constexpr int kFrameHeaderHeight = 64;
    constexpr int kFrameSideWidth = 21;
    constexpr int kFrameFooterHeight = 45;
    constexpr int kInnerRight = CNewUIJewelBank::JEWELBANK_WIDTH - kFrameSideWidth; // 169: inner edge
    // Row layout: the jewel name on line 1; on line 2 the icon, the count and the withdraw
    // buttons all sit on the same horizontal axis (same vertical centre).
    constexpr int kFirstRowY = 68;   // content starts just below the header (top-aligned)
    constexpr int kRowHeight = 38;
    constexpr int kVisibleRows = 5;  // rows that fit between the header and the hint line
    constexpr int kIconLeft = 14;
    constexpr int kIconScale = 18;     // icon draw size (on line 2, left of the count)
    constexpr int kTextCol = 38;       // name (line 1) and count (line 2) share this x
    constexpr int kLine1Y = 2;         // name vertical offset
    constexpr int kLine2Y = 20;        // icon + count + buttons vertical offset
    constexpr int kMaxItemTypeMultiplier = MAX_ITEM_INDEX; // group * 512 + number

    // Window labels are kept English/ASCII (the in-game text renderer cannot show diacritics).
    const wchar_t* const kJewelBankTitle = L"Jewel Bank";
    const wchar_t* const kJewelBankEmpty = L"No bankable items configured";
    const wchar_t* const kJewelBankHint  = L"Ctrl+Right-click a jewel to deposit";

    // Per-row withdraw buttons. "All" (kBankAllAmount) and every value is clamped to the row count.
    constexpr int kBankAllAmount = 9999;
    const int kAmountValues[CNewUIJewelBank::AMOUNT_BUTTON_COUNT] = { 1, 10, 30, kBankAllAmount };
    const wchar_t* const kAmountLabels[CNewUIJewelBank::AMOUNT_BUTTON_COUNT] = { L"-1", L"-10", L"-30", L"All" };

    constexpr int kBtnW = 22;
    constexpr int kBtnH = 16;
    constexpr int kBtnGap = 2;
    constexpr int kBtnBlockWidth = CNewUIJewelBank::AMOUNT_BUTTON_COUNT * kBtnW + (CNewUIJewelBank::AMOUNT_BUTTON_COUNT - 1) * kBtnGap; // 94

    // Scrollbar geometry: the thumb is 15px wide and drawn 4px left of the track x, so a track x
    // of kInnerRight-13 keeps the whole thumb inside the frame. It only appears on overflow.
    constexpr int kScrollTrackX = kInnerRight - 13;      // 156
    constexpr int kScrollThumbLeft = kScrollTrackX - 4;  // 152
    constexpr int kScrollHeight = kVisibleRows * kRowHeight; // 190

    // The button block always ends just left of the scrollbar gutter: the "All" button clips against
    // the frame's inner border if pushed further right, and keeping a fixed gutter means the buttons
    // never shift when the scrollbar appears/disappears. The gutter is empty until the list overflows.
    constexpr int kBtnBlockRight = kScrollThumbLeft - 2;         // 150
    constexpr int kBtnBlockLeft  = kBtnBlockRight - kBtnBlockWidth; // 56
    constexpr int kNameWidth = kInnerRight - kTextCol;           // names are short; run to inner edge
    constexpr int kCountWidth = kBtnBlockLeft - kTextCol - 3;    // count clips before the buttons
    // Hint sits just above the footer/exit-button row so it never draws over the exit button.
    constexpr int kHintY = CNewUIJewelBank::JEWELBANK_HEIGHT - kFrameFooterHeight - 10;

    int BtnX(int j) { return kBtnBlockLeft + j * (kBtnW + kBtnGap); }
}

CNewUIJewelBank::CNewUIJewelBank()
{
    m_pNewUIMng = NULL;
    m_Pos.x = m_Pos.y = 0;
    m_ScrollOffset = 0;
    m_bScrollVisible = false;
}

CNewUIJewelBank::~CNewUIJewelBank()
{
    Release();
}

bool CNewUIJewelBank::Create(CNewUIManager* pNewUIMng, int x, int y)
{
    if (NULL == pNewUIMng)
        return false;

    m_pNewUIMng = pNewUIMng;
    m_pNewUIMng->AddUIObj(SEASON3B::INTERFACE_JEWELBANK, this);

    SetPos(x, y);
    LoadImages();

    m_BtnExit.ChangeButtonImgState(true, IMAGE_JEWELBANK_EXIT_BTN, false);
    m_BtnExit.ChangeButtonInfo(m_Pos.x + JEWELBANK_WIDTH - 45, m_Pos.y + JEWELBANK_HEIGHT - 38, 36, 29);
    m_BtnExit.ChangeToolTipText(&I18N::Game::Close388, true);

    m_ScrollBar.Create(m_Pos.x + kScrollTrackX, m_Pos.y + kFirstRowY, kScrollHeight);
    m_ScrollBar.Show(false);

    // Per-row withdraw buttons as native widgets (hover/press states, click handling).
    for (int i = 0; i < MAX_JEWELBANK_ENTRIES; ++i)
    {
        for (int j = 0; j < AMOUNT_BUTTON_COUNT; ++j)
        {
            m_BtnAmount[i][j].ChangeButtonImgState(true, IMAGE_JEWELBANK_BTN, true);
            m_BtnAmount[i][j].SetFont(g_hFont);
            m_BtnAmount[i][j].ChangeText(std::wstring(kAmountLabels[j]));
            m_BtnAmount[i][j].ChangeTextColor(RGBA(255, 235, 150, 255));
        }
    }
    LayoutButtons();

    Show(false);

    return true;
}

void CNewUIJewelBank::LayoutButtons()
{
    for (int i = 0; i < MAX_JEWELBANK_ENTRIES; ++i)
    {
        const int vr = VisibleRowOf(i);
        for (int j = 0; j < AMOUNT_BUTTON_COUNT; ++j)
        {
            if (vr < 0 || vr >= kVisibleRows)
            {
                // Park off-screen so scrolled-out rows are never rendered or clickable.
                m_BtnAmount[i][j].ChangeButtonInfo(-1000, -1000, kBtnW, kBtnH + 5);
                continue;
            }
            const int line2Y = m_Pos.y + kFirstRowY + vr * kRowHeight + kLine2Y;
            m_BtnAmount[i][j].ChangeButtonInfo(m_Pos.x + BtnX(j), line2Y - 3, kBtnW, kBtnH + 5);
        }
    }
}

void CNewUIJewelBank::Release()
{
    m_ScrollBar.Release();
    UnloadImages();

    if (m_pNewUIMng)
    {
        m_pNewUIMng->RemoveUIObj(this);
        m_pNewUIMng = NULL;
    }
}

void CNewUIJewelBank::SetPos(int x, int y)
{
    m_Pos.x = x;
    m_Pos.y = y;
    m_ScrollBar.SetPos(m_Pos.x + kScrollTrackX, m_Pos.y + kFirstRowY);
    LayoutButtons();
}

void CNewUIJewelBank::SetBalances(const std::vector<JewelBankEntry>& entries)
{
    m_Entries = entries;
    if (m_Entries.size() > MAX_JEWELBANK_ENTRIES)
    {
        m_Entries.resize(MAX_JEWELBANK_ENTRIES);
    }
    m_ScrollOffset = 0; // start at the top on a fresh balance list
}

void CNewUIJewelBank::LoadImages()
{
    LoadBitmap(L"Interface\\newui_msgbox_back.jpg", IMAGE_JEWELBANK_BACK, GL_LINEAR);
    LoadBitmap(L"Interface\\newui_item_back01.tga", IMAGE_JEWELBANK_TOP, GL_LINEAR);
    LoadBitmap(L"Interface\\newui_item_back02-L.tga", IMAGE_JEWELBANK_LEFT, GL_LINEAR);
    LoadBitmap(L"Interface\\newui_item_back02-R.tga", IMAGE_JEWELBANK_RIGHT, GL_LINEAR);
    LoadBitmap(L"Interface\\newui_item_back03.tga", IMAGE_JEWELBANK_BOTTOM, GL_LINEAR);
    LoadBitmap(L"Interface\\newui_exit_00.tga", IMAGE_JEWELBANK_EXIT_BTN, GL_LINEAR);
    LoadBitmap(L"Interface\\newui_btn_empty_very_small.tga", IMAGE_JEWELBANK_BTN, GL_LINEAR);
}

void CNewUIJewelBank::UnloadImages()
{
    DeleteBitmap(IMAGE_JEWELBANK_BTN);
    DeleteBitmap(IMAGE_JEWELBANK_EXIT_BTN);
    DeleteBitmap(IMAGE_JEWELBANK_BOTTOM);
    DeleteBitmap(IMAGE_JEWELBANK_RIGHT);
    DeleteBitmap(IMAGE_JEWELBANK_LEFT);
    DeleteBitmap(IMAGE_JEWELBANK_TOP);
    DeleteBitmap(IMAGE_JEWELBANK_BACK);
}

bool CNewUIJewelBank::UpdateMouseEvent()
{
    if (m_bScrollVisible)
    {
        m_ScrollBar.UpdateMouseEvent();
    }

    if (m_BtnExit.UpdateMouseEvent() == true)
    {
        g_pNewUISystem->Hide(SEASON3B::INTERFACE_JEWELBANK);
        PlayBuffer(SOUND_CLICK01);
        return false;
    }

    if (HandleRowButtons())
    {
        return false;
    }

    if (CheckMouseIn(m_Pos.x, m_Pos.y, JEWELBANK_WIDTH, JEWELBANK_HEIGHT))
        return false;

    return true;
}

bool CNewUIJewelBank::HandleRowButtons()
{
    // Call UpdateMouseEvent on every active button each frame so hover/press states update;
    // act on the first one that reports a click.
    for (size_t i = 0; i < m_Entries.size(); ++i)
    {
        const int vr = VisibleRowOf((int)i);
        if (vr < 0 || vr >= kVisibleRows)
        {
            continue; // scrolled out of view
        }
        for (int j = 0; j < AMOUNT_BUTTON_COUNT; ++j)
        {
            if (!m_BtnAmount[i][j].UpdateMouseEvent())
            {
                continue;
            }

            const DWORD have = m_Entries[i].Count;
            if (have == 0)
            {
                PlayBuffer(SOUND_CLICK01);
                return true; // nothing to withdraw
            }

            int amount = kAmountValues[j];
            if (amount > (int)have) // clamp -1/-10/-30 and All to what is available
            {
                amount = (int)have;
            }

            SendBankCommand(L"withdraw", m_Entries[i].Alias, amount);
            PlayBuffer(SOUND_CLICK01);
            return true;
        }
    }
    return false;
}

bool CNewUIJewelBank::TryDepositAllOfItem(int group, int number)
{
    for (size_t i = 0; i < m_Entries.size(); ++i)
    {
        if (m_Entries[i].Group == group && m_Entries[i].Number == number)
        {
            SendBankCommand(L"deposit", m_Entries[i].Alias, kBankAllAmount);
            return true;
        }
    }
    return false;
}

void CNewUIJewelBank::SendBankCommand(const wchar_t* action, const wchar_t* alias, int amount)
{
    if (SocketClient == NULL || Hero == NULL || alias == NULL || alias[0] == L'\0')
    {
        return;
    }

    wchar_t command[64] = { 0, };
    mu_swprintf(command, L"/bank %ls %ls %d", action, alias, amount);
    SocketClient->ToGameServer()->SendPublicChatMessage(Hero->ID, command);

    // Refresh the window: only /bankdata emits the balances packet (deposit/withdraw just mutate + reply).
    SocketClient->ToGameServer()->SendPublicChatMessage(Hero->ID, L"/bankdata");
}

bool CNewUIJewelBank::UpdateKeyEvent()
{
    if (g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_JEWELBANK) == true)
    {
        if (SEASON3B::IsPress(VK_ESCAPE) == true)
        {
            g_pNewUISystem->Hide(SEASON3B::INTERFACE_JEWELBANK);
            PlayBuffer(SOUND_CLICK01);
            return false;
        }
    }
    return true;
}

bool CNewUIJewelBank::Update()
{
    UpdateScroll();
    LayoutButtons();
    return true;
}

// Shows/hides the scrollbar based on how many jewels are configured and refreshes the visible
// window offset. With the real 4-jewel config nothing overflows, so the scrollbar stays hidden.
void CNewUIJewelBank::UpdateScroll()
{
    const int entries = (int)m_Entries.size();
    const int maxOffset = (entries > kVisibleRows) ? (entries - kVisibleRows) : 0;
    m_bScrollVisible = (maxOffset > 0);

    if (!m_bScrollVisible)
    {
        m_ScrollBar.Show(false);
        m_ScrollOffset = 0;
        return;
    }

    m_ScrollBar.Show(true);
    m_ScrollBar.SetMaxPos(maxOffset);

    // Mouse wheel over the window scrolls the list (wheel up shows earlier rows).
    if (MouseWheel != 0 && CheckMouseIn(m_Pos.x, m_Pos.y, JEWELBANK_WIDTH, JEWELBANK_HEIGHT))
    {
        if (MouseWheel > 0) m_ScrollBar.SetCurPos(m_ScrollBar.GetCurPos() - 1);
        else                m_ScrollBar.SetCurPos(m_ScrollBar.GetCurPos() + 1);
        MouseWheel = 0;
    }

    m_ScrollBar.Update();

    m_ScrollOffset = m_ScrollBar.GetCurPos();
    if (m_ScrollOffset < 0)          m_ScrollOffset = 0;
    if (m_ScrollOffset > maxOffset)  m_ScrollOffset = maxOffset;
}

bool CNewUIJewelBank::Render()
{
    EnableAlphaTest();
    glColor4f(1.f, 1.f, 1.f, 1.f);

    RenderFrame();
    RenderRows();

    // Withdraw buttons (dimmed on empty rows), only for the visible window.
    for (size_t i = 0; i < m_Entries.size(); ++i)
    {
        const int vr = VisibleRowOf((int)i);
        if (vr < 0 || vr >= kVisibleRows)
        {
            continue;
        }
        const bool dim = (m_Entries[i].Count == 0);
        for (int j = 0; j < AMOUNT_BUTTON_COUNT; ++j)
        {
            m_BtnAmount[i][j].ChangeAlpha(dim ? 0.5f : 1.0f);
            m_BtnAmount[i][j].Render();
        }
    }

    g_pRenderText->SetFont(g_hFont);
    g_pRenderText->SetTextColor(200, 130, 60, 255);
    g_pRenderText->SetBgColor(0, 0, 0, 0);
    g_pRenderText->RenderText(m_Pos.x + 12.0f, m_Pos.y + (float)kHintY, kJewelBankHint, JEWELBANK_WIDTH - 24.0f, 0, RT3_SORT_CENTER);

    m_BtnExit.Render();

    DisableAlphaBlend();

    RenderIcons();

    if (m_bScrollVisible)
    {
        m_ScrollBar.Render();
    }

    return true;
}

// Renders the jewel icons in a 3D perspective pass (RenderItem3D needs the perspective/depth
// state set up; mirrors CNewUIGoldBowmanLena::Render3D()).
void CNewUIJewelBank::RenderIcons()
{
    if (m_Entries.empty())
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

    for (size_t i = 0; i < m_Entries.size(); ++i)
    {
        const int vr = VisibleRowOf((int)i);
        if (vr < 0 || vr >= kVisibleRows)
        {
            continue;
        }
        const JewelBankEntry& entry = m_Entries[i];
        const int itemType = entry.Group * kMaxItemTypeMultiplier + entry.Number;
        const float rowY = m_Pos.y + (float)(kFirstRowY + vr * kRowHeight);
        glColor4f(1.f, 1.f, 1.f, 1.f);
        // On line 2, vertically centred with the count text / buttons.
        RenderItem3D((float)(m_Pos.x + kIconLeft), rowY + (float)(kLine2Y - 2), (float)kIconScale, (float)kIconScale, itemType, 0, 0, 0, false);
    }

    UpdateMousePositionn();

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    RestoreCameraPerspective();
    BeginBitmap();
}

float CNewUIJewelBank::GetLayerDepth()
{
    return 4.0f;
}

void CNewUIJewelBank::RenderFrame()
{
    RenderImage(IMAGE_JEWELBANK_BACK, m_Pos.x, m_Pos.y, (float)JEWELBANK_WIDTH, (float)JEWELBANK_HEIGHT);
    RenderImage(IMAGE_JEWELBANK_TOP, m_Pos.x, m_Pos.y, (float)JEWELBANK_WIDTH, (float)kFrameHeaderHeight);
    RenderImage(IMAGE_JEWELBANK_LEFT, m_Pos.x, m_Pos.y + kFrameHeaderHeight, (float)kFrameSideWidth, (float)(JEWELBANK_HEIGHT - kFrameHeaderHeight));
    RenderImage(IMAGE_JEWELBANK_RIGHT, m_Pos.x + JEWELBANK_WIDTH - kFrameSideWidth, m_Pos.y + kFrameHeaderHeight, (float)kFrameSideWidth, (float)(JEWELBANK_HEIGHT - kFrameHeaderHeight));
    RenderImage(IMAGE_JEWELBANK_BOTTOM, m_Pos.x, m_Pos.y + JEWELBANK_HEIGHT - kFrameFooterHeight, (float)JEWELBANK_WIDTH, (float)kFrameFooterHeight);

    wchar_t szText[128] = { 0, };
    g_pRenderText->SetFont(g_hFontBold);
    g_pRenderText->SetTextColor(255, 230, 150, 255);
    g_pRenderText->SetBgColor(0, 0, 0, 0);

    mu_swprintf(szText, L"%ls", kJewelBankTitle);
    g_pRenderText->RenderText((float)m_Pos.x, m_Pos.y + 12.0f, szText, (float)JEWELBANK_WIDTH, 0, RT3_SORT_CENTER);
}

void CNewUIJewelBank::RenderRows()
{
    g_pRenderText->SetFont(g_hFont);
    g_pRenderText->SetBgColor(0, 0, 0, 0);

    if (m_Entries.empty())
    {
        g_pRenderText->SetTextColor(200, 200, 200, 255);
        g_pRenderText->RenderText(m_Pos.x + 15.0f, m_Pos.y + kFirstRowY, kJewelBankEmpty, JEWELBANK_WIDTH - 30.0f, 0, RT3_SORT_CENTER);
        return;
    }

    wchar_t szName[64] = { 0, };
    wchar_t szCount[32] = { 0, };
    for (size_t i = 0; i < m_Entries.size(); ++i)
    {
        const int vr = VisibleRowOf((int)i);
        if (vr < 0 || vr >= kVisibleRows)
        {
            continue;
        }
        const JewelBankEntry& entry = m_Entries[i];
        const int itemType = entry.Group * kMaxItemTypeMultiplier + entry.Number;
        const float rowY = m_Pos.y + (float)(kFirstRowY + vr * kRowHeight);
        const float line1 = rowY + (float)kLine1Y;
        const float line2 = rowY + (float)kLine2Y;

        // Name (line 1) and count (line 2) share kTextCol so they line up under each other;
        // the icon is drawn (vertically centred) in the 3D pass.
        GetItemName(itemType, 0, szName);
        g_pRenderText->SetFont(g_hFont);
        g_pRenderText->SetTextColor(235, 235, 235, 255);
        g_pRenderText->RenderText(m_Pos.x + (float)kTextCol, line1, szName, (float)kNameWidth, 0, RT3_SORT_LEFT);

        mu_swprintf(szCount, L"x%u", entry.Count);
        g_pRenderText->SetTextColor(150, 220, 255, 255);
        g_pRenderText->RenderText(m_Pos.x + (float)kTextCol, line2, szCount, (float)kCountWidth, 0, RT3_SORT_LEFT);
        // The withdraw buttons are CNewUIButton widgets, rendered/handled in Render()/HandleRowButtons().
    }
}
