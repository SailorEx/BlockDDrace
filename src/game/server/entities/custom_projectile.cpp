/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */
#include <engine/server.h>
#include <engine/config.h>
#include <game/generated/protocol.h>
#include <game/server/gamecontext.h>
#include <game/server/teams.h>
#include <game/server/gamemodes/DDRace.h>
#include "custom_projectile.h"

CCustomProjectile::CCustomProjectile(CGameWorld *pGameWorld, int Owner, vec2 Pos, vec2 Dir, bool Freeze,
		bool Explosive, bool Unfreeze, bool Bloody, bool Ghost, bool Spooky, int Type, float Lifetime, float Accel, float Speed) :
		CEntity(pGameWorld, CGameWorld::ENTTYPE_PROJECTILE)
{
	m_Owner = Owner;
	m_Pos = Pos;
	m_Core = normalize(Dir) * Speed;
	m_Freeze = Freeze;
	m_Explosive = Explosive;
	m_Unfreeze = Unfreeze;
	m_Bloody = Bloody;
	m_Ghost = Ghost;
	m_Spooky = Spooky;
	m_Direction = Dir;
	m_EvalTick = Server()->Tick();
	m_LifeTime = Server()->TickSpeed() * Lifetime;
	m_Type = Type;
	m_Accel = Accel;
	m_PrevPos = m_Pos;
	GameWorld()->InsertEntity(this);
}

bool CCustomProjectile::HitCharacter()
{
	CCharacter* pOwner = GameServer()->GetPlayerChar(m_Owner);
	CCharacter* pHit = GameServer()->m_World.IntersectCharacter(m_PrevPos, m_Pos + m_Core, 6.0f, m_Pos + m_Core, pOwner, m_Owner);
	if (!pHit)
		return false;

	if (pHit->GetPlayer()->GetCID() == m_Owner) // dont hit yourself
		return false;
	if (pHit->m_Passive || pOwner->m_Passive)
		return false;
	if (pHit->Team() != pOwner->Team())
		return false;

	if (m_Spooky)
	{
		pHit->SetEmote(3, Server()->Tick() + 2 * Server()->TickSpeed()); // eyeemote surprise
		GameServer()->SendEmoticon(pHit->GetPlayer()->GetCID(), 7);		//emoticon ghost
	}

	if (m_Bloody)
		GameServer()->CreateDeath(m_Pos, pHit->GetPlayer()->GetCID());

	if (m_Freeze)
		pHit->Freeze();

	if (m_Unfreeze)
		pHit->UnFreeze();

	if (m_Explosive)
	{
		GameServer()->CreateExplosion(m_Pos, m_Owner, m_Type, m_Owner == -1, (pHit ? pHit->Team() : -1), m_TeamMask);
		GameServer()->CreateSound(m_Pos, SOUND_GRENADE_EXPLODE, m_TeamMask);
	}
	else
		pHit->TakeDamage(m_Direction * max(0.001f, 0.0f), g_pData->m_Weapons.m_aId[GameServer()->GetRealWeapon(m_Type)].m_Damage, m_Owner, m_Type);

	GameServer()->m_World.DestroyEntity(this);
	return true;
}

void CCustomProjectile::Move()
{
	m_Pos += m_Core;
	m_Core *= m_Accel;
}

void CCustomProjectile::Reset()
{
	GameServer()->m_World.DestroyEntity(this);
}

void CCustomProjectile::Tick()
{
	CCharacter* pOwner = GameServer()->GetPlayerChar(m_Owner);
	if (!pOwner)
	{
		Reset();
		return;
	}

	m_TeamMask = pOwner->Teams()->TeamMask(pOwner->Team(), -1, m_Owner);

	m_LifeTime--;
	if (m_LifeTime <= 0)
	{
		Reset();
		return;
	}
	Move();
	HitCharacter();

	bool Res;
	Res = GameServer()->Collision()->IsSolid(m_Pos.x, m_Pos.y);
	if (Res)
	{
		if (m_Explosive)
		{
			GameServer()->CreateExplosion(m_Pos, m_Owner, m_Type, m_Owner == -1, pOwner->Team(), m_TeamMask);
			GameServer()->CreateSound(m_Pos, SOUND_GRENADE_EXPLODE, m_TeamMask);
		}

		if (m_Bloody)
		{
			if (m_IsInsideWall == 1 && Server()->Tick() % 5 == 0)
				GameServer()->CreateDeath(m_PrevPos, m_Owner);
			else
				GameServer()->CreateDeath(m_PrevPos, m_Owner);
		}

		if (m_Ghost && m_IsInsideWall == 0)
			m_IsInsideWall = 1; // enteres the wall, collides the first time

		if (m_IsInsideWall == 2 || !m_Ghost) // collides second time with a wall
		{
			Reset();
			return;
		}
	}
	else if(m_Ghost && m_IsInsideWall == 1)
		m_IsInsideWall = 2; // leaves the wall

	m_PrevPos = m_Pos;
}

void CCustomProjectile::Snap(int SnappingClient)
{
	if (NetworkClipped(SnappingClient))
		return;

	CCharacter* pSnapChar = GameServer()->GetPlayerChar(SnappingClient);
	CCharacter* pOwner = GameServer()->GetPlayerChar(m_Owner);
	if (pOwner && pSnapChar)
	{
		int64_t TeamMask = pOwner->Teams()->TeamMask(pOwner->Team(), -1, m_Owner);
		if (!CmaskIsSet(TeamMask, SnappingClient))
			return;
	}

	if (m_Type == WEAPON_PLASMA_RIFLE)
	{
		CNetObj_Laser *pLaser = static_cast<CNetObj_Laser *>(Server()->SnapNewItem(NETOBJTYPE_LASER, m_ID, sizeof(CNetObj_Laser)));
		if (!pLaser)
			return;

		pLaser->m_X = (int)m_Pos.x;
		pLaser->m_Y = (int)m_Pos.y;
		pLaser->m_FromX = (int)m_Pos.x;
		pLaser->m_FromY = (int)m_Pos.y;
		pLaser->m_StartTick = m_EvalTick;
	}
	else if (m_Type == WEAPON_HEART_GUN)
	{
		CNetObj_Pickup *pPickup = static_cast<CNetObj_Pickup *>(Server()->SnapNewItem(NETOBJTYPE_PICKUP, m_ID, sizeof(CNetObj_Pickup)));
		if (!pPickup)
			return;

		pPickup->m_X = (int)m_Pos.x;
		pPickup->m_Y = (int)m_Pos.y;
		pPickup->m_Type = POWERUP_HEALTH;
	}
}
