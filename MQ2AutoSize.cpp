// MQ2AutoSize.cpp : Resize spawns by distance or whole zone (client only)
//
// 06/09/2009: Fixed for changes to the eqgame function
//				 added corpse option and autosave -pms
// 02/09/2009: added parameters, merc, npc & everything spawn options,
//				 bug fixes and code cleanup - pms
// 06/28/2008: finds its own offset - ieatacid
//
// version 0.9.3 (by Psycotic)
// v1.0 - Eqmule 07-22-2016 - Added string safety.
//
//////////////////////////////////////////////////////////////////////////////
// Usage:
//	This plugin will automatically resize configured spawns to the specified
//	size. You can configure it to only resize within a specific range and then
//	resize back to normal when your distance moves out of that range.
//	Current default range is set to 50 and may be changed via INI or cmd line
//	NOTE:  These effects are CLIENT SIDE ONLY!
//
// Commands:
//  /autosize [on|off]			- Toggles zone-wide  AutoSize on/off
//  /autosize dist				- Toggles distance-based AutoSize on/off
//  /autosize range #			- Sets range for distance-based AutoSize
//  /autosize pc [on|off]		- Toggles AutoSize PC spawn types
//  /autosize npc [on|off]		- Toggles AutoSize NPC spawn types
//  /autosize pets [on|off]		- Toggles AutoSize pet spawn types
//  /autosize mercs [on|off]	- Toggles AutoSize mercenary spawn types
//  /autosize mounts [on|off]	- Toggles AutoSize mounted player spawn types
//  /autosize corpse [on|off]	- Toggles AutoSize corpse spawn types
//  /autosize target [on|off]	- Resizes your target to sizetarget size
//  /autosize self [on|off]		- Toggles AutoSize for your character
//  /autosize everything [on|off]	- Toggles AutoSize all spawn types
//
//  (Valid sizes 1 to 250)
//  /autosize size #			- Sets default size for "everything"
//  /autosize sizepc #			- Sets size for PC spawn types
//  /autosize sizenpc #			- Sets size for NPC spawn types
//  /autosize sizepets #		- Sets size for pet spawn types
//  /autosize sizetarget #		- Sets size for target parameter
//  /autosize sizemercs #		- Sets size for mercenary spawn types
//  /autosize sizemounts #		- Sets size for mounted player spawn types
//  /autosize sizecorpse #		- Sets size for corpse spawn types
//  /autosize sizeself #		- Sets size for your character
//
//  /autosize status			- Display current plugin settings to chatwnd
//  /autosize help				- Display command syntax to chatwnd
//  /autosize save				- Save settings to INI file (auto on plugin unload)
//  /autosize load				- Load settings from INI file (auto on plugin load)
//  /autosize autosave [on|off]	- Automatically save settings to INI file when an option is toggled or size is set
//
//////////////////////////////////////////////////////////////////////////////

#include <mq/Plugin.h>
#include <mq/imgui/ImGuiUtils.h>

const char* MODULE_NAME = "MQ2AutoSize";
PreSetup(MODULE_NAME);
PLUGIN_VERSION(1.0);
// this controls how many pulses to perform a radius-based resize (bad performance hit)
const int	SKIP_PULSES = 5;
// min and max size values
const float MIN_SIZE = 1.0f;
const float MAX_SIZE = 250.0f;

// used by the plugin
const float OTHER_SIZE = 1.0f;
const float ZERO_SIZE = 0.0f;
unsigned int uiSkipPulse = 0;
char szTemp[MAX_STRING] = { 0 };
void SpawnListResize(bool bReset);

class AutoSizeType* pAutoSizeType = nullptr;

float SaneSize(float fValidate)
{
	if (fValidate < MIN_SIZE)
	{
		return MIN_SIZE;
	}
	if (fValidate > MAX_SIZE)
	{
		return MAX_SIZE;
	}
	return fValidate;
}

template <unsigned int _Size>LPSTR SafeItoa(int _Value, char(&_Buffer)[_Size], int _Radix)
{
	errno_t err = _itoa_s(_Value, _Buffer, _Radix);
	if (!err) {
		return _Buffer;
	}
	return "";
}

enum eAutoSizeType
{
	AS_PC = 0,
	AS_NPC,
	AS_PETS,
	AS_MERCS,
	AS_MOUNTS,
	AS_CORPSES,
	AS_TARGET,
	AS_EVERYTHING,
	AS_SELF,
};

static std::map<std::string, eAutoSizeType, CaseInsensitiveLess> stringToTypeMap = {
	{"sizepc", AS_PC},
	{"sizenpc", AS_NPC},
	{"sizepets", AS_PETS},
	{"sizecorpse", AS_CORPSES},
	{"sizemounts", AS_MOUNTS},
	{"sizemercs", AS_MERCS},
	{"size", AS_EVERYTHING},
	{"sizetarget", AS_TARGET},
	{"sizeself", AS_SELF},
	{"pc", AS_PC},
	{"npc", AS_NPC},
	{"pets", AS_PETS},
	{"corpse", AS_CORPSES},
	{"mercs", AS_MERCS},
	{"mounts", AS_MOUNTS},
	{"everything", AS_EVERYTHING},
	{"target", AS_TARGET},
	{"self", AS_SELF},
};

class AutoSizeSetting
{
public:
	AutoSizeSetting(eAutoSizeType _autoSizeType, std::string _name, std::string _iniKeyEnabled, std::string _iniKeySize, eSpawnType _spawnType)
	{
		autoSizeType = _autoSizeType;
		name = _name;
		iniKeyEnabled = _iniKeyEnabled;
		iniKeySize = _iniKeySize;
		spawnType = _spawnType;
		enabled = false;
		size = 1.0;
	}

	eAutoSizeType GetAutoSizeType();
	std::string GetName();
	eSpawnType GetSpawnType();
	bool IsEnabled();
	void SetEnabled(bool enabled);
	float GetSize();
	void SetSize(float size);
	void LoadINI();
	void SaveINI();
private:
	eAutoSizeType autoSizeType;
	std::string name;
	std::string iniKeyEnabled;
	std::string iniKeySize;
	eSpawnType spawnType;
	bool enabled;
	float size;
};

eAutoSizeType AutoSizeSetting::GetAutoSizeType()
{
	return autoSizeType;
}

std::string AutoSizeSetting::GetName()
{
	return name;
}

eSpawnType AutoSizeSetting::GetSpawnType()
{
	return spawnType;
}

bool AutoSizeSetting::IsEnabled()
{
	return enabled;
}

void AutoSizeSetting::SetEnabled(bool _enabled)
{
	enabled = _enabled;
}

float AutoSizeSetting::GetSize()
{
	return size;
}

void AutoSizeSetting::SetSize(float _size)
{
	size = _size;
}

void AutoSizeSetting::LoadINI()
{
	GetPrivateProfileString("Config", iniKeyEnabled, "on", szTemp, 10, INIFileName);
	float iniSize = SaneSize((float)GetPrivateProfileInt("Config", iniKeySize, (int)MIN_SIZE, INIFileName));
	enabled = string_equals(szTemp, "on");
	size = iniSize;
}

void AutoSizeSetting::SaveINI()
{
	WritePrivateProfileString("Config", iniKeyEnabled, enabled ? "on" : "off", INIFileName);
	WritePrivateProfileString("Config", iniKeySize, SafeItoa((int)size, szTemp, 10), INIFileName);
}

class AutoSizeConfiguration
{
public:
	bool autosave = false;
	bool byRangeEnabled = true;
	bool byZoneEnabled = false;
	int resizeRange = 50;

	std::vector<AutoSizeSetting> typeSettings = {
		AutoSizeSetting(AS_PC, "PC", "ResizePC", "SizePC", PC),
		AutoSizeSetting(AS_NPC, "NPC", "ResizeNPC", "SizeNPC", NPC),
		AutoSizeSetting(AS_PETS, "Pets", "ResizePets", "SizePets", PET),
		AutoSizeSetting(AS_MERCS, "Mercs", "ResizeMercs", "SizeMercs", MERCENARY),
		AutoSizeSetting(AS_MOUNTS, "Mounts", "ResizeMounts", "SizeMounts", MOUNT),
		AutoSizeSetting(AS_CORPSES, "Corpses", "ResizeCorpse", "SizeCorpse", CORPSE),
		AutoSizeSetting(AS_TARGET, "Target", "ResizeTarget", "SizeTarget", NONE),
		AutoSizeSetting(AS_EVERYTHING, "Everything", "ResizeAll", "SizeDefault", NONE),
		AutoSizeSetting(AS_SELF, "Self", "ResizeSelf", "SizeSelf", NONE),
	};

	void LoadINI();
	void SaveINI();
	AutoSizeSetting* GetTypeSetting(eAutoSizeType);
};
AutoSizeConfiguration AutoSizeConfig;

void AutoSizeConfiguration::LoadINI()
{
	GetPrivateProfileString("Config", "AutoSave", NULL, szTemp, 10, INIFileName);
	AutoSizeConfig.autosave = string_equals(szTemp, "on");

	for (auto iter = AutoSizeConfig.typeSettings.begin(); iter != AutoSizeConfig.typeSettings.end(); ++iter)
	{
		AutoSizeSetting* setting = &(*iter);
		setting->LoadINI();
	}

	GetPrivateProfileString("Config", "SizeByRange", NULL, szTemp, 10, INIFileName);
	AutoSizeConfig.byZoneEnabled = !(AutoSizeConfig.byRangeEnabled = string_equals(szTemp, "on"));
	AutoSizeConfig.resizeRange = GetPrivateProfileInt("Config", "Range", AutoSizeConfig.resizeRange, INIFileName);
	WriteChatf("\ay%s\aw:: Configuration file loaded.", MODULE_NAME);

	// apply new INI read
	if (GetGameState() == GAMESTATE_INGAME && pLocalPlayer && AutoSizeConfig.byZoneEnabled) SpawnListResize(false);
}

void AutoSizeConfiguration::SaveINI()
{
	WritePrivateProfileString("Config", "AutoSave", AutoSizeConfig.autosave ? "on" : "off", INIFileName);
	WritePrivateProfileString("Config", "SizeByRange", AutoSizeConfig.byRangeEnabled ? "on" : "off", INIFileName);
	WritePrivateProfileString("Config", "Range", SafeItoa(AutoSizeConfig.resizeRange, szTemp, 10), INIFileName);

	for (auto iter = AutoSizeConfig.typeSettings.begin(); iter != AutoSizeConfig.typeSettings.end(); ++iter)
	{
		AutoSizeSetting setting = *iter;
		setting.SaveINI();
	}
	WriteChatf("\ay%s\aw:: Configuration file saved.", MODULE_NAME);
}

AutoSizeSetting* AutoSizeConfiguration::GetTypeSetting(eAutoSizeType autoSizeType)
{
	return &typeSettings.at(autoSizeType);
}

// class to access the ChangeHeight function
class PlayerZoneClient_Hook
{
public:
	DETOUR_TRAMPOLINE_DEF(void, ChangeHeight_Trampoline, (float, float, float, bool))
	void ChangeHeight_Detour(float newHeight, float cameraPos, float speedScale, bool unused)
	{
		ChangeHeight_Trampoline(newHeight, cameraPos, speedScale, unused);
	}

	// this assures valid function call
	void ChangeHeight_Wrapper(float fNewSize)
	{
		float fView = OTHER_SIZE;
		PlayerClient* pSpawn = reinterpret_cast<PlayerClient*>(this);

		if (pSpawn->SpawnID == pLocalPlayer->SpawnID)
		{
			fView = ZERO_SIZE;
		}

		ChangeHeight_Trampoline(fNewSize, fView, 1.0f, false);
	};
};

void ChangeSize(PlayerClient* pChangeSpawn, float fNewSize)
{
	if (pChangeSpawn)
	{
		reinterpret_cast<PlayerZoneClient_Hook*>(pChangeSpawn)->ChangeHeight_Wrapper(fNewSize);
	}
}

void SizePasser(PSPAWNINFO pSpawn, bool bReset)
{
	if ((!bReset && !AutoSizeConfig.byZoneEnabled && !AutoSizeConfig.byRangeEnabled) || GetGameState() != GAMESTATE_INGAME) return;
	PSPAWNINFO pChSpawn = pCharSpawn;
	PSPAWNINFO pLPlayer = pLocalPlayer;
	if (!pLPlayer || !pChSpawn->SpawnID || !pSpawn || !pSpawn->SpawnID) return;

	AutoSizeSetting* setting = AutoSizeConfig.GetTypeSetting(AS_SELF);
	if (pSpawn->SpawnID == pLPlayer->SpawnID)
	{
		if (setting->IsEnabled()) ChangeSize(pSpawn, bReset ? ZERO_SIZE : setting->GetSize());
		return;
	}

	switch (GetSpawnType(pSpawn))
	{
	case PC:
		setting = AutoSizeConfig.GetTypeSetting(AS_PC);
		if (setting->IsEnabled())
		{
			ChangeSize(pSpawn, bReset ? ZERO_SIZE : setting->GetSize());
			return;
		}
		break;
	case NPC:
		setting = AutoSizeConfig.GetTypeSetting(AS_NPC);
		if (setting->IsEnabled())
		{
			ChangeSize(pSpawn, bReset ? ZERO_SIZE : setting->GetSize());
			return;
		}
		break;
	case PET:
		setting = AutoSizeConfig.GetTypeSetting(AS_PETS);
		if (setting->IsEnabled())
		{
			ChangeSize(pSpawn, bReset ? ZERO_SIZE : setting->GetSize());
			return;
		}
		break;
	case MERCENARY:
		setting = AutoSizeConfig.GetTypeSetting(AS_MERCS);
		if (setting->IsEnabled())
		{
			ChangeSize(pSpawn, bReset ? ZERO_SIZE : setting->GetSize());
			return;
		}
		break;
	case CORPSE:
		setting = AutoSizeConfig.GetTypeSetting(AS_CORPSES);
		if (setting->IsEnabled())
		{
			ChangeSize(pSpawn, bReset ? ZERO_SIZE : setting->GetSize());
			return;
		}
		break;
	case MOUNT:
		setting = AutoSizeConfig.GetTypeSetting(AS_MOUNTS);
		if (setting->IsEnabled() && pSpawn->SpawnID != pChSpawn->SpawnID)
		{
			ChangeSize(pSpawn, bReset ? ZERO_SIZE : setting->GetSize());
		}
		return;
	default:
		break;
	}

	setting = AutoSizeConfig.GetTypeSetting(AS_EVERYTHING);
	if (setting->IsEnabled())
	{
		ChangeSize(pSpawn, bReset ? ZERO_SIZE : setting->GetSize());
	}
}

void ResetAllByType(eSpawnType OurType)
{
	PSPAWNINFO pSpawn = pSpawnList;
	PSPAWNINFO pChSpawn = pCharSpawn;
	PSPAWNINFO pLPlayer = pLocalPlayer;
	if (GetGameState() != GAMESTATE_INGAME || !pLPlayer || !pChSpawn->SpawnID || !pSpawn || !pSpawn->SpawnID) return;

	while (pSpawn)
	{
		if (pSpawn->SpawnID == pLPlayer->SpawnID)
		{
			pSpawn = pSpawn->pNext;
			continue;
		}

		eSpawnType ListType = GetSpawnType(pSpawn);
		if (ListType == OurType) ChangeSize(pSpawn, ZERO_SIZE);
		pSpawn = pSpawn->pNext;
	}
}

void SpawnListResize(bool bReset)
{
	if (GetGameState() != GAMESTATE_INGAME) return;
	PSPAWNINFO pSpawn = pSpawnList;
	while (pSpawn)
	{
		SizePasser(pSpawn, bReset);
		pSpawn = pSpawn->pNext;
	}
}

PLUGIN_API void OnAddSpawn(PSPAWNINFO pNewSpawn)
{
	if (AutoSizeConfig.byZoneEnabled) SizePasser(pNewSpawn, false);
}

PLUGIN_API void OnEndZone()
{
	SpawnListResize(false);
}

PLUGIN_API void OnPulse()
{
	if (GetGameState() != GAMESTATE_INGAME || !AutoSizeConfig.byRangeEnabled) return;
	if (uiSkipPulse < SKIP_PULSES)
	{
		uiSkipPulse++;
		return;
	}

	PSPAWNINFO pAllSpawns = pSpawnList;
	float fDist = 0.0f;
	uiSkipPulse = 0;

	while (pAllSpawns)
	{
		fDist = GetDistance(pLocalPlayer, pAllSpawns);
		if (fDist < AutoSizeConfig.resizeRange)
		{
			SizePasser(pAllSpawns, false);
		}
		else if (fDist < AutoSizeConfig.resizeRange + 50)
		{
			SizePasser(pAllSpawns, true);
		}
		pAllSpawns = pAllSpawns->pNext;
	}
}

void OutputHelp()
{
	WriteChatf("\ay%s\aw:: Command Usage Help", MODULE_NAME);
	WriteChatf("  \ag/autosize\ax - Toggles zone-wide AutoSize on/off");
	WriteChatf("  \ag/autosize\ax \aydist\ax - Toggles distance-based AutoSize on/off");
	WriteChatf("  \ag/autosize\ax \ayrange #\ax - Sets range for distance checking");
	WriteChatf("--- Valid Resize Toggles ---");
	WriteChatf("  \ag/autosize\ax [ \aypc\ax | \aynpc\ax | \aypets\ax | \aymercs\ax | \aymounts\ax | \aycorpse\ax | \aytarget\ax | \ayeverything\ax | \ayself\ax ] [ \ayon\ax | \ayoff\ax | \ay#\ax ]");
	WriteChatf("--- Valid Size Syntax (1 to 250) ---");
	WriteChatf("  \ag/autosize\ax [ \aysize\ax | \aysizepc\ax | \aysizenpc\ax | \aysizepets\ax | \aysizemercs\ax | \aysizemounts\ax | \aysizecorpse\ax | \aysizetarget\ax | \aysizeself\ax ] [ \ay#\ax ]");
	WriteChatf("--- Other Valid Commands ---");
	WriteChatf("  \ag/autosize\ax [ \ayhelp\ax | \aystatus\ax | \ayautosave\ax | \aysave\ax | \ayload\ax | \ayui\ax ]");
}

const char* GetTypeEnabledStr(eAutoSizeType autoSizeType)
{
	AutoSizeSetting* setting = AutoSizeConfig.GetTypeSetting(autoSizeType);
	return setting->IsEnabled() ? "\agon\ax" : "\aroff\ax";
}

float GetTypeSize(eAutoSizeType autoSizeType)
{
	return AutoSizeConfig.GetTypeSetting(autoSizeType)->GetSize();
}

void OutputStatus()
{
	char szMethod[100] = { 0 };
	char szOn[10] = "\agon\ax";
	char szOff[10] = "\aroff\ax";
	if (AutoSizeConfig.byZoneEnabled)
	{
		sprintf_s(szMethod, "\ayZonewide\ax");
	}
	else if (AutoSizeConfig.byRangeEnabled)
	{
		sprintf_s(szMethod, "\ayRange\ax) RangeSize(\ag%d\ax", AutoSizeConfig.resizeRange);
	}
	else
	{
		sprintf_s(szMethod, "\arInactive\ax");
	}

	WriteChatf("\ay%s\aw:: Current Status -- Method: (%s)%s", MODULE_NAME, szMethod, AutoSizeConfig.autosave ? " \agAUTOSAVING" : "");
	WriteChatf("Toggles: PC(%s) NPC(%s) Pets(%s) Mercs(%s) Mounts(%s) Corpses(%s) Self(%s) Everything(%s) ", GetTypeEnabledStr(AS_PC), GetTypeEnabledStr(AS_NPC), GetTypeEnabledStr(AS_PETS), GetTypeEnabledStr(AS_MERCS), GetTypeEnabledStr(AS_MOUNTS), GetTypeEnabledStr(AS_CORPSES), GetTypeEnabledStr(AS_SELF), GetTypeEnabledStr(AS_EVERYTHING));
	WriteChatf("Sizes: PC(\ag%.0f\ax) NPC(\ag%.0f\ax) Pets(\ag%.0f\ax) Mercs(\ag%.0f\ax) Mounts(\ag%.0f\ax) Corpses(\ag%.0f\ax) Target(\ag%.0f\ax) Self(\ag%.0f\ax) Everything(\ag%.0f\ax)", GetTypeSize(AS_PC), GetTypeSize(AS_NPC), GetTypeSize(AS_PETS), GetTypeSize(AS_MERCS), GetTypeSize(AS_MOUNTS), GetTypeSize(AS_CORPSES), GetTypeSize(AS_TARGET), GetTypeSize(AS_SELF), GetTypeSize(AS_EVERYTHING));
}

void SizeTarget(AutoSizeSetting* setting)
{
	PSPAWNINFO pTheTarget = pTarget;
	if (pTheTarget && GetGameState() == GAMESTATE_INGAME && pTheTarget->SpawnID)
	{
		ChangeSize(pTheTarget, setting->GetSize());
		char szTarName[MAX_STRING] = { 0 };
		sprintf_s(szTarName, "%s", pTheTarget->DisplayedName);
		WriteChatf("\ay%s\aw:: Resized \ay%s\ax to \ag%.0f\ax", MODULE_NAME, szTarName, setting->GetSize());
	}
	else
	{
		WriteChatf("\ay%s\aw:: \arYou must have a target to use this parameter.", MODULE_NAME);
	}
}

void SetTypeEnabled(AutoSizeSetting* setting, bool bNewValue)
{
	setting->SetEnabled(bNewValue);
	switch (setting->GetAutoSizeType())
	{
	case AS_SELF:
		if (!bNewValue)
		{
			if ((pLocalPlayer)->Mount) ChangeSize(pLocalPlayer, ZERO_SIZE);
			else ChangeSize(pCharSpawn, ZERO_SIZE);
		}
		break;
	case AS_TARGET:
		SizeTarget(setting);
		break;
	case AS_EVERYTHING:
		if (!bNewValue) SpawnListResize(true);
		break;
	default:
		if (!bNewValue) ResetAllByType(setting->GetSpawnType());
	}
	WriteChatf("\ay%s\aw:: Option (\ay%s\ax) now %s\ax", MODULE_NAME, setting->GetName().c_str(), bNewValue ? "\agenabled" : "\ardisabled");
	if (AutoSizeConfig.autosave) AutoSizeConfig.SaveINI();
}

void SetSizeConfig(AutoSizeSetting* setting, float fNewSize)
{
	if (fNewSize >= MIN_SIZE && fNewSize <= MAX_SIZE)
	{
		float fPrevSize = setting->GetSize();
		setting->SetSize(fNewSize);
		WriteChatf("\ay%s\aw:: %s size changed from \ay%.0f\ax to \ag%.0f", MODULE_NAME, setting->GetName().c_str(), fPrevSize, setting->GetSize());
	}
	else
	{
		WriteChatf("\ay%s\aw:: %s size is \ag%.0f\ax (was not modified)", MODULE_NAME, setting->GetName().c_str(), setting->GetSize());
	}
	if (AutoSizeConfig.autosave) AutoSizeConfig.SaveINI();
}

void SetEnabled(bool bEnable)
{
	AutoSizeConfig.byZoneEnabled = bEnable;
	if (AutoSizeConfig.byZoneEnabled)
	{
		if (AutoSizeConfig.byRangeEnabled)
		{
			AutoSizeConfig.byRangeEnabled = false;
			WriteChatf("\ay%s\aw:: AutoSize (\ayRange\ax) now \ardisabled\ax!", MODULE_NAME);
		}
	}
	SpawnListResize(!bEnable);
	WriteChatf("\ay%s\aw:: AutoSize (\ayZonewide\ax) now %s\ax!", MODULE_NAME, AutoSizeConfig.byZoneEnabled ? "\agenabled" : "\ardisabled");
	if (AutoSizeConfig.autosave) AutoSizeConfig.SaveINI();
}

void SetRangeEnabled(bool bEnable)
{
	AutoSizeConfig.byRangeEnabled = bEnable;
	if (AutoSizeConfig.byRangeEnabled)
	{
		if (AutoSizeConfig.byZoneEnabled)
		{
			AutoSizeConfig.byZoneEnabled = false;
			WriteChatf("\ay%s\aw:: AutoSize (\ayAllZone\ax) now \ardisabled\ax!", MODULE_NAME);
		}
	}
	SpawnListResize(!bEnable);
	WriteChatf("\ay%s\aw:: AutoSize (\ayRange\ax) now %s\ax!", MODULE_NAME, AutoSizeConfig.byRangeEnabled ? "\agenabled" : "\ardisabled");
	if (AutoSizeConfig.autosave) AutoSizeConfig.SaveINI();
}

void AutoSizeCmd(PSPAWNINFO pLPlayer, char* szLine)
{
	char szCurArg[MAX_STRING] = { 0 };
	char szNextArg[MAX_STRING] = { 0 };
	GetArg(szCurArg, szLine, 1);
	GetArg(szNextArg, szLine, 2);
	float fNewSize = (float)atof(szNextArg);

	// TODO:  Previous implementation of toggle by range was broken.  Need to add handling for toggle when range is set
	if (!*szCurArg)
	{
		SetEnabled(!AutoSizeConfig.byZoneEnabled);
	}
	else
	{
		if (stringToTypeMap.find(szCurArg) != stringToTypeMap.end())
		{
			AutoSizeSetting* setting = AutoSizeConfig.GetTypeSetting(stringToTypeMap.find(szCurArg)->second);
			if (fNewSize > 0)
			{
				SetSizeConfig(setting, fNewSize);
			}
			else
			{
				bool bNewValue = !setting->IsEnabled();
				if (ci_equals("on", szNextArg))
				{
					bNewValue = true;
				}
				else if (ci_equals("off", szNextArg))
				{
					bNewValue = false;
				}
				SetTypeEnabled(setting, bNewValue);
			}
		}
		// TODO:  else after returns here, determine what's actually needed and clean up
		else if (ci_equals(szCurArg, "dist"))
		{
			SetRangeEnabled(!AutoSizeConfig.byRangeEnabled);
			return;
		}
		else if (ci_equals(szCurArg, "save"))
		{
			AutoSizeConfig.SaveINI();
			return;
		}
		else if (ci_equals(szCurArg, "load"))
		{
			AutoSizeConfig.LoadINI();
			return;
		}
		else if (ci_equals(szCurArg, "autosave"))
		{
			if (ci_equals("on", szNextArg))
			{
				AutoSizeConfig.autosave = true;
			}
			else if (ci_equals("off", szNextArg))
			{
				AutoSizeConfig.autosave = false;
			}
			else
			{
				AutoSizeConfig.autosave = !AutoSizeConfig.autosave;
			}
			WriteChatf("\ay%s\aw:: Option (\ayAutosave\ax) now %s\ax", MODULE_NAME, AutoSizeConfig.autosave ? "\agenabled" : "\ardisabled");
			if (AutoSizeConfig.autosave) AutoSizeConfig.SaveINI();
			return;
		}
		else if (ci_equals(szCurArg, "range"))
		{
			if (atoi(szNextArg) > 0)
			{
				AutoSizeConfig.resizeRange = atoi(szNextArg);
				WriteChatf("\ay%s\aw:: Range set to \ag%d", MODULE_NAME, AutoSizeConfig.resizeRange);
			}
			else
			{
				WriteChatf("\ay%s\aw:: Range is \ag%d\ax (was not modified)", MODULE_NAME, AutoSizeConfig.resizeRange);
			}
			if (AutoSizeConfig.autosave) AutoSizeConfig.SaveINI();
			return;
		}
		else if (ci_equals(szCurArg, "help"))
		{
			OutputHelp();
			return;
		}
		else if (ci_equals(szCurArg, "status"))
		{
			OutputStatus();
			return;
		}
		else if (ci_equals(szCurArg, "on"))
		{
			SetEnabled(true);
		}
		else if (ci_equals(szCurArg, "off"))
		{
			SetEnabled(false);
		}
		else if (ci_equals(szCurArg, "ui"))
		{
			EzCommand("/mqsettings plugins/autosize");
		}
		else
		{
			WriteChatf("\ay%s\aw:: \arInvalid command parameter.", MODULE_NAME);
			return;
		}

		// if size change or everything, pets, mercs,mounts toggled and won't be handled onpulse
		if (AutoSizeConfig.byZoneEnabled) SpawnListResize(false);
	}
}

void MQ2AutoSizeImGuiSettingsPanel()
{
	if (ImGui::Checkbox("Auto Save Settings", &AutoSizeConfig.autosave))
	{
		WriteChatf("\ay%s\aw:: Option (\ay%s\ax) now %s\ax", MODULE_NAME, "autosave", AutoSizeConfig.autosave ? "\agenabled" : "\ardisabled");
		if (AutoSizeConfig.autosave) AutoSizeConfig.SaveINI();
	}
	if (ImGui::Button("Load"))
	{
		AutoSizeConfig.LoadINI();
	}
	ImGui::SameLine();
	if (ImGui::Button("Save"))
	{
		AutoSizeConfig.SaveINI();
	}
	ImGui::Separator();
	if (ImGui::Checkbox("Enabled Zone Wide", &AutoSizeConfig.byZoneEnabled))
	{
		SetEnabled(AutoSizeConfig.byZoneEnabled);
	}
	if (ImGui::Checkbox("Enabled in Range", &AutoSizeConfig.byRangeEnabled))
	{
		SetRangeEnabled(AutoSizeConfig.byRangeEnabled);
	}
	{
		ImGui::BeginDisabled(!AutoSizeConfig.byRangeEnabled);
		ImGui::SetNextItemWidth(150);
		if (ImGui::SliderInt("Resize Range", &AutoSizeConfig.resizeRange, 0, 250))
		{
			WriteChatf("\ay%s\aw:: Range set to \ag%d", MODULE_NAME, AutoSizeConfig.resizeRange);
			if (AutoSizeConfig.autosave) AutoSizeConfig.SaveINI();
		}
		ImGui::EndDisabled();
	}
	ImGui::Separator();
	ImGui::Text("Configure per spawn type AutoSize settings");
	for (auto iter = AutoSizeConfig.typeSettings.begin(); iter != AutoSizeConfig.typeSettings.end(); ++iter)
	{
		AutoSizeSetting* setting = &(*iter);
		ImGui::PushID(setting);
		bool tempValue = setting->IsEnabled();
		if (ImGui::Checkbox("##checkbox", &tempValue))
		{
			SetTypeEnabled(setting, tempValue);
		}
		ImGui::SameLine();
		float tempSize = setting->GetSize();
		ImGui::SetNextItemWidth(150);
		if (ImGui::SliderFloat(setting->GetName().c_str(), &tempSize, MIN_SIZE, MAX_SIZE))
		{
			SetSizeConfig(setting, tempSize);
		}
		ImGui::PopID();
	}
	if (AutoSizeConfig.byZoneEnabled) SpawnListResize(false);
}

// --------------------------------------
// Custom ${AutoSize} TLO
class AutoSizeType : public MQ2Type
{
public:
	enum class VarMembers
	{
		Self = 1,
		SizeSelf,
		PC,
		SizePC,
		NPC,
		SizeNPC,
		Pets,
		SizePets,
		Mercs,
		SizeMercs,
		Corpse,
		SizeCorpse,
		Mounts,
		SizeMounts,
		Target,
		SizeTarget,
		Everything,
		Size,
		AutoSave,
		SizeByZone,
		SizeByRange,
		Range,
	};

	AutoSizeType();

	virtual bool GetMember(MQVarPtr VarPtr, const char* Member, char* Index, MQTypeVar& Dest) override;
	virtual bool ToString(MQVarPtr VarPtr, char* Destination) override;
};

AutoSizeType::AutoSizeType() : MQ2Type("AutoSize")
{
	ScopedTypeMember(VarMembers, Self);
	ScopedTypeMember(VarMembers, SizeSelf);
	ScopedTypeMember(VarMembers, PC);
	ScopedTypeMember(VarMembers, SizePC);
	ScopedTypeMember(VarMembers, NPC);
	ScopedTypeMember(VarMembers, SizeNPC);
	ScopedTypeMember(VarMembers, Pets);
	ScopedTypeMember(VarMembers, SizePets);
	ScopedTypeMember(VarMembers, Mercs);
	ScopedTypeMember(VarMembers, SizeMercs);
	ScopedTypeMember(VarMembers, Corpse);
	ScopedTypeMember(VarMembers, SizeCorpse);
	ScopedTypeMember(VarMembers, Mounts);
	ScopedTypeMember(VarMembers, SizeMounts);
	ScopedTypeMember(VarMembers, Target);
	ScopedTypeMember(VarMembers, SizeTarget);
	ScopedTypeMember(VarMembers, Everything);
	ScopedTypeMember(VarMembers, Size);
	ScopedTypeMember(VarMembers, AutoSave);
	ScopedTypeMember(VarMembers, SizeByZone);
	ScopedTypeMember(VarMembers, SizeByRange);
	ScopedTypeMember(VarMembers, Range);
}

bool AutoSizeType::GetMember(MQVarPtr VarPtr, const char* Member, char* Index, MQTypeVar& Dest)
{
	MQTypeMember* pMember = AutoSizeType::FindMember(Member);
	if (!pMember) return false;

	AutoSizeSetting* setting = stringToTypeMap.find(pMember->Name) != stringToTypeMap.end() ? AutoSizeConfig.GetTypeSetting(stringToTypeMap.find(pMember->Name)->second) : nullptr;
	switch (static_cast<VarMembers>(pMember->ID))
	{
	case VarMembers::Self:
		// setting = AutoSizeConfig.GetTypeSetting(AS_SELF);
		Dest.DWord = setting->IsEnabled();
		Dest.Type = mq::datatypes::pBoolType;
		return true;
	case VarMembers::SizeSelf:
		// setting = AutoSizeConfig.GetTypeSetting(AS_SELF);
		Dest.Float = setting->GetSize();
		Dest.Type = mq::datatypes::pFloatType;
		return true;
	case VarMembers::PC:
		// setting = AutoSizeConfig.GetTypeSetting(AS_PC);
		Dest.DWord = setting->IsEnabled();
		Dest.Type = mq::datatypes::pBoolType;
		return true;
	case VarMembers::SizePC:
		// setting = AutoSizeConfig.GetTypeSetting(AS_PC);
		Dest.Float = setting->GetSize();
		Dest.Type = mq::datatypes::pFloatType;
		return true;
	case VarMembers::NPC:
		// setting = AutoSizeConfig.GetTypeSetting(AS_NPC);
		Dest.DWord = setting->IsEnabled();
		Dest.Type = mq::datatypes::pBoolType;
		return true;
	case VarMembers::SizeNPC:
		// setting = AutoSizeConfig.GetTypeSetting(AS_NPC);
		Dest.Float = setting->GetSize();
		Dest.Type = mq::datatypes::pFloatType;
		return true;
	case VarMembers::Pets:
		// setting = AutoSizeConfig.GetTypeSetting(AS_PETS);
		Dest.DWord = setting->IsEnabled();
		Dest.Type = mq::datatypes::pBoolType;
		return true;
	case VarMembers::SizePets:
		// setting = AutoSizeConfig.GetTypeSetting(AS_PETS);
		Dest.Float = setting->GetSize();
		Dest.Type = mq::datatypes::pFloatType;
		return true;
	case VarMembers::Corpse:
		// setting = AutoSizeConfig.GetTypeSetting(AS_CORPSES);
		Dest.DWord = setting->IsEnabled();
		Dest.Type = mq::datatypes::pBoolType;
		return true;
	case VarMembers::SizeCorpse:
		// setting = AutoSizeConfig.GetTypeSetting(AS_CORPSES);
		Dest.Float = setting->GetSize();
		Dest.Type = mq::datatypes::pFloatType;
		return true;
	case VarMembers::Mounts:
		// setting = AutoSizeConfig.GetTypeSetting(AS_MOUNTS);
		Dest.DWord = setting->IsEnabled();
		Dest.Type = mq::datatypes::pBoolType;
		return true;
	case VarMembers::SizeMounts:
		// setting = AutoSizeConfig.GetTypeSetting(AS_MOUNTS);
		Dest.Float = setting->GetSize();
		Dest.Type = mq::datatypes::pFloatType;
		return true;
	case VarMembers::Mercs:
		// setting = AutoSizeConfig.GetTypeSetting(AS_MERCS);
		Dest.DWord = setting->IsEnabled();
		Dest.Type = mq::datatypes::pBoolType;
		return true;
	case VarMembers::SizeMercs:
		// setting = AutoSizeConfig.GetTypeSetting(AS_MERCS);
		Dest.Float = setting->GetSize();
		Dest.Type = mq::datatypes::pFloatType;
		return true;
	case VarMembers::Target:
		// setting = AutoSizeConfig.GetTypeSetting(AS_TARGET);
		Dest.DWord = setting->IsEnabled();
		Dest.Type = mq::datatypes::pBoolType;
		return true;
	case VarMembers::SizeTarget:
		// setting = AutoSizeConfig.GetTypeSetting(AS_TARGET);
		Dest.Float = setting->GetSize();
		Dest.Type = mq::datatypes::pFloatType;
		return true;
	case VarMembers::Everything:
		// setting = AutoSizeConfig.GetTypeSetting(AS_EVERYTHING);
		Dest.DWord = setting->IsEnabled();
		Dest.Type = mq::datatypes::pBoolType;
		return true;
	case VarMembers::Size:
		// setting = AutoSizeConfig.GetTypeSetting(AS_EVERYTHING);
		Dest.Float = setting->GetSize();
		Dest.Type = mq::datatypes::pFloatType;
		return true;
	case VarMembers::AutoSave:
		Dest.DWord = AutoSizeConfig.autosave;
		Dest.Type = mq::datatypes::pBoolType;
		return true;
	case VarMembers::SizeByZone:
		Dest.DWord = AutoSizeConfig.byZoneEnabled;
		Dest.Type = mq::datatypes::pBoolType;
		return true;
	case VarMembers::SizeByRange:
		Dest.DWord = AutoSizeConfig.byRangeEnabled;
		Dest.Type = mq::datatypes::pBoolType;
		return true;
	case VarMembers::Range:
		Dest.Int64 = AutoSizeConfig.resizeRange;
		Dest.Type = mq::datatypes::pInt64Type;
		return true;
	}
	return false;
}

bool AutoSizeType::ToString(MQVarPtr VarPtr, char* Destination)
{
	strcpy_s(Destination, MAX_STRING, "AutoSize");
	return true;
}

bool dataAutoSize(const char* Index, MQTypeVar& Dest)
{
	Dest.DWord = 1;
	Dest.Type = pAutoSizeType;
	return true;
}

PLUGIN_API void InitializePlugin()
{
	EzDetour(PlayerZoneClient__ChangeHeight, &PlayerZoneClient_Hook::ChangeHeight_Detour, &PlayerZoneClient_Hook::ChangeHeight_Trampoline);

	AddCommand("/autosize", AutoSizeCmd);
	AutoSizeConfig.LoadINI();
	AddSettingsPanel("plugins/AutoSize", MQ2AutoSizeImGuiSettingsPanel);
	pAutoSizeType = new AutoSizeType;

	AddMQ2Data("AutoSize", dataAutoSize);
}

PLUGIN_API void ShutdownPlugin()
{
	RemoveDetour(PlayerZoneClient__ChangeHeight);

	RemoveCommand("/autosize");
	SpawnListResize(true);
	AutoSizeConfig.SaveINI();
	RemoveSettingsPanel("plugins/AutoSize");
	RemoveMQ2Data("AutoSize");

	delete pAutoSizeType;
}