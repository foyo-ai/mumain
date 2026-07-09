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

namespace
{
    constexpr int kFrameHeaderHeight = 64;
    constexpr int kFrameSideWidth = 21;
    constexpr int kFrameFooterHeight = 45;
    // Two-line rows: a vertically-centred icon on the left, then a name (line 1) / count (line 2)
    // column aligned to the same x, and the withdraw buttons on line 2.
    constexpr int kFirstRowY = 78;
    constexpr int kRowHeight = 40;
    constexpr int kIconLeft = 12;
    constexpr int kIconScale = 26;     // icon draw size (vertically centred over both lines)
    constexpr int kIconY = 4;          // icon vertical offset within the row
    constexpr int kTextCol = 44;       // name (line 1) and count (line 2) share this x
    constexpr int kLine1Y = 2;         // name vertical offset
    constexpr int kLine2Y = 22;        // count + buttons vertical offset
    constexpr int kMaxItemTypeMultiplier = MAX_ITEM_INDEX; // group * 512 + number

    // Window labels are kept English/ASCII (the in-game text renderer cannot show diacritics).
    const wchar_t* const kJewelBankTitle = L"Jewel Bank";
    const wchar_t* const kJewelBankEmpty = L"No bankable items configured";
    const wchar_t* const kJewelBankHint  = L"Ctrl+Right-click a jewel to deposit";

    // Per-row withdraw buttons. "All" (kBankAllAmount) and every value is clamped to the row count.
    constexpr int kBankAllAmount = 9999;
    const int kAmountValues[CNewUIJewelBank::AMOUNT_BUTTON_COUNT] = { 1, 10, 30, kBankAllAmount };
    const wchar_t* const kAmountLabels[CNewUIJewelBank::AMOUNT_BUTTON_COUNT] = { L"-1", L"-10", L"-30", L"All" };

    constexpr int kBtnW = 24;
    constexpr int kBtnH = 15;
    constexpr int kBtnGap = 3;
    constexpr int kBtnBlockRight = CNewUIJewelBank::JEWELBANK_WIDTH - 14;
    constexpr int kBtnBlockLeft = kBtnBlockRight - (CNewUIJewelBank::AMOUNT_BUTTON_COUNT * kBtnW + (CNewUIJewelBank::AMOUNT_BUTTON_COUNT - 1) * kBtnGap);
    constexpr int kNameWidth = CNewUIJewelBank::JEWELBANK_WIDTH - kTextCol - 12;
    constexpr int kHintY = CNewUIJewelBank::JEWELBANK_HEIGHT - 38;

    int BtnX(int j) { return kBtnBlockLeft + j * (kBtnW + kBtnGap); }
}

CNewUIJewelBank::CNewUIJewelBank()
{
    m_pNewUIMng = NULL;
    m_Pos.x = m_Pos.y = 0;
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

    Show(false);

    return true;
}

void CNewUIJewelBank::Release()
{
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
}

void CNewUIJewelBank::SetBalances(const std::vector<JewelBankEntry>& entries)
{
    m_Entries = entries;
    if (m_Entries.size() > MAX_JEWELBANK_ENTRIES)
    {
        m_Entries.resize(MAX_JEWELBANK_ENTRIES);
    }
}

void CNewUIJewelBank::LoadImages()
{
    LoadBitmap(L"Interface\\newui_msgbox_back.jpg", IMAGE_JEWELBANK_BACK, GL_LINEAR);
    LoadBitmap(L"Interface\\newui_item_back01.tga", IMAGE_JEWELBANK_TOP, GL_LINEAR);
    LoadBitmap(L"Interface\\newui_item_back02-L.tga", IMAGE_JEWELBANK_LEFT, GL_LINEAR);
    LoadBitmap(L"Interface\\newui_item_back02-R.tga", IMAGE_JEWELBANK_RIGHT, GL_LINEAR);
    LoadBitmap(L"Interface\\newui_item_back03.tga", IMAGE_JEWELBANK_BOTTOM, GL_LINEAR);
    LoadBitmap(L"Interface\\newui_exit_00.tga", IMAGE_JEWELBANK_EXIT_BTN, GL_LINEAR);
}

bool CNewUIJewelBank::UpdateMouseEvent()
{
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
    if (!SEASON3B::IsPress(VK_LBUTTON))
    {
        return false;
    }

    for (size_t i = 0; i < m_Entries.size(); ++i)
    {
        const int line2Y = m_Pos.y + kFirstRowY + (int)i * kRowHeight + kLine2Y;
        for (int j = 0; j < AMOUNT_BUTTON_COUNT; ++j)
        {
            const int bx = m_Pos.x + BtnX(j);
            if (!CheckMouseIn(bx, line2Y, kBtnW, kBtnH))
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
    return true;
}

bool CNewUIJewelBank::Render()
{
    EnableAlphaTest();
    glColor4f(1.f, 1.f, 1.f, 1.f);

    RenderFrame();
    RenderRows();

    g_pRenderText->SetFont(g_hFont);
    g_pRenderText->SetTextColor(200, 130, 60, 255);
    g_pRenderText->SetBgColor(0, 0, 0, 0);
    g_pRenderText->RenderText(m_Pos.x + 12.0f, m_Pos.y + (float)kHintY, kJewelBankHint, JEWELBANK_WIDTH - 24.0f, 0, RT3_SORT_CENTER);

    m_BtnExit.Render();

    DisableAlphaBlend();

    RenderIcons();

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
        const JewelBankEntry& entry = m_Entries[i];
        const int itemType = entry.Group * kMaxItemTypeMultiplier + entry.Number;
        const float rowY = m_Pos.y + (float)(kFirstRowY + (int)i * kRowHeight);
        glColor4f(1.f, 1.f, 1.f, 1.f);
        RenderItem3D((float)(m_Pos.x + kIconLeft), rowY + (float)kIconY, (float)kIconScale, (float)kIconScale, itemType, 0, 0, 0, false);
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
    g_pRenderText->RenderText((float)m_Pos.x, m_Pos.y + 30.0f, szText, (float)JEWELBANK_WIDTH, 0, RT3_SORT_CENTER);
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
        const JewelBankEntry& entry = m_Entries[i];
        const int itemType = entry.Group * kMaxItemTypeMultiplier + entry.Number;
        const float rowY = m_Pos.y + (float)(kFirstRowY + (int)i * kRowHeight);
        const float line1 = rowY + (float)kLine1Y;
        const float line2 = rowY + (float)kLine2Y;
        const bool dim = (entry.Count == 0);

        // Name (line 1) and count (line 2) share kTextCol so they line up under each other;
        // the icon is drawn (vertically centred) in the 3D pass.
        GetItemName(itemType, 0, szName);
        g_pRenderText->SetFont(g_hFont);
        g_pRenderText->SetTextColor(235, 235, 235, 255);
        g_pRenderText->RenderText(m_Pos.x + (float)kTextCol, line1, szName, (float)kNameWidth, 0, RT3_SORT_LEFT);

        mu_swprintf(szCount, L"x%u", entry.Count);
        g_pRenderText->SetTextColor(150, 220, 255, 255);
        g_pRenderText->RenderText(m_Pos.x + (float)kTextCol, line2, szCount, 40.0f, 0, RT3_SORT_LEFT);

        // Withdraw buttons: the shared "empty button" texture behind each label so they read as
        // clickable (RenderImage batches fine, unlike RenderColor which only draws once per frame).
        for (int j = 0; j < AMOUNT_BUTTON_COUNT; ++j)
        {
            const float bx = (float)(m_Pos.x + BtnX(j));
            glColor4f(1.f, 1.f, 1.f, dim ? 0.55f : 1.f);
            RenderImage(IMAGE_JEWELBANK_BTN, bx, line2 - 3.f, (float)kBtnW, (float)kBtnH + 5.f);
            glColor4f(1.f, 1.f, 1.f, 1.f);

            g_pRenderText->SetFont(g_hFont);
            g_pRenderText->SetTextColor(dim ? 150 : 255, dim ? 140 : 235, dim ? 100 : 150, 255);
            g_pRenderText->RenderText(bx, line2, kAmountLabels[j], (float)kBtnW, 0, RT3_SORT_CENTER);
        }
    }
}
