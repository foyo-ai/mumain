// <copyright file="NewUIJewelBank.cpp" company="MUnique">
// Licensed under the MIT License. See LICENSE file in the project root for full license information.
// </copyright>

#include "stdafx.h"
#include "UI/NewUI/Bank/NewUIJewelBank.h"
#include "UI/NewUI/NewUISystem.h"
#include "Engine/Object/ZzzInventory.h"
#include "Engine/Object/ZzzInterface.h"
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

    if (CheckMouseIn(m_Pos.x, m_Pos.y, JEWELBANK_WIDTH, JEWELBANK_HEIGHT))
        return false;

    return true;
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

    m_BtnExit.Render();

    DisableAlphaBlend();

    return true;
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
