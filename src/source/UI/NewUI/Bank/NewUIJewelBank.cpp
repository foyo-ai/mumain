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
#include "I18N/All.h"

using namespace SEASON3B;

namespace
{
    constexpr int kFrameHeaderHeight = 64;
    constexpr int kFrameSideWidth = 21;
    constexpr int kFrameFooterHeight = 45;
    constexpr int kFirstRowY = 74;
    constexpr int kRowHeight = 24;
    constexpr int kTextLeft = 20;
    constexpr int kMaxItemTypeMultiplier = MAX_ITEM_INDEX; // group * 512 + number

    // Window labels are kept English/ASCII (the in-game text renderer cannot show diacritics).
    const wchar_t* const kJewelBankTitle = L"Jewel Bank";
    const wchar_t* const kJewelBankEmpty = L"No bankable items configured";
    const wchar_t* const kJewelBankHint = L"L-click deposit  R-click withdraw";

    // Quick-amount buttons. "All" sends a large count; the server clamps to what is actually available.
    constexpr int kBankAllAmount = 9999;
    const int kAmountValues[CNewUIJewelBank::AMOUNT_BUTTON_COUNT] = { 1, 10, 50, kBankAllAmount };
    const wchar_t* const kAmountLabels[CNewUIJewelBank::AMOUNT_BUTTON_COUNT] = { L"1", L"10", L"50", L"All" };

    constexpr int kAmountBtnW = 34;
    constexpr int kAmountBtnH = 20;
    constexpr int kAmountBtnGap = 4;
    constexpr int kAmountBtnFirstX = 26;
    constexpr int kAmountBtnY = CNewUIJewelBank::JEWELBANK_HEIGHT - 82;
    constexpr int kHintY = CNewUIJewelBank::JEWELBANK_HEIGHT - 56;
}

CNewUIJewelBank::CNewUIJewelBank()
{
    m_pNewUIMng = NULL;
    m_Pos.x = m_Pos.y = 0;
    m_AmountIndex = 0;
}

int CNewUIJewelBank::GetSelectedAmount() const
{
    if (m_AmountIndex < 0 || m_AmountIndex >= AMOUNT_BUTTON_COUNT)
    {
        return 1;
    }
    return kAmountValues[m_AmountIndex];
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

    if (HandleAmountButtons())
    {
        return false;
    }

    // Left-click a jewel row to deposit the selected amount; right-click to withdraw it.
    const int amount = GetSelectedAmount();
    for (size_t i = 0; i < m_Entries.size(); ++i)
    {
        const int rowY = m_Pos.y + kFirstRowY + (int)i * kRowHeight;
        if (!CheckMouseIn(m_Pos.x + kFrameSideWidth, rowY, JEWELBANK_WIDTH - kFrameSideWidth * 2, kRowHeight))
        {
            continue;
        }

        if (SEASON3B::IsPress(VK_LBUTTON))
        {
            SendBankCommand(L"deposit", m_Entries[i].Alias, amount);
            PlayBuffer(SOUND_CLICK01);
            return false;
        }
        if (SEASON3B::IsPress(VK_RBUTTON))
        {
            SendBankCommand(L"withdraw", m_Entries[i].Alias, amount);
            PlayBuffer(SOUND_CLICK01);
            return false;
        }
    }

    if (CheckMouseIn(m_Pos.x, m_Pos.y, JEWELBANK_WIDTH, JEWELBANK_HEIGHT))
        return false;

    return true;
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
    RenderBalances();
    RenderAmountButtons();

    m_BtnExit.Render();

    DisableAlphaBlend();

    return true;
}

bool CNewUIJewelBank::HandleAmountButtons()
{
    if (!SEASON3B::IsPress(VK_LBUTTON))
    {
        return false;
    }

    for (int j = 0; j < AMOUNT_BUTTON_COUNT; ++j)
    {
        const int bx = m_Pos.x + kAmountBtnFirstX + j * (kAmountBtnW + kAmountBtnGap);
        const int by = m_Pos.y + kAmountBtnY;
        if (CheckMouseIn(bx, by, kAmountBtnW, kAmountBtnH))
        {
            m_AmountIndex = j;
            PlayBuffer(SOUND_CLICK01);
            return true;
        }
    }
    return false;
}

void CNewUIJewelBank::RenderAmountButtons()
{
    g_pRenderText->SetFont(g_hFont);
    g_pRenderText->SetBgColor(0, 0, 0, 0);

    for (int j = 0; j < AMOUNT_BUTTON_COUNT; ++j)
    {
        const float bx = (float)(m_Pos.x + kAmountBtnFirstX + j * (kAmountBtnW + kAmountBtnGap));
        const float by = (float)(m_Pos.y + kAmountBtnY);

        if (j == m_AmountIndex)
        {
            g_pRenderText->SetTextColor(255, 230, 150, 255);
        }
        else
        {
            g_pRenderText->SetTextColor(170, 170, 170, 255);
        }
        g_pRenderText->RenderText(bx, by + 3.0f, kAmountLabels[j], (float)kAmountBtnW, 0, RT3_SORT_CENTER);
    }

    g_pRenderText->SetTextColor(140, 140, 140, 255);
    g_pRenderText->RenderText(m_Pos.x + 12.0f, m_Pos.y + (float)kHintY, kJewelBankHint, JEWELBANK_WIDTH - 24.0f, 0, RT3_SORT_CENTER);
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
    g_pRenderText->RenderText(m_Pos.x + 15.0f, m_Pos.y + 22.0f, szText, JEWELBANK_WIDTH - 30.0f, 0, RT3_SORT_CENTER);
}

void CNewUIJewelBank::RenderBalances()
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
        GetItemName(itemType, 0, szName);

        const float rowY = m_Pos.y + (float)(kFirstRowY + (int)i * kRowHeight);

        g_pRenderText->SetTextColor(230, 230, 230, 255);
        g_pRenderText->RenderText(m_Pos.x + (float)kTextLeft, rowY, szName, 120.0f, 0, RT3_SORT_LEFT);

        mu_swprintf(szCount, L"%u", entry.Count);
        g_pRenderText->SetTextColor(150, 220, 255, 255);
        g_pRenderText->RenderText(m_Pos.x + (float)kTextLeft, rowY, szCount, (float)(JEWELBANK_WIDTH - kTextLeft * 2), 0, RT3_SORT_RIGHT);
    }
}
