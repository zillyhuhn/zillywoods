#include <base/color.h>
#include <base/math.h>

#include <engine/engine.h>
#include <engine/graphics.h>
#include <engine/serverbrowser.h>
#include <engine/storage.h>
#include <engine/textrender.h>
#include <engine/external/json-parser/json.h>
#include <engine/shared/config.h>

#include <generated/protocol.h>
#include <generated/client_data.h>

#include <game/client/components/maplayers.h>
#include <game/client/components/sounds.h>
#include <game/client/components/stats.h>
#include <game/client/ui.h>
#include <game/client/render.h>
#include <game/client/gameclient.h>
#include <game/client/animstate.h>

#include "binds.h"
#include "countryflags.h"
#include "menus.h"

void CMenus::RenderSettingsZilly(CUIRect MainView)
{
	CUIRect Label, Button, Game, BottomView, Background;

	// cut view
	MainView.HSplitBottom(80.0f, &MainView, &BottomView);
	BottomView.HSplitTop(20.f, 0, &BottomView);

	// render game menu backgrounds
	int NumOptions = 6;
	float ButtonHeight = 20.0f;
	float Spacing = 2.0f;
	float BackgroundHeight = (float)(NumOptions+1)*ButtonHeight+(float)NumOptions*Spacing;

	if(this->Client()->State() == IClient::STATE_ONLINE)
		Background = MainView;
	else
		MainView.HSplitTop(20.0f, 0, &Background);
	RenderTools()->DrawUIRect(&Background, vec4(0.0f, 0.0f, 0.0f, Config()->m_ClMenuAlpha/100.0f), this->Client()->State() == IClient::STATE_OFFLINE ? CUI::CORNER_ALL : CUI::CORNER_B, 5.0f);
	MainView.HSplitTop(20.0f, 0, &MainView);
	MainView.HSplitTop(BackgroundHeight, &Game, &MainView);
	RenderTools()->DrawUIRect(&Game, vec4(0.0f, 0.0f, 0.0f, 0.25f), CUI::CORNER_ALL, 5.0f);

	// render client menu background
	NumOptions = 4;
	if(Config()->m_ClAutoDemoRecord) NumOptions += 1;
	if(Config()->m_ClAutoScreenshot) NumOptions += 1;
	BackgroundHeight = (float)(NumOptions+1)*ButtonHeight+(float)NumOptions*Spacing;

	CUIRect GameLeft, GameRight;
	// render game menu
	Game.HSplitTop(ButtonHeight, &Label, &Game);
	Label.y += 2.0f;
	UI()->DoLabel(&Label, Localize("ZillyWoods"), ButtonHeight*ms_FontmodHeight*0.8f, CUI::ALIGN_CENTER);

	Game.VSplitMid(&GameLeft, &GameRight);
	GameLeft.VSplitRight(Spacing * 0.5f, &GameLeft, 0);
	GameRight.VSplitLeft(Spacing * 0.5f, 0, &GameRight);

	// left side
	GameLeft.HSplitTop(Spacing, 0, &GameLeft);
	GameLeft.HSplitTop(ButtonHeight, &Button, &GameLeft);
	static int s_ShowAdmins = 0;
	if(DoButton_CheckBox(&s_ShowAdmins, Localize("Show admins in scoreboard (red)"), Config()->m_ClShowAdmins, &Button))
		Config()->m_ClShowAdmins ^= 1;

	GameLeft.HSplitTop(Spacing, 0, &GameLeft);
	GameLeft.HSplitTop(ButtonHeight, &Button, &GameLeft);
	static int s_SilentMessages = 0;
	if(DoButton_CheckBox(&s_SilentMessages, Localize("Show silent chat messages"), Config()->m_ClShowSilentMessages, &Button))
		Config()->m_ClShowSilentMessages ^= 1;

	GameLeft.HSplitTop(Spacing, 0, &GameLeft);
	GameLeft.HSplitTop(ButtonHeight, &Button, &GameLeft);
	static int s_TextEntities = 0;
	if(DoButton_CheckBox(&s_TextEntities, Localize("Show text entities"), Config()->m_ClTextEntities, &Button))
		Config()->m_ClTextEntities ^= 1;

	// right side
	GameRight.HSplitTop(Spacing, 0, &GameRight);
	GameRight.HSplitTop(ButtonHeight, &Button, &GameRight);
	static int s_ClientChatCmds = 0;
	if(DoButton_CheckBox(&s_ClientChatCmds, Localize("Client commands in chat (starting with '/')"), Config()->m_ClClientChatCommands, &Button))
		Config()->m_ClClientChatCommands ^= 1;

	GameRight.HSplitTop(Spacing, 0, &GameRight);
	GameRight.HSplitTop(ButtonHeight, &Button, &GameRight);
	static int s_OldGunPos = 0;
	if(DoButton_CheckBox(&s_OldGunPos, Localize("Old gun position"), Config()->m_ClOldGunPosition, &Button))
		Config()->m_ClOldGunPosition ^= 1;

	GameRight.HSplitTop(Spacing, 0, &GameRight);
	GameRight.HSplitTop(ButtonHeight, &Button, &GameRight);
	static int s_OldChatSounds = 0;
	if(DoButton_CheckBox(&s_OldChatSounds, Localize("Old chat sounds"), Config()->m_ClOldChatSounds, &Button))
		Config()->m_ClOldChatSounds ^= 1;

	GameRight.HSplitTop(Spacing, 0, &GameRight);
	GameRight.HSplitTop(ButtonHeight, &Button, &GameRight);
	static int s_CustomClientDetection = 0;
	if(DoButton_CheckBox(&s_CustomClientDetection, Localize("Enable custom client recognition"), Config()->m_ClClientRecognition, &Button))
		Config()->m_ClClientRecognition ^= 1;

	CUIRect aRects[2];
	MainView.HSplitTop(5.0f, 0, &MainView);
	MainView.HSplitTop(82.5f, &Label, &MainView);

	Label.VSplitMid(&aRects[0], &aRects[1]);
	aRects[0].VSplitRight(10.0f, &aRects[0], 0);
	aRects[1].VSplitLeft(10.0f, 0, &aRects[1]);

	int *pColorSlider[2][3] = {{&Config()->m_ClBackgroundHue, &Config()->m_ClBackgroundSat, &Config()->m_ClBackgroundLht}, {&Config()->m_ClBackgroundEntitiesHue, &Config()->m_ClBackgroundEntitiesSat, &Config()->m_ClBackgroundEntitiesLht}};

	const char *paParts[] = {
		Localize("Background (when quads disabled or zoomed out)"),
		Localize("Background (when entities are enabled)")};
	const char *paLabels[] = {
		Localize("Hue"),
		Localize("Sat."),
		Localize("Lht.")};

	for(int i = 0; i < 2; i++)
	{
		aRects[i].HSplitTop(20.0f, &Label, &aRects[i]);
		UI()->DoLabel(&Label, paParts[i], 14.0f, CUI::ALIGN_CENTER, -1);
		aRects[i].VSplitLeft(20.0f, 0, &aRects[i]);
		aRects[i].HSplitTop(2.5f, 0, &aRects[i]);

		for(int s = 0; s < 3; s++)
		{
			//CUIRect Text;
			//MainView.HSplitTop(19.0f, &Button, &MainView);
			//Button.VMargin(15.0f, &Button);
			//Button.VSplitLeft(100.0f, &Text, &Button);
			////Button.VSplitRight(5.0f, &Button, 0);
			//Button.HSplitTop(4.0f, 0, &Button);

			aRects[i].HSplitTop(20.0f, &Label, &aRects[i]);
			Label.VSplitLeft(100.0f, &Label, &Button);
			Button.HMargin(2.0f, &Button);

			float k = (*pColorSlider[i][s]) / 255.0f;
			k = DoScrollbarH(pColorSlider[i][s], &Button, k);
			*pColorSlider[i][s] = (int)(k*255.0f);
			UI()->DoLabel(&Label, paLabels[s], 15.0f, CUI::ALIGN_CENTER, -1);
		}
	}

	GameLeft.HSplitTop(5.0f, &Button, &GameLeft);
	GameLeft.HSplitTop(20.0f, &Button, &GameLeft);
	Button.VSplitLeft(190.0f, &Label, &Button);
	Button.HMargin(2.0f, &Button);
	UI()->DoLabel(&Label, Localize("Overlay entities"), 14.0f, CUI::ALIGN_LEFT, -1);
	Config()->m_ClOverlayEntities = (int)(DoScrollbarH(&Config()->m_ClOverlayEntities, &Button, Config()->m_ClOverlayEntities/100.0f)*100.0f);
	GameLeft.HSplitTop(20.0f, 0, &GameLeft);

	// reset button
	Spacing = 3.0f;
	float ButtonWidth = (BottomView.w/6.0f)-(Spacing*5.0)/6.0f;

	BottomView.VSplitRight(ButtonWidth, 0, &BottomView);
	RenderTools()->DrawUIRect4(&BottomView, vec4(0.0f, 0.0f, 0.0f, Config()->m_ClMenuAlpha/100.0f), vec4(0.0f, 0.0f, 0.0f, Config()->m_ClMenuAlpha/100.0f), vec4(0.0f, 0.0f, 0.0f, 0.0f), vec4(0.0f, 0.0f, 0.0f, 0.0f), CUI::CORNER_T, 5.0f);

	BottomView.HSplitTop(25.0f, &BottomView, 0);
	Button = BottomView;
	static CButtonContainer s_ResetButton;
	if(DoButton_Menu(&s_ResetButton, Localize("Reset"), 0, &Button))
	{
		Config()->m_ClShowAdmins = 1;
		Config()->m_ClShowSilentMessages = 1;
		Config()->m_ClTextEntities = 1;
		Config()->m_ClClientChatCommands = 0;
		Config()->m_ClOldGunPosition = 0;
		Config()->m_ClOldChatSounds = 0;
		Config()->m_ClClientRecognition = 0;
		Config()->m_ClOverlayEntities = 0;
	}
}
