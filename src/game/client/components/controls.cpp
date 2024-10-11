/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <base/math.h>

#include <engine/shared/config.h>

#include <game/collision.h>
#include <game/client/gameclient.h>
#include <game/client/component.h>
#include <game/client/components/chat.h>
#include <game/client/components/menus.h>
#include <game/client/components/scoreboard.h>

#include "controls.h"

CControls::CControls()
{
	mem_zero(&m_LastData, sizeof(m_LastData));
	m_LastDummy = 0;
	m_OtherFire = 0;
}

void CControls::OnReset()
{
	ResetInput(0);
	ResetInput(1);
}

void CControls::ResetInput(int Dummy)
{
	m_LastData[Dummy].m_Direction = 0;
	//m_LastData[Dummy].m_Hook = 0;
	// simulate releasing the fire button
	if((m_LastData[Dummy].m_Fire&1) != 0)
		m_LastData[Dummy].m_Fire++;
	m_LastData[Dummy].m_Fire &= INPUT_STATE_MASK;
	m_LastData[Dummy].m_Jump = 0;
	m_InputData[Dummy] = m_LastData[Dummy];

	m_InputDirectionLeft[Dummy] = 0;
	m_InputDirectionRight[Dummy] = 0;
}

void CControls::OnRelease()
{
	OnReset();
}

void CControls::OnPlayerDeath()
{
	if(!m_pClient->m_Snap.m_pGameDataRace || !(m_pClient->m_Snap.m_pGameDataRace->m_RaceFlags&RACEFLAG_KEEP_WANTED_WEAPON))
		m_LastData[m_pClient->Config()->m_ClDummy].m_WantedWeapon = m_InputData[m_pClient->Config()->m_ClDummy].m_WantedWeapon = 0;
}

struct CInputState
{
	CControls *m_pControls;
	int *m_pVariable1;
	int *m_pVariable2;
};

static void ConKeyInputState(IConsole::IResult *pResult, void *pUserData)
{
	CInputState *pState = (CInputState *)pUserData;
	if (pState->m_pControls->GameClient()->Config()->m_ClDummy)
		*pState->m_pVariable2 = pResult->GetInteger(0);
	else
		*pState->m_pVariable1 = pResult->GetInteger(0);
}

static void ConKeyInputCounter(IConsole::IResult *pResult, void *pUserData)
{
	CInputState *pState = (CInputState *)pUserData;
	int *v;
	if (pState->m_pControls->GameClient()->Config()->m_ClDummy)
		v = pState->m_pVariable2;
	else
		v = pState->m_pVariable1;

	if(((*v)&1) != pResult->GetInteger(0))
		(*v)++;
	*v &= INPUT_STATE_MASK;
}

struct CInputSet
{
	CControls *m_pControls;
	int *m_pVariable1;
	int *m_pVariable2;
	int m_Value;
};

static void ConKeyInputSet(IConsole::IResult *pResult, void *pUserData)
{
	CInputSet *pSet = (CInputSet *)pUserData;
	if(pResult->GetInteger(0))
	{
		if (pSet->m_pControls->GameClient()->Config()->m_ClDummy)
			*pSet->m_pVariable2 = pSet->m_Value;
		else
			*pSet->m_pVariable1 = pSet->m_Value;
	}
}

static void ConKeyInputNextPrevWeapon(IConsole::IResult *pResult, void *pUserData)
{
	CInputSet *pSet = (CInputSet *)pUserData;
	ConKeyInputCounter(pResult, pSet);
	pSet->m_pControls->m_InputData[pSet->m_pControls->GameClient()->Config()->m_ClDummy].m_WantedWeapon = 0;
}

void CControls::OnConsoleInit()
{
	// game commands
	{ static CInputState s_State = {this, &m_InputDirectionLeft[0], &m_InputDirectionLeft[1]}; Console()->Register("+left", "", CFGFLAG_CLIENT, ConKeyInputState, (void *)&s_State, "Move left"); }
	{ static CInputState s_State = {this, &m_InputDirectionRight[0], &m_InputDirectionRight[1]}; Console()->Register("+right", "", CFGFLAG_CLIENT, ConKeyInputState, (void *)&s_State, "Move right"); }
	{ static CInputState s_State = {this, &m_InputData[0].m_Jump, &m_InputData[1].m_Jump}; Console()->Register("+jump", "", CFGFLAG_CLIENT, ConKeyInputState, (void *)&s_State, "Jump"); }
	{ static CInputState s_State = {this, &m_InputData[0].m_Hook, &m_InputData[1].m_Hook}; Console()->Register("+hook", "", CFGFLAG_CLIENT, ConKeyInputState, (void *)&s_State, "Hook"); }
	{ static CInputState s_State = {this, &m_InputData[0].m_Fire, &m_InputData[1].m_Fire}; Console()->Register("+fire", "", CFGFLAG_CLIENT, ConKeyInputCounter, (void *)&s_State, "Fire"); }
	{ static CInputState s_State = {this, &m_ShowHookColl[0], &m_ShowHookColl[1]}; Console()->Register("+showhookcoll", "", CFGFLAG_CLIENT, ConKeyInputState, (void *)&s_State, "Show Hook Collision"); }

	{ static CInputSet s_Set = {this, &m_InputData[0].m_WantedWeapon, &m_InputData[1].m_WantedWeapon, 1}; Console()->Register("+weapon1", "", CFGFLAG_CLIENT, ConKeyInputSet, (void *)&s_Set, "Switch to hammer"); }
	{ static CInputSet s_Set = {this, &m_InputData[0].m_WantedWeapon, &m_InputData[1].m_WantedWeapon, 2}; Console()->Register("+weapon2", "", CFGFLAG_CLIENT, ConKeyInputSet, (void *)&s_Set, "Switch to gun"); }
	{ static CInputSet s_Set = {this, &m_InputData[0].m_WantedWeapon, &m_InputData[1].m_WantedWeapon, 3}; Console()->Register("+weapon3", "", CFGFLAG_CLIENT, ConKeyInputSet, (void *)&s_Set, "Switch to shotgun"); }
	{ static CInputSet s_Set = {this, &m_InputData[0].m_WantedWeapon, &m_InputData[1].m_WantedWeapon, 4}; Console()->Register("+weapon4", "", CFGFLAG_CLIENT, ConKeyInputSet, (void *)&s_Set, "Switch to grenade"); }
	{ static CInputSet s_Set = {this, &m_InputData[0].m_WantedWeapon, &m_InputData[1].m_WantedWeapon, 5}; Console()->Register("+weapon5", "", CFGFLAG_CLIENT, ConKeyInputSet, (void *)&s_Set, "Switch to rifle"); }

	{ static CInputSet s_Set = {this, &m_InputData[0].m_NextWeapon, &m_InputData[1].m_NextWeapon, 0}; Console()->Register("+nextweapon", "", CFGFLAG_CLIENT, ConKeyInputNextPrevWeapon, (void *)&s_Set, "Switch to next weapon"); }
	{ static CInputSet s_Set = {this, &m_InputData[0].m_PrevWeapon, &m_InputData[1].m_PrevWeapon, 0}; Console()->Register("+prevweapon", "", CFGFLAG_CLIENT, ConKeyInputNextPrevWeapon, (void *)&s_Set, "Switch to previous weapon"); }
}

void CControls::OnMessage(int Msg, void *pRawMsg)
{
	if(Msg == NETMSGTYPE_SV_WEAPONPICKUP)
	{
		CNetMsg_Sv_WeaponPickup *pMsg = (CNetMsg_Sv_WeaponPickup *)pRawMsg;
		if(m_pClient->Config()->m_ClAutoswitchWeapons)
			m_InputData[m_pClient->Config()->m_ClDummy].m_WantedWeapon = pMsg->m_Weapon+1;
	}
}

int CControls::SnapInput(int *pData)
{
	static int64 LastSendTime = 0;
	bool Send = false;

	// update player state
	if(m_pClient->m_pChat->IsActive())
		m_InputData[m_pClient->Config()->m_ClDummy].m_PlayerFlags = PLAYERFLAG_CHATTING;
	else
		m_InputData[m_pClient->Config()->m_ClDummy].m_PlayerFlags = 0;

	if(m_pClient->m_pScoreboard->IsActive())
		m_InputData[m_pClient->Config()->m_ClDummy].m_PlayerFlags |= PLAYERFLAG_SCOREBOARD;

	if(m_LastData[m_pClient->Config()->m_ClDummy].m_PlayerFlags != m_InputData[m_pClient->Config()->m_ClDummy].m_PlayerFlags)
		Send = true;

	m_LastData[m_pClient->Config()->m_ClDummy].m_PlayerFlags = m_InputData[m_pClient->Config()->m_ClDummy].m_PlayerFlags;

	// we freeze the input if chat or menu is activated
	if(m_pClient->m_pChat->IsActive() || m_pClient->m_pMenus->IsActive())
	{
		ResetInput(m_pClient->Config()->m_ClDummy);

		mem_copy(pData, &m_InputData[m_pClient->Config()->m_ClDummy], sizeof(m_InputData[0]));

		// send once a second just to be sure
		if(time_get() > LastSendTime + time_freq())
			Send = true;
	}
	else
	{
		m_InputData[m_pClient->Config()->m_ClDummy].m_TargetX = (int)m_MousePos[m_pClient->Config()->m_ClDummy].x;
		m_InputData[m_pClient->Config()->m_ClDummy].m_TargetY = (int)m_MousePos[m_pClient->Config()->m_ClDummy].y;
		if(!m_InputData[m_pClient->Config()->m_ClDummy].m_TargetX && !m_InputData[m_pClient->Config()->m_ClDummy].m_TargetY)
		{
			m_InputData[m_pClient->Config()->m_ClDummy].m_TargetX = 1;
			m_MousePos[m_pClient->Config()->m_ClDummy].x = 1;
		}

		// set direction
		m_InputData[m_pClient->Config()->m_ClDummy].m_Direction = 0;
		if(m_InputDirectionLeft[m_pClient->Config()->m_ClDummy] && !m_InputDirectionRight[m_pClient->Config()->m_ClDummy])
			m_InputData[m_pClient->Config()->m_ClDummy].m_Direction = -1;
		if(!m_InputDirectionLeft[m_pClient->Config()->m_ClDummy] && m_InputDirectionRight[m_pClient->Config()->m_ClDummy])
			m_InputData[m_pClient->Config()->m_ClDummy].m_Direction = 1;

		// dummy copy moves
		if(m_pClient->Config()->m_ClDummyCopyMoves)
		{
			CNetObj_PlayerInput *pDummyInput = &m_pClient->m_DummyInput;
			pDummyInput->m_Direction = m_InputData[m_pClient->Config()->m_ClDummy].m_Direction;
			pDummyInput->m_Hook = m_InputData[m_pClient->Config()->m_ClDummy].m_Hook;
			pDummyInput->m_Jump = m_InputData[m_pClient->Config()->m_ClDummy].m_Jump;
			pDummyInput->m_PlayerFlags = m_InputData[m_pClient->Config()->m_ClDummy].m_PlayerFlags;
			pDummyInput->m_TargetX = m_InputData[m_pClient->Config()->m_ClDummy].m_TargetX;
			pDummyInput->m_TargetY = m_InputData[m_pClient->Config()->m_ClDummy].m_TargetY;
			pDummyInput->m_WantedWeapon = m_InputData[m_pClient->Config()->m_ClDummy].m_WantedWeapon;

			pDummyInput->m_Fire += m_InputData[m_pClient->Config()->m_ClDummy].m_Fire - m_LastData[m_pClient->Config()->m_ClDummy].m_Fire;
			pDummyInput->m_NextWeapon += m_InputData[m_pClient->Config()->m_ClDummy].m_NextWeapon - m_LastData[m_pClient->Config()->m_ClDummy].m_NextWeapon;
			pDummyInput->m_PrevWeapon += m_InputData[m_pClient->Config()->m_ClDummy].m_PrevWeapon - m_LastData[m_pClient->Config()->m_ClDummy].m_PrevWeapon;

			m_InputData[!m_pClient->Config()->m_ClDummy] = *pDummyInput;
		}
		
		if(m_pClient->Config()->m_ClDummyControl){
			CNetObj_PlayerInput *pDummyInput = &m_pClient->m_DummyInput;
			pDummyInput->m_Jump = m_pClient->Config()->m_ClDummyJump;
			pDummyInput->m_Fire = m_pClient->Config()->m_ClDummyFire;
			pDummyInput->m_Hook = m_pClient->Config()->m_ClDummyHook;
		}

		// stress testing
		if(m_pClient->Config()->m_DbgStress)
		{
			float t = Client()->LocalTime();
			mem_zero(&m_InputData[m_pClient->Config()->m_ClDummy], sizeof(m_InputData[m_pClient->Config()->m_ClDummy]));

			m_InputData[m_pClient->Config()->m_ClDummy].m_Direction = ((int)t/2)%3-1;
			m_InputData[m_pClient->Config()->m_ClDummy].m_Jump = ((int)t)&1;
			m_InputData[m_pClient->Config()->m_ClDummy].m_Fire = ((int)(t*10));
			m_InputData[m_pClient->Config()->m_ClDummy].m_Hook = ((int)(t*2))&1;
			m_InputData[m_pClient->Config()->m_ClDummy].m_WantedWeapon = ((int)t)%NUM_WEAPONS;
			m_InputData[m_pClient->Config()->m_ClDummy].m_TargetX = (int)(sinf(t*3)*100.0f);
			m_InputData[m_pClient->Config()->m_ClDummy].m_TargetY = (int)(cosf(t*3)*100.0f);
		}

		if (m_pClient->Config()->m_ClDummy != m_LastDummy)
		{
			int tmp = m_OtherFire;
			m_OtherFire = m_InputData[m_pClient->Config()->m_ClDummy].m_Fire;
			m_InputData[m_pClient->Config()->m_ClDummy].m_Fire = tmp;
			m_LastDummy = m_pClient->Config()->m_ClDummy;
		}

		// check if we need to send input
		if(m_InputData[m_pClient->Config()->m_ClDummy].m_Direction != m_LastData[m_pClient->Config()->m_ClDummy].m_Direction) Send = true;
		else if(m_InputData[m_pClient->Config()->m_ClDummy].m_Jump != m_LastData[m_pClient->Config()->m_ClDummy].m_Jump) Send = true;
		else if(m_InputData[m_pClient->Config()->m_ClDummy].m_Fire != m_LastData[m_pClient->Config()->m_ClDummy].m_Fire) Send = true;
		else if(m_InputData[m_pClient->Config()->m_ClDummy].m_Hook != m_LastData[m_pClient->Config()->m_ClDummy].m_Hook) Send = true;
		else if(m_InputData[m_pClient->Config()->m_ClDummy].m_WantedWeapon != m_LastData[m_pClient->Config()->m_ClDummy].m_WantedWeapon) Send = true;
		else if(m_InputData[m_pClient->Config()->m_ClDummy].m_NextWeapon != m_LastData[m_pClient->Config()->m_ClDummy].m_NextWeapon) Send = true;
		else if(m_InputData[m_pClient->Config()->m_ClDummy].m_PrevWeapon != m_LastData[m_pClient->Config()->m_ClDummy].m_PrevWeapon) Send = true;

		// send at at least 10hz
		if(time_get() > LastSendTime + time_freq()/25)
			Send = true;
	}

	// copy and return size
	m_LastData[m_pClient->Config()->m_ClDummy] = m_InputData[m_pClient->Config()->m_ClDummy];

	if(!Send)
		return 0;

	LastSendTime = time_get();
	mem_copy(pData, &m_InputData[m_pClient->Config()->m_ClDummy], sizeof(m_InputData[0]));
	return sizeof(m_InputData[0]);
}

void CControls::OnRender()
{
	ClampMousePos();

	// update target pos
	if(m_pClient->m_Snap.m_pGameData && !m_pClient->m_Snap.m_SpecInfo.m_Active)
		m_TargetPos[m_pClient->Config()->m_ClDummy] = m_pClient->m_LocalCharacterPos + m_MousePos[m_pClient->Config()->m_ClDummy];
	else if(m_pClient->m_Snap.m_SpecInfo.m_Active && m_pClient->m_Snap.m_SpecInfo.m_UsePosition)
		m_TargetPos[m_pClient->Config()->m_ClDummy] = m_pClient->m_Snap.m_SpecInfo.m_Position + m_MousePos[m_pClient->Config()->m_ClDummy];
	else
		m_TargetPos[m_pClient->Config()->m_ClDummy] = m_MousePos[m_pClient->Config()->m_ClDummy];
}

bool CControls::OnCursorMove(float x, float y, int CursorType)
{
	if(m_pClient->IsWorldPaused() || (m_pClient->m_Snap.m_SpecInfo.m_Active && m_pClient->m_pChat->IsActive()))
		return false;

	if(CursorType == IInput::CURSOR_JOYSTICK
		&& Config()->m_JoystickAbsolute
		&& m_pClient->m_Snap.m_pGameData
		&& !m_pClient->m_Snap.m_SpecInfo.m_Active)
	{
		float absX = 0.0f, absY = 0.0f;
		if(Input()->JoystickAbsolute(&absX, &absY))
			m_MousePos[m_pClient->Config()->m_ClDummy] = vec2(absX, absY) * GetMaxMouseDistance();
		return true;
	}

	float Factor = 1.0f;
	switch(CursorType)
	{
		case IInput::CURSOR_MOUSE:
			Factor = Config()->m_InpMousesens/100.0f;
			break;
		case IInput::CURSOR_JOYSTICK:
			Factor = Config()->m_JoystickSens/100.0f;
			break;
	}

	m_MousePos[m_pClient->Config()->m_ClDummy] += vec2(x, y) * Factor;
	return true;
}

void CControls::ClampMousePos()
{
	if(m_pClient->m_Snap.m_SpecInfo.m_Active && !m_pClient->m_Snap.m_SpecInfo.m_UsePosition)
	{
		m_MousePos[m_pClient->Config()->m_ClDummy].x = clamp(m_MousePos[m_pClient->Config()->m_ClDummy].x, 200.0f, Collision()->GetWidth()*32-200.0f);
		m_MousePos[m_pClient->Config()->m_ClDummy].y = clamp(m_MousePos[m_pClient->Config()->m_ClDummy].y, 200.0f, Collision()->GetHeight()*32-200.0f);
	}
	else
	{
		const float MouseMax = GetMaxMouseDistance();
		if(length(m_MousePos[m_pClient->Config()->m_ClDummy]) > MouseMax)
			m_MousePos[m_pClient->Config()->m_ClDummy] = normalize(m_MousePos[m_pClient->Config()->m_ClDummy])*MouseMax;
	}
}

float CControls::GetMaxMouseDistance() const
{
	if(Config()->m_ClDynamicCamera)
	{
		float CameraMaxDistance = 200.0f;
		float FollowFactor = Config()->m_ClMouseFollowfactor/100.0f;
		return min(CameraMaxDistance/FollowFactor + Config()->m_ClMouseDeadzone, (float)Config()->m_ClMouseMaxDistanceDynamic);
	}
	else
		return (float)Config()->m_ClMouseMaxDistanceStatic;
}
