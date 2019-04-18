#ifndef GAME_SERVER_ENTITIES_WEAPON_H
#define GAME_SERVER_ENTITIES_WEAPON_H

class CWeapon : public CEntity
{
public:
	CWeapon(CGameWorld *pGameWorld, int Weapon, int Lifetime, int Owner, int Direction, int ResponsibleTeam, int Bullets, bool Jetpack = false);

	virtual void Reset();
	virtual void Tick();
	virtual void Snap(int SnappingClient);

	int IsCharacterNear();

	void IsShieldNear();

	void Pickup();

private:
	vec2 m_Vel;

	int m_Owner;
	int m_ResponsibleTeam;
	int m_Type;
	int m_Lifetime;
	int m_Bullets;

	int m_PickupDelay;

	bool m_Jetpack;

	bool m_EreaseWeapon;

	static int const ms_PhysSize = 14;

	// have to define a new ID variable for the bullet
	int m_ID2;

	float m_ProximityRadius;
};

#endif
