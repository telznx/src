// SDK.hpp CORRIGIDO

#pragma once
#include <Includes/Includes.hpp>
#include <Core/SDK/Memory.hpp>
#include <Core/Offsets.hpp>
#include <Core/SDK/Structs/GameClasses.hpp>

#include <d3dx11.h>
#include <d3d11.h>
#include <D3DX11tex.h>
#include <D3dx9math.h>
#pragma comment( lib, "d3d11.lib" )
#pragma comment( lib, "D3DX11.lib" )

// Se g_Variables é usado pelas funções inline abaixo e não está em <Includes/Includes.hpp>,
// você pode precisar de uma declaração forward ou extern aqui, ou incluir o header apropriado.
// Exemplo:
// struct Variables; // Ou o nome real da sua struct/classe para g_Variables
// extern Variables g_Variables; 


namespace Core {
	namespace SDK {
		namespace Pointers {
			// Object Pointers
			inline CPedFactory* pWorld = nullptr;
			inline CPed* pLocalPlayer = nullptr;
			inline CReplayInterFace* pReplayInterFace = nullptr;
			inline uintptr_t pBlipList = 0, pViewPort = 0, pCamGamePlayDirector = 0;
		}

		namespace Game
		{

			struct cSkeleton_t {
				std::unordered_map<unsigned int, int> StoredBonesIdx;
				struct crSkeletonData_t {
					uintptr_t Ptr, m_BoneIdTable;
					unsigned int m_Used;
					unsigned int m_NumBones;
					unsigned short m_BoneIdTable_Slots;
				} crSkeletonData;
				uintptr_t m_pSkeleton, Arg2;
				D3DXMATRIX Arg1;
			};

			struct NetworkInfo {
				std::string UserName;
				std::string DiscordId;
				std::string SteamId;
			};

			struct EntityStruct {
				CPed* Ped;

				int Id;
				int Index;
				int PedType;

				bool IsFriend;

				D3DXVECTOR3 Pos;

				float Health;
				float MaxHealth;
				float Armor;
				float Distance;

				std::string WeaponName;

				NetworkInfo NetworkInfo;

				float HealthAnim;
			};

			struct EspAnim {
				bool CanFadeOut;
				float Health;
				float Armor;
				float Alpha;
			};

			struct VehicleStructure {
				CVehicle* Pointer;
				CPed* Driver;
				std::string Name;
				float Dist;
				bool IsLocked;
				D3DXVECTOR3 Pos;
			};

			inline std::vector<EntityStruct> EntityList;
			inline std::vector<VehicleStructure> VehicleList;
			inline std::unordered_map<CPed*, bool> FriendMap;

			inline D3DXVECTOR2 WorldToScreen(D3DXVECTOR3 World)
			{
				D3DXMATRIX ViewMatrix = Mem.Read<D3DXMATRIX>(Core::SDK::Pointers::pViewPort + 0x24C);
				D3DXMatrixTranspose(&ViewMatrix, &ViewMatrix);

				auto VecX = D3DXVECTOR4(ViewMatrix._21, ViewMatrix._22, ViewMatrix._23, ViewMatrix._24),
					VecY = D3DXVECTOR4(ViewMatrix._31, ViewMatrix._32, ViewMatrix._33, ViewMatrix._34),
					VecZ = D3DXVECTOR4(ViewMatrix._41, ViewMatrix._42, ViewMatrix._43, ViewMatrix._44);

				D3DXVECTOR3 ScreenPos = D3DXVECTOR3(
					(VecX.x * World.x) + (VecX.y * World.y) + (VecX.z * World.z) + VecX.w,
					(VecY.x * World.x) + (VecY.y * World.y) + (VecY.z * World.z) + VecY.w,
					(VecZ.x * World.x) + (VecZ.y * World.y) + (VecZ.z * World.z) + VecZ.w
				);

				if (ScreenPos.z <= 0.1f)
					return D3DXVECTOR2(0, 0);

				ScreenPos.z = 1.0f / ScreenPos.z;
				ScreenPos.x *= ScreenPos.z;
				ScreenPos.y *= ScreenPos.z;

				// Acesso a g_Variables - certifique-se que está definido e acessível.
				ScreenPos.x = (g_Variables.g_vGameWindowSize.x / 2.0f) + (ScreenPos.x * (g_Variables.g_vGameWindowSize.x / 2.0f));
				ScreenPos.y = (g_Variables.g_vGameWindowSize.y / 2.0f) - (ScreenPos.y * (g_Variables.g_vGameWindowSize.y / 2.0f));
				return D3DXVECTOR2(ScreenPos.x, ScreenPos.y);
			}

			inline std::unordered_map<uintptr_t, std::unordered_map<unsigned int, int>> StoredBonesIdx;
			inline std::mutex BonesCache;


			inline bool GetBoneIndex(cSkeleton_t cSkeleton, unsigned int boneId, int& outBoneIdx)
			{
				std::lock_guard<std::mutex> lock(BonesCache);
				{
					auto StoredBonesIdxCache = StoredBonesIdx[cSkeleton.crSkeletonData.Ptr];

					if (StoredBonesIdxCache.find(boneId) != StoredBonesIdxCache.end()) {
						outBoneIdx = StoredBonesIdxCache[boneId];
						return true;
					}


					if (cSkeleton.crSkeletonData.m_Used != 0) // crSkeletonData->m_BoneIdTable.m_Used
					{
						unsigned short m_BoneIdTable_Slots = cSkeleton.crSkeletonData.m_BoneIdTable_Slots; // crSkeletonData->m_BoneIdTable.m_Slots

						std::uintptr_t m_BoneIdTable_Hash = Mem.Read<std::uintptr_t>(cSkeleton.crSkeletonData.m_BoneIdTable + 0x8 * (boneId % (unsigned int)m_BoneIdTable_Slots));
						for (std::uintptr_t i = m_BoneIdTable_Hash; i != 0; i = Mem.Read<std::uintptr_t>(i + 0x8))
						{
							int i_key = Mem.Read<int>(i);
							if (boneId == i_key)
							{
								int p_Data = Mem.Read<int>(i + 0x4);
								if (p_Data)
								{
									outBoneIdx = p_Data;
									StoredBonesIdxCache[boneId] = p_Data;
									return true;
								}
							}
						}
					}
					else if (boneId < cSkeleton.crSkeletonData.m_NumBones)
					{
						outBoneIdx = boneId;
						StoredBonesIdxCache[boneId] = boneId;

						return true;
					}

					return false;
				}
			}

			inline D3DXVECTOR3 GetBonePosInstFrag(std::uintptr_t InstFrag, unsigned int BoneID, D3DXMATRIX Arg1, uintptr_t Arg2)
			{
				D3DXMATRIX v4; // r8
				D3DXMATRIX Result; // rax

				v4 = Arg1;
				// Removido (!v4) pois D3DXMATRIX não é um ponteiro. Se Arg1 for inválida, é outro problema.
				// if (!v4) { return D3DXVECTOR3(0, 0, 0); } 

				Result = Mem.Read<D3DXMATRIX>(Arg2 + ((unsigned __int64)BoneID << 6));
				// Removido (!Result) pois D3DXMATRIX não é um ponteiro.
				// if (!Result) { return D3DXVECTOR3(0, 0, 0); }

				D3DXVECTOR3 vec1(v4._11, v4._12, v4._13);
				D3DXVECTOR3 vec2(v4._21, v4._22, v4._23);
				D3DXVECTOR3 vec3(v4._31, v4._32, v4._33);
				D3DXVECTOR3 vec4(v4._41, v4._42, v4._43);
				D3DXVECTOR3 vec5(Result._41, Result._42, Result._43);
				return D3DXVECTOR3(
					vec1.x * vec5.x + vec4.x + vec2.x * vec5.y + vec3.x * vec5.z,
					vec1.y * vec5.x + vec4.y + vec2.y * vec5.y + vec3.y * vec5.z,
					vec1.z * vec5.x + vec4.z + vec2.z * vec5.y + vec3.z * vec5.z
				);
			}

			inline D3DXVECTOR3 GetBonePosComplex(CPed* Ped, unsigned int Mask, cSkeleton_t cSkeleton_t)
			{
				bool result = false;

				int BoneId = 0;

				if (GetBoneIndex(cSkeleton_t, Mask, BoneId)) {
					return GetBonePosInstFrag(cSkeleton_t.m_pSkeleton, BoneId, cSkeleton_t.Arg1, cSkeleton_t.Arg2);
				}

				return Ped->GetPos();
			}

			inline std::string GetPedName(int id, CPed* Ped)
			{
				for (size_t i = 0; i < SDK::Game::EntityList.size(); i++) // Usar size_t para o loop
				{
					// g_Offsets precisa estar acessível aqui.
					uintptr_t NetPointer = Mem.Read<uintptr_t>(g_Offsets.m_NetIdToNamesEntry + (i * 0x8));

					if (!NetPointer) continue;

					int Id_Net = Mem.Read<int>(NetPointer + 0x10); // Renomeado para Id_Net para evitar sombreamento

					if (Id_Net == id) {
						return Mem.ReadString(NetPointer + 0x18);
					}
				}

				return "NPC";
			}

			inline CPed* GetClosestPed(int MaxDistance, bool IgnoreNpc, bool VisibleCheck)
			{
				CPed* ClosestPed = nullptr;
				float ClosestDist = FLT_MAX;

				for (auto& Entity : Core::SDK::Game::EntityList)
				{
					CPed* Ped = Entity.Ped;

					if (Ped == Core::SDK::Pointers::pLocalPlayer)
						continue;

					if (std::abs(Entity.Health) <= 101.f) // Condição original, talvez Entity.Health <= 0.f ?
						continue;

					if (IgnoreNpc && Entity.PedType != 2)
						continue;

					if (Entity.IsFriend)
						continue;

					if (Entity.Distance > MaxDistance)
						continue;

					D3DXVECTOR2 PedPos = Core::SDK::Game::WorldToScreen(Entity.Pos);

					// g_Variables precisa estar acessível aqui.
					float x = PedPos.x - g_Variables.g_vGameWindowCenter.x;
					float y = PedPos.y - g_Variables.g_vGameWindowCenter.y;
					float AimDist = std::sqrt((x * x) + (y * y));

					if (AimDist < ClosestDist)
					{
						ClosestDist = AimDist;
						ClosestPed = Ped;
					}
				}

				return ClosestPed;
			}


			// --- ADICIONADO A DECLARAÇÃO DE GetClosestVehicle ABAIXO ---
			CVehicle* GetClosestVehicle(float max_dist, bool check_visible_flag);
			// --- FIM DA ADIÇÃO ---

			inline bool IsOnScreen(D3DXVECTOR2 Pos)
			{
				if (Pos.x == 0.0f && Pos.y == 0.0f) return false; // Comparação mais precisa para floats
				// Uma checagem mais robusta seria verificar se está dentro dos limites da tela:
				// return (Pos.x >= 0 && Pos.x <= g_Variables.g_vGameWindowSize.x && Pos.y >= 0 && Pos.y <= g_Variables.g_vGameWindowSize.y);
				return true; // A sua lógica original
			}

		} // Fim do namespace Game


	} // Fim do namespace SDK
} // Fim do namespace Core