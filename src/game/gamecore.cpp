/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "gamecore.h"

#include <engine/shared/config.h>

const char *CTuningParams::m_apNames[] =
{
	#define MACRO_TUNING_PARAM(Name,ScriptName,Value) #ScriptName,
	#include "tuning.h"
	#undef MACRO_TUNING_PARAM
};


bool CTuningParams::Set(int Index, float Value)
{
	if(Index < 0 || Index >= Num())
		return false;
	((CTuneParam *)this)[Index] = Value;
	return true;
}

bool CTuningParams::Get(int Index, float *pValue) const
{
	if(Index < 0 || Index >= Num())
		return false;
	*pValue = (float)((CTuneParam *)this)[Index];
	return true;
}

bool CTuningParams::Set(const char *pName, float Value)
{
	for(int i = 0; i < Num(); i++)
		if(str_comp_nocase(pName, m_apNames[i]) == 0)
			return Set(i, Value);
	return false;
}

bool CTuningParams::Get(const char *pName, float *pValue) const
{
	for(int i = 0; i < Num(); i++)
		if(str_comp_nocase(pName, m_apNames[i]) == 0)
			return Get(i, pValue);

	return false;
}

float HermiteBasis1(float v)
{
	return 2*v*v*v - 3*v*v+1;
}

float VelocityRamp(float Value, float Start, float Range, float Curvature)
{
	if(Value < Start)
		return 1.0f;
	return 1.0f/powf(Curvature, (Value-Start)/Range);
}

const float CCharacterCore::PHYS_SIZE = 28.0f;

void CCharacterCore::Init(CConfig *pConfig, CWorldCore *pWorld, CCollision *pCollision)
{
	m_pWorld = pWorld;
	m_pCollision = pCollision;

	// DDRace

	m_pConfig = pConfig;
	m_pTeleOuts = NULL;
}

void CCharacterCore::Reset()
{
	m_Pos = vec2(0,0);
	m_Vel = vec2(0,0);
	m_NewHook = false;
	m_HookDragVel = vec2(0,0);
	m_HookPos = vec2(0,0);
	m_HookDir = vec2(0,0);
	m_HookTick = 0;
	m_HookState = HOOK_IDLE;
	m_HookedPlayer = -1;
	m_Jumped = 0;
	m_TriggeredEvents = 0;
	m_Death = false;
}

void CCharacterCore::Tick(bool UseInput)
{
	if(m_pConfig->m_ClDDracePrediction)
		m_MoveRestrictions = m_pCollision->GetMoveRestrictions(UseInput ? IsSwitchActiveCb : 0, this, m_Pos);
	m_TriggeredEvents = 0;

	// get ground state
	bool Grounded = false;
	if(m_pCollision->CheckPoint(m_Pos.x+PHYS_SIZE/2, m_Pos.y+PHYS_SIZE/2+5))
		Grounded = true;
	if(m_pCollision->CheckPoint(m_Pos.x-PHYS_SIZE/2, m_Pos.y+PHYS_SIZE/2+5))
		Grounded = true;

	vec2 TargetDirection = normalize(vec2(m_Input.m_TargetX, m_Input.m_TargetY));

	m_Vel.y += m_pWorld->m_Tuning[m_pConfig->m_ClDummy].m_Gravity;

	float MaxSpeed = Grounded ? m_pWorld->m_Tuning[m_pConfig->m_ClDummy].m_GroundControlSpeed : m_pWorld->m_Tuning[m_pConfig->m_ClDummy].m_AirControlSpeed;
	float Accel = Grounded ? m_pWorld->m_Tuning[m_pConfig->m_ClDummy].m_GroundControlAccel : m_pWorld->m_Tuning[m_pConfig->m_ClDummy].m_AirControlAccel;
	float Friction = Grounded ? m_pWorld->m_Tuning[m_pConfig->m_ClDummy].m_GroundFriction : m_pWorld->m_Tuning[m_pConfig->m_ClDummy].m_AirFriction;

	// handle input
	if(UseInput)
	{
		m_Direction = m_Input.m_Direction;
		m_Angle = (int)(angle(vec2(m_Input.m_TargetX, m_Input.m_TargetY))*256.0f);

		// handle jump
		if(m_Input.m_Jump)
		{
			if(!(m_Jumped&1))
			{
				if(Grounded)
				{
					m_TriggeredEvents |= COREEVENTFLAG_GROUND_JUMP;
					m_Vel.y = -m_pWorld->m_Tuning[m_pConfig->m_ClDummy].m_GroundJumpImpulse;
					m_Jumped |= 1;
				}
				else if(!(m_Jumped&2))
				{
					m_TriggeredEvents |= COREEVENTFLAG_AIR_JUMP;
					m_Vel.y = -m_pWorld->m_Tuning[m_pConfig->m_ClDummy].m_AirJumpImpulse;
					m_Jumped |= 3;
				}
			}
		}
		else
			m_Jumped &= ~1;

		// handle hook
		if(m_Input.m_Hook)
		{
			if(m_HookState == HOOK_IDLE)
			{
				m_HookState = HOOK_FLYING;
				m_HookPos = m_Pos+TargetDirection*PHYS_SIZE*1.5f;
				m_HookDir = TargetDirection;
				m_HookedPlayer = -1;
				m_HookTick = 0;
				//m_TriggeredEvents |= COREEVENTFLAG_HOOK_LAUNCH;
			}
		}
		else
		{
			m_HookedPlayer = -1;
			m_HookState = HOOK_IDLE;
			m_HookPos = m_Pos;
		}
	}

	// add the speed modification according to players wanted direction
	if(m_Direction < 0)
		m_Vel.x = SaturatedAdd(-MaxSpeed, MaxSpeed, m_Vel.x, -Accel);
	if(m_Direction > 0)
		m_Vel.x = SaturatedAdd(-MaxSpeed, MaxSpeed, m_Vel.x, Accel);
	if(m_Direction == 0)
		m_Vel.x *= Friction;

	// handle jumping
	// 1 bit = to keep track if a jump has been made on this input
	// 2 bit = to keep track if a air-jump has been made
	if(Grounded)
		m_Jumped &= ~2;

	// do hook
	if(m_HookState == HOOK_IDLE)
	{
		m_HookedPlayer = -1;
		m_HookState = HOOK_IDLE;
		m_HookPos = m_Pos;
	}
	else if(m_HookState >= HOOK_RETRACT_START && m_HookState < HOOK_RETRACT_END)
	{
		m_HookState++;
	}
	else if(m_HookState == HOOK_RETRACT_END)
	{
		m_HookState = HOOK_RETRACTED;
		//m_TriggeredEvents |= COREEVENTFLAG_HOOK_RETRACT;
	}
	else if(m_HookState == HOOK_FLYING)
	{
		vec2 NewPos = m_HookPos+m_HookDir*m_pWorld->m_Tuning[m_pConfig->m_ClDummy].m_HookFireSpeed;
		if((!m_NewHook && distance(m_Pos, NewPos) > m_pWorld->m_Tuning[m_pConfig->m_ClDummy].m_HookLength)
			|| (m_NewHook && distance(m_HookTeleBase, NewPos) > m_pWorld->m_Tuning[m_pConfig->m_ClDummy].m_HookLength))
		{
			m_HookState = HOOK_RETRACT_START;
			NewPos = m_Pos + normalize(NewPos-m_Pos) * m_pWorld->m_Tuning[m_pConfig->m_ClDummy].m_HookLength;
		}

		// make sure that the hook doesn't go though the ground
		bool GoingToHitGround = false;
		bool GoingToRetract = false;
		bool GoingThroughTele = false;
		int teleNr = 0;
		int Hit;
		if(m_pConfig->m_ClDDracePrediction)
			Hit = m_pCollision->IntersectLineTeleHook(m_HookPos, NewPos, &NewPos, 0, &teleNr);
		else
			Hit = m_pCollision->IntersectLine(m_HookPos, NewPos, &NewPos, 0);

		//m_NewHook = false;

		if(Hit)
		{
			if(Hit == TILE_NOHOOK)
				GoingToRetract = true;
			else if (Hit == TILE_TELEINHOOK)
				GoingThroughTele = true;
			else
				GoingToHitGround = true;
		}

		// Check against other players first
		if(m_pWorld && m_pWorld->m_Tuning[m_pConfig->m_ClDummy].m_PlayerHooking)
		{
			float Distance = 0.0f;
			for(int i = 0; i < MAX_CLIENTS; i++)
			{
				CCharacterCore *pCharCore = m_pWorld->m_apCharacters[i];
				if(!pCharCore || pCharCore == this)
					continue;

				vec2 ClosestPoint = closest_point_on_line(m_HookPos, NewPos, pCharCore->m_Pos);
				if(distance(pCharCore->m_Pos, ClosestPoint) < PHYS_SIZE+2.0f)
				{
					if (m_HookedPlayer == -1 || distance(m_HookPos, pCharCore->m_Pos) < Distance)
					{
						m_TriggeredEvents |= COREEVENTFLAG_HOOK_ATTACH_PLAYER;
						m_HookState = HOOK_GRABBED;
						m_HookedPlayer = i;
						Distance = distance(m_HookPos, pCharCore->m_Pos);
					}
				}
			}
		}

		if(m_HookState == HOOK_FLYING)
		{
			// check against ground
			if(GoingToHitGround)
			{
				m_TriggeredEvents |= COREEVENTFLAG_HOOK_ATTACH_GROUND;
				m_HookState = HOOK_GRABBED;
			}
			else if(GoingToRetract)
			{
				m_TriggeredEvents |= COREEVENTFLAG_HOOK_HIT_NOHOOK;
				m_HookState = HOOK_RETRACT_START;
			}

			if(GoingThroughTele && m_pTeleOuts && m_pTeleOuts->size() && (*m_pTeleOuts)[teleNr-1].size())
			{
				m_TriggeredEvents = 0;
				m_HookedPlayer = -1;

				m_NewHook = true;
				int Num = (*m_pTeleOuts)[teleNr-1].size();
				m_HookPos = (*m_pTeleOuts)[teleNr-1][(Num==1)?0:rand() % Num]+TargetDirection*PHYS_SIZE*1.5f;
				m_HookDir = TargetDirection;
				m_HookTeleBase = m_HookPos;
			}
			else
			{
				m_HookPos = NewPos;
			}
		}
	}

	if(m_HookState == HOOK_GRABBED)
	{
		if(m_HookedPlayer != -1)
		{
			CCharacterCore *pCharCore = m_pWorld->m_apCharacters[m_HookedPlayer];
			if(pCharCore)
				m_HookPos = pCharCore->m_Pos;
			else
			{
				// release hook
				m_HookedPlayer = -1;
				m_HookState = HOOK_RETRACTED;
				m_HookPos = m_Pos;
			}

			// keep players hooked for a max of 1.5sec
			//if(Server()->Tick() > hook_tick+(Server()->TickSpeed()*3)/2)
				//release_hooked();
		}

		// don't do this hook rutine when we are hook to a player
		if(m_HookedPlayer == -1 && distance(m_HookPos, m_Pos) > 46.0f)
		{
			vec2 HookVel = normalize(m_HookPos-m_Pos)*m_pWorld->m_Tuning[m_pConfig->m_ClDummy].m_HookDragAccel;
			// the hook as more power to drag you up then down.
			// this makes it easier to get on top of an platform
			if(HookVel.y > 0)
				HookVel.y *= 0.3f;

			// the hook will boost it's power if the player wants to move
			// in that direction. otherwise it will dampen everything abit
			if((HookVel.x < 0 && m_Direction < 0) || (HookVel.x > 0 && m_Direction > 0))
				HookVel.x *= 0.95f;
			else
				HookVel.x *= 0.75f;

			vec2 NewVel = m_Vel+HookVel;

			// check if we are under the legal limit for the hook
			if(length(NewVel) < m_pWorld->m_Tuning[m_pConfig->m_ClDummy].m_HookDragSpeed || length(NewVel) < length(m_Vel))
				m_Vel = NewVel; // no problem. apply

		}

		// release hook (max hook time is 1.25
		m_HookTick++;
		if(m_HookedPlayer != -1 && (m_HookTick > SERVER_TICK_SPEED+SERVER_TICK_SPEED/5 || !m_pWorld->m_apCharacters[m_HookedPlayer]))
		{
			m_HookedPlayer = -1;
			m_HookState = HOOK_RETRACTED;
			m_HookPos = m_Pos;
		}
	}

	if(m_pWorld)
	{
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			CCharacterCore *pCharCore = m_pWorld->m_apCharacters[i];
			if(!pCharCore)
				continue;

			//player *p = (player*)ent;
			if(pCharCore == this) // || !(p->flags&FLAG_ALIVE)
				continue; // make sure that we don't nudge our self

			// handle player <-> player collision
			float Distance = distance(m_Pos, pCharCore->m_Pos);
			vec2 Dir = normalize(m_Pos - pCharCore->m_Pos);
			if(m_pWorld->m_Tuning[m_pConfig->m_ClDummy].m_PlayerCollision && Distance < PHYS_SIZE*1.25f && Distance > 0.0f)
			{
				float a = (PHYS_SIZE*1.45f - Distance);
				float Velocity = 0.5f;

				// make sure that we don't add excess force by checking the
				// direction against the current velocity. if not zero.
				if (length(m_Vel) > 0.0001)
					Velocity = 1-(dot(normalize(m_Vel), Dir)+1)/2;

				m_Vel += Dir*a*(Velocity*0.75f);
				m_Vel *= 0.85f;
			}

			// handle hook influence
			if(m_HookedPlayer == i && m_pWorld->m_Tuning[m_pConfig->m_ClDummy].m_PlayerHooking)
			{
				if(Distance > PHYS_SIZE*1.50f) // TODO: fix tweakable variable
				{
					float Accel = m_pWorld->m_Tuning[m_pConfig->m_ClDummy].m_HookDragAccel * (Distance/m_pWorld->m_Tuning[m_pConfig->m_ClDummy].m_HookLength);

					// add force to the hooked player
					pCharCore->m_HookDragVel += Dir*Accel*1.5f;

					// add a little bit force to the guy who has the grip
					m_HookDragVel -= Dir*Accel*0.25f;
				}
			}
		}

		if (m_HookState != HOOK_FLYING)
		{
			m_NewHook = false;
		}
	}
	// TODO: why is this line not in ddnet?
	m_Vel = ClampVel(m_MoveRestrictions, m_Vel);

	// clamp the velocity to something sane
	if(length(m_Vel) > 6000)
		m_Vel = normalize(m_Vel) * 6000;
}

void CCharacterCore::AddDragVelocity()
{
	// Apply hook interaction velocity
	float DragSpeed = m_pWorld->m_Tuning[m_pConfig->m_ClDummy].m_HookDragSpeed;

	m_Vel.x = SaturatedAdd(-DragSpeed, DragSpeed, m_Vel.x, m_HookDragVel.x);
	m_Vel.y = SaturatedAdd(-DragSpeed, DragSpeed, m_Vel.y, m_HookDragVel.y);
}

void CCharacterCore::ResetDragVelocity()
{
	m_HookDragVel = vec2(0,0);
}

void CCharacterCore::Move()
{
	if(!m_pWorld)
		return;

	float RampValue = VelocityRamp(length(m_Vel)*50, m_pWorld->m_Tuning[m_pConfig->m_ClDummy].m_VelrampStart, m_pWorld->m_Tuning[m_pConfig->m_ClDummy].m_VelrampRange, m_pWorld->m_Tuning[m_pConfig->m_ClDummy].m_VelrampCurvature);

	m_Vel.x = m_Vel.x*RampValue;

	vec2 NewPos = m_Pos;
	m_pCollision->MoveBox(&NewPos, &m_Vel, vec2(PHYS_SIZE, PHYS_SIZE), 0, &m_Death);

	m_Vel.x = m_Vel.x*(1.0f/RampValue);

	if(m_pWorld->m_Tuning[m_pConfig->m_ClDummy].m_PlayerCollision)
	{
		// check player collision
		float Distance = distance(m_Pos, NewPos);
		int End = Distance+1;
		vec2 LastPos = m_Pos;
		for(int i = 0; i < End; i++)
		{
			float a = i/Distance;
			vec2 Pos = mix(m_Pos, NewPos, a);
			for(int p = 0; p < MAX_CLIENTS; p++)
			{
				CCharacterCore *pCharCore = m_pWorld->m_apCharacters[p];
				if(!pCharCore || pCharCore == this)
					continue;
				float D = distance(Pos, pCharCore->m_Pos);
				if(D < PHYS_SIZE && D >= 0.0f)
				{
					if(a > 0.0f)
						m_Pos = LastPos;
					else if(distance(NewPos, pCharCore->m_Pos) > D)
						m_Pos = NewPos;
					return;
				}
			}
			LastPos = Pos;
		}
	}

	m_Pos = NewPos;
}

void CCharacterCore::Write(CNetObj_CharacterCore *pObjCore) const
{
	pObjCore->m_X = round_to_int(m_Pos.x);
	pObjCore->m_Y = round_to_int(m_Pos.y);

	pObjCore->m_VelX = round_to_int(m_Vel.x*256.0f);
	pObjCore->m_VelY = round_to_int(m_Vel.y*256.0f);
	pObjCore->m_HookState = m_HookState;
	pObjCore->m_HookTick = m_HookTick;
	pObjCore->m_HookX = round_to_int(m_HookPos.x);
	pObjCore->m_HookY = round_to_int(m_HookPos.y);
	pObjCore->m_HookDx = round_to_int(m_HookDir.x*256.0f);
	pObjCore->m_HookDy = round_to_int(m_HookDir.y*256.0f);
	pObjCore->m_HookedPlayer = m_HookedPlayer;
	pObjCore->m_Jumped = m_Jumped;
	pObjCore->m_Direction = m_Direction;
	pObjCore->m_Angle = m_Angle;
}

void CCharacterCore::Read(const CNetObj_CharacterCore *pObjCore)
{
	m_Pos.x = pObjCore->m_X;
	m_Pos.y = pObjCore->m_Y;
	m_Vel.x = pObjCore->m_VelX/256.0f;
	m_Vel.y = pObjCore->m_VelY/256.0f;
	m_HookState = pObjCore->m_HookState;
	m_HookTick = pObjCore->m_HookTick;
	m_HookPos.x = pObjCore->m_HookX;
	m_HookPos.y = pObjCore->m_HookY;
	m_HookDir.x = pObjCore->m_HookDx/256.0f;
	m_HookDir.y = pObjCore->m_HookDy/256.0f;
	m_HookedPlayer = pObjCore->m_HookedPlayer;
	m_Jumped = pObjCore->m_Jumped;
	m_Direction = pObjCore->m_Direction;
	m_Angle = pObjCore->m_Angle;
}

void CCharacterCore::Quantize()
{
	CNetObj_CharacterCore Core;
	Write(&Core);
	Read(&Core);
}

// DDRace

bool CCharacterCore::IsSwitchActiveCb(int Number, void *pUser)
{
	/*
	// TODO: team info
	CCharacterCore *pThis = (CCharacterCore *)pUser;
	if(pThis->Collision()->m_pSwitchers)
		if(pThis->m_pTeams->Team(pThis->m_Id) != (pThis->m_pTeams->m_IsDDRace16 ? VANILLA_TEAM_SUPER : TEAM_SUPER))
			return pThis->Collision()->m_pSwitchers[Number].m_Status[pThis->m_pTeams->Team(pThis->m_Id)];
	*/
	return false;
}

vec2 CCharacterCore::LimitVel(vec2 Vel)
{
	return ClampVel(m_MoveRestrictions, Vel);
}

void CCharacterCore::ApplyForce(vec2 Force)
{
	m_Vel = LimitVel(m_Vel + Force);
}
