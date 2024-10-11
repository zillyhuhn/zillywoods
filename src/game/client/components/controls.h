/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_CONTROLS_H
#define GAME_CLIENT_COMPONENTS_CONTROLS_H
#include <base/vmath.h>
#include <game/client/component.h>

class CControls : public CComponent
{
public:
	vec2 m_MousePos[2];
	vec2 m_TargetPos[2];

	CNetObj_PlayerInput m_InputData[2];
	CNetObj_PlayerInput m_LastData[2];
	int m_InputDirectionLeft[2];
	int m_InputDirectionRight[2];
	int m_ShowHookColl[2];
	int m_LastDummy;
	int m_OtherFire;

	CControls();

	virtual void OnReset();
	virtual void OnRelease();
	virtual void OnRender();
	virtual void OnMessage(int MsgType, void *pRawMsg);
	virtual bool OnCursorMove(float x, float y, int CursorType);
	virtual void OnConsoleInit();
	virtual void OnPlayerDeath();

	int SnapInput(int *pData);
	void ClampMousePos();
	void ResetInput(int Dummy);

	class CGameClient *GameClient() const { return m_pClient; }
	float GetMaxMouseDistance() const;
};
#endif
