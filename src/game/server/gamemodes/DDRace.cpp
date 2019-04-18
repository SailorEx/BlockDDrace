/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */
/* Based on Race mod stuff and tweaked by GreYFoX@GTi and others to fit our DDRace needs. */
#include <engine/server.h>
#include <engine/shared/config.h>
#include <game/mapitems.h>
#include <game/server/entities/character.h>
#include <game/server/player.h>
#include <game/server/gamecontext.h>
#include <game/server/entities/flag.h>
#include "DDRace.h"
#include "gamemode.h"

CGameControllerDDRace::CGameControllerDDRace(class CGameContext *pGameServer) :
		IGameController(pGameServer), m_Teams(pGameServer)
{
	m_apFlags[0] = 0;
	m_apFlags[1] = 0;

	m_GameFlags = GAMEFLAG_FLAGS;

	m_pGameType = g_Config.m_SvTestingCommands ? TEST_NAME : GAME_NAME;

	InitTeleporter();
}

bool CGameControllerDDRace::OnEntity(int Index, vec2 Pos)
{
	int Team = -1;
	if (Index == ENTITY_FLAGSTAND_RED) Team = TEAM_RED;
	if (Index == ENTITY_FLAGSTAND_BLUE) Team = TEAM_BLUE;
	if (Team == -1 || m_apFlags[Team])
		return false;

	CFlag *F = new CFlag(&GameServer()->m_World, Team);
	F->m_StandPos = Pos;
	F->m_Pos = Pos;
	m_apFlags[Team] = F;
	GameServer()->m_World.InsertEntity(F);
	return true;
}

int CGameControllerDDRace::OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int WeaponID)
{
	int HadFlag = 0;

	// drop flags
	for (int i = 0; i < 2; i++)
	{
		CFlag *F = m_apFlags[i];
		if (F && pKiller && pKiller->GetCharacter() && F->m_pCarryingCharacter == pKiller->GetCharacter())
			HadFlag |= 2;
		if (F && F->m_pCarryingCharacter == pVictim)
		{
			if (g_Config.m_SvFlagSounds)
			{
				GameServer()->CreateSoundGlobal(SOUND_CTF_DROP);
			}
			F->m_DropTick = Server()->Tick();
			F->m_pCarryingCharacter = 0;
			F->m_Vel = vec2(0, 0);

			HadFlag |= 1;
		}
	}

	return HadFlag;
}

void CGameControllerDDRace::ChangeFlagOwner(int id, int character)
{
	CFlag *F = m_apFlags[id];
	if ((m_apFlags[0]->m_pCarryingCharacter == GameServer()->GetPlayerChar(character)) || (m_apFlags[1]->m_pCarryingCharacter == GameServer()->GetPlayerChar(character))) {

	}
	else
	{
		F->m_AtStand = 0;

		if (g_Config.m_SvFlagSounds)
		{
			GameServer()->CreateSoundGlobal(SOUND_CTF_DROP);
		}

		if (GameServer()->m_apPlayers[character] && GameServer()->m_apPlayers[character]->GetCharacter())
		{
			F->m_pCarryingCharacter = GameServer()->m_apPlayers[character]->GetCharacter();
			F->m_pCarryingCharacter->GetPlayer()->GetCharacter()->m_FirstFreezeTick = 0;
		}
	}
}

int CGameControllerDDRace::HasFlag(CCharacter *character)
{
	if (!character)
		return -1;

	for (int i = 0; i<2; i++)
	{
		if (!m_apFlags[i])
			continue;

		if (m_apFlags[i]->m_pCarryingCharacter == character)
		{
			return i;
		}
	}
	return -1;
}

void CGameControllerDDRace::DropFlag(int id, int dir)
{
	CFlag *F = m_apFlags[id]; //red=0 blue=1

	if (g_Config.m_SvFlagSounds)
	{
		GameServer()->CreateSoundGlobal(SOUND_CTF_DROP);
	}
	F->m_DropTick = Server()->Tick();
	F->m_DropFreezeTick = Server()->Tick();
	F->m_pLastCarryingCharacter = F->m_pCarryingCharacter;
	F->m_pCarryingCharacter = 0;
	F->m_Vel = vec2(5 * dir, -5);
}

void CGameControllerDDRace::Snap(int SnappingClient)
{
	IGameController::Snap(SnappingClient);

	CNetObj_GameData *pGameDataObj = (CNetObj_GameData *)Server()->SnapNewItem(NETOBJTYPE_GAMEDATA, 0, sizeof(CNetObj_GameData));
	if (!pGameDataObj)
		return;

	if (m_apFlags[TEAM_RED])
	{
		if (m_apFlags[TEAM_RED]->m_AtStand)
			pGameDataObj->m_FlagCarrierRed = FLAG_ATSTAND;
		else if (m_apFlags[TEAM_RED]->m_pCarryingCharacter && m_apFlags[TEAM_RED]->m_pCarryingCharacter->GetPlayer())
			pGameDataObj->m_FlagCarrierRed = m_apFlags[TEAM_RED]->m_pCarryingCharacter->GetPlayer()->GetCID();
		else
			pGameDataObj->m_FlagCarrierRed = FLAG_TAKEN;
	}
	else
		pGameDataObj->m_FlagCarrierRed = FLAG_MISSING;
	if (m_apFlags[TEAM_BLUE])
	{
		if (m_apFlags[TEAM_BLUE]->m_AtStand)
			pGameDataObj->m_FlagCarrierBlue = FLAG_ATSTAND;
		else if (m_apFlags[TEAM_BLUE]->m_pCarryingCharacter && m_apFlags[TEAM_BLUE]->m_pCarryingCharacter->GetPlayer())
			pGameDataObj->m_FlagCarrierBlue = m_apFlags[TEAM_BLUE]->m_pCarryingCharacter->GetPlayer()->GetCID();
		else
			pGameDataObj->m_FlagCarrierBlue = FLAG_TAKEN;
	}
	else
		pGameDataObj->m_FlagCarrierBlue = FLAG_MISSING;
}

CGameControllerDDRace::~CGameControllerDDRace()
{
	// Nothing to clean
}

void CGameControllerDDRace::Tick()
{
	IGameController::Tick();

	if (GameServer()->m_World.m_ResetRequested || GameServer()->m_World.m_Paused)
		return;

	for (int fi = 0; fi < 2; fi++)
	{
		CFlag *F = m_apFlags[fi];

		if (!F)
			continue;

		// flag hits death-tile or left the game layer, reset it
		if (GameServer()->Collision()->GetCollisionAt(F->m_Pos.x, F->m_Pos.y) == TILE_DEATH || GameServer()->Collision()->GetFCollisionAt(F->m_Pos.x, F->m_Pos.y) == TILE_DEATH || F->GameLayerClipped(F->m_Pos))
		{
			GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", "flag_return");
			if (g_Config.m_SvFlagSounds)
			{
				GameServer()->CreateSoundGlobal(SOUND_CTF_RETURN);
			}
			F->Reset();
			continue;
		}

		if (F->m_pCarryingCharacter && F->m_pCarryingCharacter != 0)
		{
			// update flag position
			F->m_Pos = F->m_pCarryingCharacter->m_Pos;

			if (F->m_pCarryingCharacter->m_FirstFreezeTick != 0) {

				if (Server()->Tick() > F->m_pCarryingCharacter->m_FirstFreezeTick + Server()->TickSpeed() * 8) {

					if (m_apFlags[0]->m_pCarryingCharacter == F->m_pCarryingCharacter->GetPlayer()->GetCharacter())
					{
						DropFlag(0, F->m_pCarryingCharacter->GetPlayer()->GetCharacter()->GetAimDir()); //red
					}
					else if (m_apFlags[1]->m_pCarryingCharacter == F->m_pCarryingCharacter->GetPlayer()->GetCharacter())
					{
						DropFlag(1, F->m_pCarryingCharacter->GetPlayer()->GetCharacter()->GetAimDir()); //blue
					}
				}

			}

		}
		else
		{
			if (GameServer()->Collision()->GetTileIndex(GameServer()->Collision()->GetMapIndex(F->m_Pos)) == 95)
			{
				if (g_Config.m_SvFlagSounds)
				{
					GameServer()->CreateSoundGlobal(SOUND_CTF_RETURN);
				}
				F->Reset();
			}

			CCharacter *apCloseCCharacters[MAX_CLIENTS];
			int Num = GameServer()->m_World.FindEntities(F->m_Pos, CFlag::ms_PhysSize, (CEntity**)apCloseCCharacters, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);
			for (int i = 0; i < Num; i++)
			{
				if (!apCloseCCharacters[i]->IsAlive() || apCloseCCharacters[i]->GetPlayer()->GetTeam() == TEAM_SPECTATORS || GameServer()->Collision()->IntersectLine(F->m_Pos, apCloseCCharacters[i]->m_Pos, NULL, NULL))
					continue;
				if (m_apFlags[0] && m_apFlags[1])
				{
					if (m_apFlags[0]->m_pCarryingCharacter == apCloseCCharacters[i] || m_apFlags[1]->m_pCarryingCharacter == apCloseCCharacters[i] || (F->m_pLastCarryingCharacter == apCloseCCharacters[i] && (F->m_DropFreezeTick + Server()->TickSpeed() * 2) > Server()->Tick()))
						continue;
				}
				else
				{
					if (m_apFlags[0])
					{
						if (m_apFlags[0]->m_pCarryingCharacter == apCloseCCharacters[i] || (m_apFlags[0]->m_pLastCarryingCharacter == apCloseCCharacters[i] && (m_apFlags[0]->m_DropFreezeTick + Server()->TickSpeed() * 2) > Server()->Tick()))
							continue;
					}
					if (m_apFlags[1])
					{
						if (m_apFlags[1]->m_pCarryingCharacter == apCloseCCharacters[i] || (m_apFlags[1]->m_pLastCarryingCharacter == apCloseCCharacters[i] && (m_apFlags[1]->m_DropFreezeTick + Server()->TickSpeed() * 2) > Server()->Tick()))
							continue;
					}
				}

				// take the flag
				if (F->m_AtStand)
				{
					F->m_GrabTick = Server()->Tick();
				}

				F->m_AtStand = 0;
				F->m_pCarryingCharacter = apCloseCCharacters[i];
				apCloseCCharacters[i]->m_FirstFreezeTick = 0;

				if (g_Config.m_SvFlagSounds)
				{
					for (int c = 0; c < MAX_CLIENTS; c++)
					{
						CPlayer *pPlayer = GameServer()->m_apPlayers[c];
						if (!pPlayer)
							continue;

						if (pPlayer->GetTeam() == TEAM_SPECTATORS && pPlayer->m_SpectatorID != SPEC_FREEVIEW && GameServer()->m_apPlayers[pPlayer->m_SpectatorID] && GameServer()->m_apPlayers[pPlayer->m_SpectatorID]->GetTeam() == fi)
							GameServer()->CreateSoundGlobal(SOUND_CTF_GRAB_EN, c);
						else if (pPlayer->GetTeam() == fi)
							GameServer()->CreateSoundGlobal(SOUND_CTF_GRAB_EN, c);
						else
							GameServer()->CreateSoundGlobal(SOUND_CTF_GRAB_PL, c);
					}
				}
				// demo record entry
				GameServer()->CreateSoundGlobal(SOUND_CTF_GRAB_EN, -2);
				break;
			}
		}

		if ((!F->m_pCarryingCharacter || F->m_pCarryingCharacter == 0) && !F->m_AtStand)
		{
			if (Server()->Tick() > F->m_DropTick + Server()->TickSpeed() * 90)
			{
				GameServer()->CreateSoundGlobal(SOUND_CTF_RETURN);
				F->Reset();
			}
			else
			{
				//Gravity
				F->m_Vel.y += GameServer()->Tuning()->m_Gravity;

				//Friction
				vec2 TempVel = F->m_Vel;

				//Speedups
				if (GameServer()->Collision()->IsSpeedup(GameServer()->Collision()->GetMapIndex(F->m_Pos))) {
					int Force, MaxSpeed = 0;
					vec2 Direction, MaxVel;
					float TeeAngle, SpeederAngle, DiffAngle, SpeedLeft, TeeSpeed;
					GameServer()->Collision()->GetSpeedup(GameServer()->Collision()->GetMapIndex(F->m_Pos), &Direction, &Force, &MaxSpeed);

					if (Force == 255 && MaxSpeed)
					{
						F->m_Vel = Direction * (MaxSpeed / 5);
					}

					else
					{
						if (MaxSpeed > 0 && MaxSpeed < 5) MaxSpeed = 5;
						if (MaxSpeed > 0)
						{
							if (Direction.x > 0.0000001f)
								SpeederAngle = -atan(Direction.y / Direction.x);
							else if (Direction.x < 0.0000001f)
								SpeederAngle = atan(Direction.y / Direction.x) + 2.0f * asin(1.0f);
							else if (Direction.y > 0.0000001f)
								SpeederAngle = asin(1.0f);
							else
								SpeederAngle = asin(-1.0f);

							if (SpeederAngle < 0)
								SpeederAngle = 4.0f * asin(1.0f) + SpeederAngle;

							if (TempVel.x > 0.0000001f)
								TeeAngle = -atan(TempVel.y / TempVel.x);
							else if (TempVel.x < 0.0000001f)
								TeeAngle = atan(TempVel.y / TempVel.x) + 2.0f * asin(1.0f);
							else if (TempVel.y > 0.0000001f)
								TeeAngle = asin(1.0f);
							else
								TeeAngle = asin(-1.0f);

							if (TeeAngle < 0)
								TeeAngle = 4.0f * asin(1.0f) + TeeAngle;

							TeeSpeed = sqrt(pow(TempVel.x, 2) + pow(TempVel.y, 2));

							DiffAngle = SpeederAngle - TeeAngle;
							SpeedLeft = MaxSpeed / 5.0f - cos(DiffAngle) * TeeSpeed;
							if (abs(SpeedLeft) > Force && SpeedLeft > 0.0000001f)
								TempVel += Direction * Force;
							else if (abs(SpeedLeft) > Force)
								TempVel += Direction * -Force;
							else
								TempVel += Direction * SpeedLeft;
						}
						else
							TempVel += Direction * Force;
					}
				}
				

				//stopper
				int CurrentIndex = GameServer()->Collision()->GetMapIndex(F->m_Pos);
				std::list < int > Indices = GameServer()->Collision()->GetMapIndices(F->m_PrevPos, F->m_Pos);
				if (!Indices.empty())
					for (std::list < int >::iterator i = Indices.begin(); i != Indices.end(); i++)
						HandleTiles(*i);
				else
				{
					HandleTiles(CurrentIndex);
				}

				if (TempVel.x > 0 && ((m_TileIndex == TILE_STOP && m_TileFlags == ROTATION_270) || (m_TileIndexL == TILE_STOP && m_TileFlagsL == ROTATION_270) || (m_TileIndexL == TILE_STOPS && (m_TileFlagsL == ROTATION_90 || m_TileFlagsL == ROTATION_270)) || (m_TileIndexL == TILE_STOPA) || (m_TileFIndex == TILE_STOP && m_TileFFlags == ROTATION_270) || (m_TileFIndexL == TILE_STOP && m_TileFFlagsL == ROTATION_270) || (m_TileFIndexL == TILE_STOPS && (m_TileFFlagsL == ROTATION_90 || m_TileFFlagsL == ROTATION_270)) || (m_TileFIndexL == TILE_STOPA)))
					TempVel.x = 0;
				if (TempVel.x < 0 && ((m_TileIndex == TILE_STOP && m_TileFlags == ROTATION_90) || (m_TileIndexR == TILE_STOP && m_TileFlagsR == ROTATION_90) || (m_TileIndexR == TILE_STOPS && (m_TileFlagsR == ROTATION_90 || m_TileFlagsR == ROTATION_270)) || (m_TileIndexR == TILE_STOPA) || (m_TileFIndex == TILE_STOP && m_TileFFlags == ROTATION_90) || (m_TileFIndexR == TILE_STOP && m_TileFFlagsR == ROTATION_90) || (m_TileFIndexR == TILE_STOPS && (m_TileFFlagsR == ROTATION_90 || m_TileFFlagsR == ROTATION_270)) || (m_TileFIndexR == TILE_STOPA)))
					TempVel.x = 0;
				F->m_Vel = TempVel;
				IsGrounded(fi, true);

				GameServer()->Collision()->MoveBox(&F->m_Pos, &F->m_Vel, vec2(F->ms_PhysSize, F->ms_PhysSize), 0.5f);
			}
		}

		F->m_PrevPos = F->m_Pos;
	}
}

bool CGameControllerDDRace::IsGrounded(int ID, bool SetVel)
{
	CFlag *F = m_apFlags[ID];

	if (!F)
		return false;

	if ((GameServer()->Collision()->CheckPoint(F->m_Pos.x + ms_PhysSize / 2, F->m_Pos.y + ms_PhysSize / 2 + 5))
		|| (GameServer()->Collision()->CheckPoint(F->m_Pos.x - ms_PhysSize / 2, F->m_Pos.y + ms_PhysSize / 2 + 5)))
	{
		if (SetVel)
			F->m_Vel.x *= 0.75f;
		return true;
	}

	if (F->m_Vel.y < 0 && (((m_TileIndex == TILE_STOP && m_TileFlags == ROTATION_180) || (m_TileIndexB == TILE_STOP && m_TileFlagsB == ROTATION_180) || (m_TileIndexB == TILE_STOPS && (m_TileFlagsB == ROTATION_0 || m_TileFlagsB == ROTATION_180)) || (m_TileIndexB == TILE_STOPA) || (m_TileFIndex == TILE_STOP && m_TileFFlags == ROTATION_180) || (m_TileFIndexB == TILE_STOP && m_TileFFlagsB == ROTATION_180) || (m_TileFIndexB == TILE_STOPS && (m_TileFFlagsB == ROTATION_0 || m_TileFFlagsB == ROTATION_180)) || (m_TileFIndexB == TILE_STOPA)))
		|| (F->m_Vel.y > 0 && ((m_TileIndex == TILE_STOP && m_TileFlags == ROTATION_0) || (m_TileIndexT == TILE_STOP && m_TileFlagsT == ROTATION_0) || (m_TileIndexT == TILE_STOPS && (m_TileFlagsT == ROTATION_0 || m_TileFlagsT == ROTATION_180)) || (m_TileIndexT == TILE_STOPA) || (m_TileFIndex == TILE_STOP && m_TileFFlags == ROTATION_0) || (m_TileFIndexT == TILE_STOP && m_TileFFlagsT == ROTATION_0) || (m_TileFIndexT == TILE_STOPS && (m_TileFFlagsT == ROTATION_0 || m_TileFFlagsT == ROTATION_180)) || (m_TileFIndexT == TILE_STOPA))))
	{
		F->m_Vel.y = 0;
		if (SetVel)
			F->m_Vel.x *= 0.925f;
		return true;
	}

	if (SetVel)
		F->m_Vel.x *= 0.98f;
	return false;
}

void CGameControllerDDRace::InitTeleporter()
{
	if (!GameServer()->Collision()->Layers()->TeleLayer())
		return;
	int Width = GameServer()->Collision()->Layers()->TeleLayer()->m_Width;
	int Height = GameServer()->Collision()->Layers()->TeleLayer()->m_Height;

	for (int i = 0; i < Width * Height; i++)
	{
		int Number = GameServer()->Collision()->TeleLayer()[i].m_Number;
		int Type = GameServer()->Collision()->TeleLayer()[i].m_Type;
		if (Number > 0)
		{
			if (Type == TILE_TELEOUT)
			{
				m_TeleOuts[Number - 1].push_back(
						vec2(i % Width * 32 + 16, i / Width * 32 + 16));
			}
			else if (Type == TILE_TELECHECKOUT)
			{
				m_TeleCheckOuts[Number - 1].push_back(
						vec2(i % Width * 32 + 16, i / Width * 32 + 16));
			}
		}
	}
}

void CGameControllerDDRace::HandleTiles(int Index)
{
	for (int fi = 0; fi < 2; fi++)
	{
		CFlag *F = m_apFlags[fi];

		if (!F)
			continue;

		int MapIndex = Index;
		float Offset = 4.0f;
		MapIndexL = GameServer()->Collision()->GetPureMapIndex(vec2(F->m_Pos.x + ms_PhysSize + Offset, F->m_Pos.y));
		MapIndexR = GameServer()->Collision()->GetPureMapIndex(vec2(F->m_Pos.x - ms_PhysSize - Offset, F->m_Pos.y));
		MapIndexT = GameServer()->Collision()->GetPureMapIndex(vec2(F->m_Pos.x, F->m_Pos.y + ms_PhysSize + Offset));
		MapIndexB = GameServer()->Collision()->GetPureMapIndex(vec2(F->m_Pos.x, F->m_Pos.y - ms_PhysSize - Offset));
		m_TileIndex = GameServer()->Collision()->GetTileIndex(MapIndex);
		m_TileFlags = GameServer()->Collision()->GetTileFlags(MapIndex);
		m_TileIndexL = GameServer()->Collision()->GetTileIndex(MapIndexL);
		m_TileFlagsL = GameServer()->Collision()->GetTileFlags(MapIndexL);
		m_TileIndexR = GameServer()->Collision()->GetTileIndex(MapIndexR);
		m_TileFlagsR = GameServer()->Collision()->GetTileFlags(MapIndexR);
		m_TileIndexB = GameServer()->Collision()->GetTileIndex(MapIndexB);
		m_TileFlagsB = GameServer()->Collision()->GetTileFlags(MapIndexB);
		m_TileIndexT = GameServer()->Collision()->GetTileIndex(MapIndexT);
		m_TileFlagsT = GameServer()->Collision()->GetTileFlags(MapIndexT);
		m_TileFIndex = GameServer()->Collision()->GetFTileIndex(MapIndex);
		m_TileFFlags = GameServer()->Collision()->GetFTileFlags(MapIndex);
		m_TileFIndexL = GameServer()->Collision()->GetFTileIndex(MapIndexL);
		m_TileFFlagsL = GameServer()->Collision()->GetFTileFlags(MapIndexL);
		m_TileFIndexR = GameServer()->Collision()->GetFTileIndex(MapIndexR);
		m_TileFFlagsR = GameServer()->Collision()->GetFTileFlags(MapIndexR);
		m_TileFIndexB = GameServer()->Collision()->GetFTileIndex(MapIndexB);
		m_TileFFlagsB = GameServer()->Collision()->GetFTileFlags(MapIndexB);
		m_TileFIndexT = GameServer()->Collision()->GetFTileIndex(MapIndexT);
		m_TileFFlagsT = GameServer()->Collision()->GetFTileFlags(MapIndexT);
	}
}