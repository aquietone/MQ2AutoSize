// MQ2AutoSize.cpp : Resize spawns by distance or whole zone (client only)
//
// 06/09/2009: Fixed for changes to the eqgame function
//             added corpse option and autosave -pms
// 02/09/2009: added parameters, merc, npc & everything spawn options,
//             bug fixes and code cleanup - pms
// 06/28/2008: finds its own offset - ieatacid
//
// version 0.9.3 (by Psycotic)
// v1.0 - Eqmule 07-22-2016 - Added string safety.
//
//////////////////////////////////////////////////////////////////////////////
// Usage:
//   This plugin will automatically resize configured spawns to the specified
//   size. You can configure it to only resize within a specific range and then
//   resize back to normal when your distance moves out of that range.
//   Current default range is set to 50 and may be changed via INI or cmd line
//   NOTE:  These effects are CLIENT SIDE ONLY!
//
// Commands:
//  /autosize              - Toggles zone-wide  AutoSize on/off
//  /autosize dist         - Toggles distance-based AutoSize on/off
//  /autosize pc           - Toggles AutoSize PC spawn types
//  /autosize npc          - Toggles AutoSize NPC spawn types
//  /autosize pets         - Toggles AutoSize pet spawn types
//  /autosize mercs        - Toggles AutoSize mercenary spawn types
//  /autosize mounts       - Toggles AutoSize mounted player spawn types
//  /autosize corpse       - Toggles AutoSize corpse spawn types
//  /autosize target       - Resizes your target to sizetarget size
//  /autosize everything   - Toggles AutoSize all spawn types
//  /autosize self         - Toggles AutoSize for your character
//  /autosize range #      - Sets range for distance-based AutoSize
//
//  (Valid sizes 1 to 250)
//  /autosize size #       - Sets default size for "everything"
//  /autosize sizepc #     - Sets size for PC spawn types
//  /autosize sizenpc #    - Sets size for NPC spawn types
//  /autosize sizepets #   - Sets size for pet spawn types
//  /autosize sizetarget # - Sets size for target parameter
//  /autosize sizemercs #  - Sets size for mercenary spawn types
//  /autosize sizemounts # - Sets size for mounted player spawn types
//  /autosize sizecorpse # - Sets size for corpse spawn types
//  /autosize sizeself #   - Sets size for your character
//
//  /autosize status       - Display current plugin settings to chatwnd
//  /autosize help         - Display command syntax to chatwnd
//  /autosize save         - Save settings to INI file (auto on plugin unload)
//  /autosize load         - Load settings from INI file (auto on plugin load)
//  /autosize autosave     - Automatically save settings to INI file when an option is toggled or size is set
//
//////////////////////////////////////////////////////////////////////////////

#include <mq/Plugin.h>

const char* MODULE_NAME = "MQ2AutoSize";
PreSetup(MODULE_NAME);
PLUGIN_VERSION(1.0);
// this controls how many pulses to perform a radius-based resize (bad performance hit)
const int   SKIP_PULSES = 5;
// min and max size values
const float MIN_SIZE = 1.0f;
const float MAX_SIZE = 250.0f;

// used by the plugin
const float OTHER_SIZE = 1.0f;
const float ZERO_SIZE = 0.0f;
unsigned int uiSkipPulse = 0;
char szTemp[MAX_STRING] = { 0 };
void ResizeAll();

// our configuration
class COurSizes
{
public:
   COurSizes()
   {
      OptPC = OptByZone = true;
      OptNPC = OptPet = OptMerc = OptMount = OptCorpse = OptSelf = OptEverything = OptByRange = OptAutoSave = false;
      ResizeRange = 50;
      SizeDefault = SizePC = SizeNPC = SizePet = SizeMerc = SizeTarget = SizeMount = SizeCorpse = SizeSelf = 1.0f;
   };

   bool  OptAutoSave;
   bool  OptByRange;
   bool  OptByZone;
   bool  OptEverything;
   bool  OptPC;
   bool  OptNPC;
   bool  OptPet;
   bool  OptMerc;
   bool  OptMount;
   bool  OptCorpse;
   bool  OptSelf;

   int ResizeRange;

   float SizeDefault;
   float SizePC;
   float SizeNPC;
   float SizePet;
   float SizeMerc;
   float SizeTarget;
   float SizeMount;
   float SizeCorpse;
   float SizeSelf;
};
COurSizes AS_Config;

// offset pattern
unsigned long addrChangeHeight = NULL;
PBYTE patternChangeHeight = (PBYTE)"\xD9\x44\x24\x0C\x56\xD9\x44\x24\x0C\x8B\xF1";
char maskChangeHeight[] = "xxxxxxx?xxx";

// class to access the ChangeHeight function
class CSizeClass
{
public:
   void SizeFunc_Tramp(float, float, int, int);

   // this assures valid function call
   void ResizeWrapper(PSPAWNINFO pSpawn, float fNewSize)
   {
      float fView = OTHER_SIZE;
      int   iNotUsed = NULL;
      if (pSpawn->SpawnID == ((PSPAWNINFO)pLocalPlayer)->SpawnID || pSpawn->SpawnID == ((PSPAWNINFO)pCharSpawn)->SpawnID)
      {
         fView = ZERO_SIZE;
      }
      SizeFunc_Tramp(fNewSize, fView, iNotUsed, iNotUsed);
   };
};
FUNCTION_AT_ADDRESS(void CSizeClass::SizeFunc_Tramp(float, float, int, int), addrChangeHeight);

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

void LoadINI()
{
   GetPrivateProfileString("Config", "AutoSave", NULL, szTemp, 10, INIFileName);
   AS_Config.OptAutoSave = (!_strnicmp(szTemp, "on", 3));
   GetPrivateProfileString("Config", "ResizePC", "on", szTemp, 10, INIFileName);
   AS_Config.OptPC = (!_strnicmp(szTemp, "on", 3));
   GetPrivateProfileString("Config", "ResizeNPC", NULL, szTemp, 10, INIFileName);
   AS_Config.OptNPC = (!_strnicmp(szTemp, "on", 3));
   GetPrivateProfileString("Config", "ResizePets", NULL, szTemp, 10, INIFileName);
   AS_Config.OptPet = (!_strnicmp(szTemp, "on", 3));
   GetPrivateProfileString("Config", "ResizeMercs", NULL, szTemp, 10, INIFileName);
   AS_Config.OptMerc = (!_strnicmp(szTemp, "on", 3));
   GetPrivateProfileString("Config", "ResizeAll", NULL, szTemp, 10, INIFileName);
   AS_Config.OptEverything = (!_strnicmp(szTemp, "on", 3));
   GetPrivateProfileString("Config", "ResizeMounts", NULL, szTemp, 10, INIFileName);
   AS_Config.OptMount = (!_strnicmp(szTemp, "on", 3));
   GetPrivateProfileString("Config", "ResizeCorpse", NULL, szTemp, 10, INIFileName);
   AS_Config.OptCorpse = (!_strnicmp(szTemp, "on", 3));
   GetPrivateProfileString("Config", "ResizeSelf", NULL, szTemp, 10, INIFileName);
   AS_Config.OptSelf = (!_strnicmp(szTemp, "on", 3));

   GetPrivateProfileString("Config", "SizeByRange", NULL, szTemp, 10, INIFileName);
   AS_Config.OptByZone = !(AS_Config.OptByRange = (!_strnicmp(szTemp, "on", 3)));
   AS_Config.ResizeRange = GetPrivateProfileInt("Config", "Range", AS_Config.ResizeRange, INIFileName);

   // we cast more than a fisherman
   AS_Config.SizeDefault = SaneSize((float)GetPrivateProfileInt("Config", "SizeDefault", (int)MIN_SIZE, INIFileName));
   AS_Config.SizePC = SaneSize((float)GetPrivateProfileInt("Config", "SizePC", (int)MIN_SIZE, INIFileName));
   AS_Config.SizeNPC = SaneSize((float)GetPrivateProfileInt("Config", "SizeNPC", (int)MIN_SIZE, INIFileName));
   AS_Config.SizePet = SaneSize((float)GetPrivateProfileInt("Config", "SizePets", (int)MIN_SIZE, INIFileName));
   AS_Config.SizeMerc = SaneSize((float)GetPrivateProfileInt("Config", "SizeMercs", (int)MIN_SIZE, INIFileName));
   AS_Config.SizeTarget = SaneSize((float)GetPrivateProfileInt("Config", "SizeTarget", (int)MIN_SIZE, INIFileName));
   AS_Config.SizeMount = SaneSize((float)GetPrivateProfileInt("Config", "SizeMounts", (int)MIN_SIZE, INIFileName));
   AS_Config.SizeCorpse = SaneSize((float)GetPrivateProfileInt("Config", "SizeCorpse", (int)MIN_SIZE, INIFileName));
   AS_Config.SizeSelf = SaneSize((float)GetPrivateProfileInt("Config", "SizeSelf", (int)MIN_SIZE, INIFileName));

   // apply new INI read
   if (GetGameState() == GAMESTATE_INGAME && pLocalPlayer && AS_Config.OptByZone) ResizeAll();
}
template <unsigned int _Size>LPSTR SafeItoa(int _Value,char(&_Buffer)[_Size], int _Radix)
{
	errno_t err = _itoa_s(_Value, _Buffer, _Radix);
	if (!err) {
		return _Buffer;
	}
	return "";
}
void SaveINI()
{
   WritePrivateProfileString("Config", "AutoSave", AS_Config.OptAutoSave ? "on" : "off", INIFileName);
   WritePrivateProfileString("Config", "ResizePC", AS_Config.OptPC ? "on" : "off", INIFileName);
   WritePrivateProfileString("Config", "ResizeNPC", AS_Config.OptNPC ? "on" : "off", INIFileName);
   WritePrivateProfileString("Config", "ResizePets", AS_Config.OptPet ? "on" : "off", INIFileName);
   WritePrivateProfileString("Config", "ResizeMercs", AS_Config.OptMerc ? "on" : "off", INIFileName);
   WritePrivateProfileString("Config", "ResizeAll", AS_Config.OptEverything ? "on" : "off", INIFileName);
   WritePrivateProfileString("Config", "ResizeMounts", AS_Config.OptMount ? "on" : "off", INIFileName);
   WritePrivateProfileString("Config", "ResizeCorpse", AS_Config.OptCorpse ? "on" : "off", INIFileName);
   WritePrivateProfileString("Config", "ResizeSelf", AS_Config.OptSelf ? "on" : "off", INIFileName);
   WritePrivateProfileString("Config", "SizeByRange", AS_Config.OptByRange ? "on" : "off", INIFileName);
   WritePrivateProfileString("Config", "Range", SafeItoa(AS_Config.ResizeRange, szTemp, 10), INIFileName);
   WritePrivateProfileString("Config", "SizeDefault", SafeItoa((int)AS_Config.SizeDefault, szTemp, 10), INIFileName);
   WritePrivateProfileString("Config", "SizePC", SafeItoa((int)AS_Config.SizePC, szTemp, 10), INIFileName);
   WritePrivateProfileString("Config", "SizeNPC", SafeItoa((int)AS_Config.SizeNPC, szTemp, 10), INIFileName);
   WritePrivateProfileString("Config", "SizePets", SafeItoa((int)AS_Config.SizePet, szTemp, 10), INIFileName);
   WritePrivateProfileString("Config", "SizeMercs", SafeItoa((int)AS_Config.SizeMerc, szTemp, 10), INIFileName);
   WritePrivateProfileString("Config", "SizeTarget", SafeItoa((int)AS_Config.SizeTarget, szTemp, 10), INIFileName);
   WritePrivateProfileString("Config", "SizeMounts", SafeItoa((int)AS_Config.SizeMount, szTemp, 10), INIFileName);
   WritePrivateProfileString("Config", "SizeCorpse", SafeItoa((int)AS_Config.SizeCorpse, szTemp, 10), INIFileName);
   WritePrivateProfileString("Config", "SizeSelf", SafeItoa((int)AS_Config.SizeSelf, szTemp, 10), INIFileName);
}

void ChangeSize(PSPAWNINFO pChangeSpawn, float fNewSize)
{
   if (GetGameState() != GAMESTATE_INGAME || !pChangeSpawn || !pChangeSpawn->SpawnID) return;
   ((CSizeClass*)pChangeSpawn)->ResizeWrapper(pChangeSpawn, fNewSize);
}

void SizePasser(PSPAWNINFO pSpawn, bool bReset)
{
   if ((!bReset && !AS_Config.OptByZone && !AS_Config.OptByRange) || GetGameState() != GAMESTATE_INGAME) return;
   PSPAWNINFO pChSpawn = (PSPAWNINFO)pCharSpawn;
   PSPAWNINFO pLPlayer = (PSPAWNINFO)pLocalPlayer;
   if (!pLPlayer || !pChSpawn->SpawnID || !pSpawn || !pSpawn->SpawnID) return;

   if (pSpawn->SpawnID == pLPlayer->SpawnID)
   {
      if (AS_Config.OptSelf) ChangeSize(pSpawn, bReset ? ZERO_SIZE : AS_Config.SizeSelf);
      return;
   }

   switch (GetSpawnType(pSpawn))
   {
   case PC:
      if (AS_Config.OptPC)
      {
         ChangeSize(pSpawn, bReset ? ZERO_SIZE : AS_Config.SizePC);
         return;
      }
      break;
   case NPC:
      if (AS_Config.OptNPC)
      {
         ChangeSize(pSpawn, bReset ? ZERO_SIZE : AS_Config.SizeNPC);
         return;
      }
      break;
   case PET:
      if (AS_Config.OptPet)
      {
         ChangeSize(pSpawn, bReset ? ZERO_SIZE : AS_Config.SizePet);
         return;
      }
      break;
   case MERCENARY:
      if (AS_Config.OptMerc)
      {
         ChangeSize(pSpawn, bReset ? ZERO_SIZE : AS_Config.SizeMerc);
         return;
      }
      break;
   case MOUNT:
      if (AS_Config.OptMount && pSpawn->SpawnID != pChSpawn->SpawnID)
      {
         ChangeSize(pSpawn, bReset ? ZERO_SIZE : AS_Config.SizeMount);
         return;
      }
      break;
   case CORPSE:
      if (AS_Config.OptCorpse)
      {
         ChangeSize(pSpawn, bReset ? ZERO_SIZE : AS_Config.SizeCorpse);
         return;
      }
      break;
   default:
      break;
   }

   if (AS_Config.OptEverything && pSpawn->SpawnID != pChSpawn->SpawnID) ChangeSize(pSpawn, bReset ? ZERO_SIZE : AS_Config.SizeDefault);
}

void ResetAllByType(eSpawnType OurType)
{
   PSPAWNINFO pSpawn = (PSPAWNINFO)pSpawnList;
   PSPAWNINFO pChSpawn = (PSPAWNINFO)pCharSpawn;
   PSPAWNINFO pLPlayer = (PSPAWNINFO)pLocalPlayer;
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
   PSPAWNINFO pSpawn = (PSPAWNINFO)pSpawnList;
   while (pSpawn)
   {
      SizePasser(pSpawn, bReset);
      pSpawn = pSpawn->pNext;
   }
}

void ResizeAll()
{
   SpawnListResize(false);
}

void ResetAll()
{
   SpawnListResize(true);
}

PLUGIN_API void OnAddSpawn(PSPAWNINFO pNewSpawn)
{
   if (AS_Config.OptByZone) SizePasser(pNewSpawn, false);
}

PLUGIN_API void OnEndZone()
{
   ResizeAll();
}

PLUGIN_API void OnPulse()
{
   if (GetGameState() != GAMESTATE_INGAME || !AS_Config.OptByRange) return;
   if (uiSkipPulse < SKIP_PULSES)
   {
      uiSkipPulse++;
      return;
   }

   PSPAWNINFO pAllSpawns = (PSPAWNINFO)pSpawnList;
   float fDist = 0.0f;
   uiSkipPulse = 0;

   while (pAllSpawns)
   {
      fDist = GetDistance((PSPAWNINFO)pLocalPlayer, pAllSpawns);
      if (fDist < AS_Config.ResizeRange)
      {
         SizePasser(pAllSpawns, false);
      }
      else if (fDist < AS_Config.ResizeRange + 50)
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
   WriteChatf("  \ag/autosize\ax [ \aypc\ax | \aynpc\ax | \aypets\ax | \aymercs\ax | \aymounts\ax | \aycorpse\ax | \aytarget\ax | \ayeverything\ax | \ayself\ax ]");
   WriteChatf("--- Valid Size Syntax (1 to 250) ---");
   WriteChatf("  \ag/autosize\ax [ \aysize\ax | \aysizepc\ax | \aysizenpc\ax | \aysizepets\ax | \aysizemercs\ax | \aysizemounts\ax | \aysizecorpse\ax | \aysizetarget\ax | \aysizeself\ax ] [ \ay#\ax ]");
   WriteChatf("--- Other Valid Commands ---");
   WriteChatf("  \ag/autosize\ax [ \ayhelp\ax | \aystatus\ax | \ayautosave\ax | \aysave\ax | \ayload\ax ]");
}

void OutputStatus()
{
   char szMethod[100] = { 0 };
   char szOn[10] = "\agon\ax";
   char szOff[10] = "\aroff\ax";
   if (AS_Config.OptByZone)
   {
      sprintf_s(szMethod, "\ayZonewide\ax");
   }
   else if (AS_Config.OptByRange)
   {
      sprintf_s(szMethod, "\ayRange\ax) RangeSize(\ag%d\ax", AS_Config.ResizeRange);
   }
   else
   {
      sprintf_s(szMethod, "\arInactive\ax");
   }

   WriteChatf("\ay%s\aw:: Current Status -- Method: (%s)%s", MODULE_NAME, szMethod, AS_Config.OptAutoSave ? " \agAUTOSAVING" : "");
   WriteChatf("Toggles: PC(%s) NPC(%s) Pets(%s) Mercs(%s) Mounts(%s) Corpses(%s) Self(%s) Everything(%s) ", AS_Config.OptPC ? szOn : szOff, AS_Config.OptNPC ? szOn : szOff, AS_Config.OptPet ? szOn : szOff, AS_Config.OptMerc ? szOn : szOff, AS_Config.OptMount ? szOn : szOff, AS_Config.OptCorpse ? szOn : szOff, AS_Config.OptSelf ? szOn : szOff, AS_Config.OptEverything ? szOn : szOff);
   WriteChatf("Sizes: PC(\ag%.0f\ax) NPC(\ag%.0f\ax) Pets(\ag%.0f\ax) Mercs(\ag%.0f\ax) Mounts(\ag%.0f\ax) Corpses(\ag%.0f\ax) Target(\ag%.0f\ax) Self(\ag%.0f\ax) Everything(\ag%.0f\ax)", AS_Config.SizePC, AS_Config.SizeNPC, AS_Config.SizePet, AS_Config.SizeMerc, AS_Config.SizeMount, AS_Config.SizeCorpse, AS_Config.SizeTarget, AS_Config.SizeSelf, AS_Config.SizeDefault);
}

bool ToggleOption(const char* pszToggleOutput, bool* pbOption)
{
   *pbOption = !*pbOption;
   WriteChatf("\ay%s\aw:: Option (\ay%s\ax) now %s\ax", MODULE_NAME, pszToggleOutput, *pbOption ? "\agenabled" : "\ardisabled");
   if (AS_Config.OptAutoSave) SaveINI();
   return *pbOption;
}

void SetSizeConfig(const char* pszOption, float fNewSize, float* fChangeThis)
{
   if (fNewSize >= MIN_SIZE && fNewSize <= MAX_SIZE)
   {
      float fPrevSize = *fChangeThis;
      *fChangeThis = fNewSize;
      WriteChatf("\ay%s\aw:: %s size changed from \ay%.0f\ax to \ag%.0f", MODULE_NAME, pszOption, fPrevSize, *fChangeThis);
   }
   else
   {
      WriteChatf("\ay%s\aw:: %s size is \ag%.0f\ax (was not modified)", MODULE_NAME, pszOption, *fChangeThis);
   }
   if (AS_Config.OptAutoSave) SaveINI();
}

void AutoSizeCmd(PSPAWNINFO pLPlayer, char* szLine)
{
   char szCurArg[MAX_STRING] = { 0 };
   char szNumber[MAX_STRING] = { 0 };
   GetArg(szCurArg, szLine, 1);
   GetArg(szNumber, szLine, 2);
   float fNewSize = (float)atof(szNumber);

   if (!*szCurArg)
   {
      AS_Config.OptByZone = !AS_Config.OptByZone;
      if (AS_Config.OptByZone)
      {
         if (AS_Config.OptByRange)
         {
            AS_Config.OptByRange = false;
            WriteChatf("\ay%s\aw:: AutoSize (\ayRange\ax) now \ardisabled\ax!", MODULE_NAME);
         }
         ResizeAll();
      }
      else
      {
         ResetAll();
      }
      WriteChatf("\ay%s\aw:: AutoSize (\ayZonewide\ax) now %s\ax!", MODULE_NAME, AS_Config.OptByZone ? "\agenabled" : "\ardisabled");
      if (AS_Config.OptAutoSave) SaveINI();
   }
   else
   {
      if (!_strnicmp(szCurArg, "dist", 5))
      {
         AS_Config.OptByRange = !AS_Config.OptByRange;
         if (AS_Config.OptByRange)
         {
            if (AS_Config.OptByZone)
            {
               AS_Config.OptByZone = false;
               WriteChatf("\ay%s\aw:: AutoSize (\ayAllZone\ax) now \ardisabled\ax!", MODULE_NAME);
            }
         }
         else
         {
            ResetAll();
         }
         WriteChatf("\ay%s\aw:: AutoSize (\ayRange\ax) now %s\ax!", MODULE_NAME, AS_Config.OptByRange ? "\agenabled" : "\ardisabled");
         if (AS_Config.OptAutoSave) SaveINI();
         return;
      }
      else if (!_strnicmp(szCurArg, "save", 5))
      {
         SaveINI();
         WriteChatf("\ay%s\aw:: Configuration file saved.", MODULE_NAME);
         return;
      }
      else if (!_strnicmp(szCurArg, "load", 5))
      {
         LoadINI();
         WriteChatf("\ay%s\aw:: Configuration file loaded.", MODULE_NAME);
         return;
      }
      else if (!_strnicmp(szCurArg, "autosave", 9))
      {
         ToggleOption("Autosave", &AS_Config.OptAutoSave);
         return;
      }
      else if (!_strnicmp(szCurArg, "range", 6))
      {
         if (atoi(szCurArg) > 0)
         {
            AS_Config.ResizeRange = atoi(szCurArg);
            WriteChatf("\ay%s\aw:: Range set to \ag%d", MODULE_NAME, AS_Config.ResizeRange);
         }
         else
         {
            WriteChatf("\ay%s\aw:: Range is \ag%d\ax (was not modified)", MODULE_NAME, AS_Config.ResizeRange);
         }
         if (AS_Config.OptAutoSave) SaveINI();
         return;
      }
      else if (!_strnicmp(szCurArg, "size", 5))
      {
         SetSizeConfig("Default", fNewSize, &AS_Config.SizeDefault);
      }
      else if (!_strnicmp(szCurArg, "sizepc", 7))
      {
         SetSizeConfig("PC", fNewSize, &AS_Config.SizePC);
      }
      else if (!_strnicmp(szCurArg, "sizenpc", 8))
      {
         SetSizeConfig("NPC", fNewSize, &AS_Config.SizeNPC);
      }
      else if (!_strnicmp(szCurArg, "sizepets", 9))
      {
         SetSizeConfig("Pet", fNewSize, &AS_Config.SizePet);
      }
      else if (!_strnicmp(szCurArg, "sizemercs", 10))
      {
         SetSizeConfig("Mercs", fNewSize, &AS_Config.SizeMerc);
      }
      else if (!_strnicmp(szCurArg, "sizetarget", 11))
      {
         SetSizeConfig("Target", fNewSize, &AS_Config.SizeTarget);
      }
      else if (!_strnicmp(szCurArg, "sizemounts", 11))
      {
         SetSizeConfig("Mounts", fNewSize, &AS_Config.SizeMount);
      }
      else if (!_strnicmp(szCurArg, "sizecorpse", 11))
      {
         SetSizeConfig("Corpses", fNewSize, &AS_Config.SizeCorpse);
      }
      else if (!_strnicmp(szCurArg, "sizeself", 9))
      {
         SetSizeConfig("Self", fNewSize, &AS_Config.SizeSelf);
      }
      else if (!_strnicmp(szCurArg, "pc", 3))
      {
         if (!ToggleOption("PC", &AS_Config.OptPC)) ResetAllByType(PC);
      }
      else if (!_strnicmp(szCurArg, "npc", 4))
      {
         if (!ToggleOption("NPC", &AS_Config.OptNPC)) ResetAllByType(NPC);
      }
      else if (!_strnicmp(szCurArg, "everything", 11))
      {
         if (!ToggleOption("Everything", &AS_Config.OptEverything)) ResetAll();
      }
      else if (!_strnicmp(szCurArg, "pets", 5))
      {
         if (!ToggleOption("Pets", &AS_Config.OptPet)) ResetAllByType(PET);
      }
      else if (!_strnicmp(szCurArg, "mercs", 6))
      {
         if (!ToggleOption("Mercs", &AS_Config.OptMerc)) ResetAllByType(MERCENARY);
      }
      else if (!_strnicmp(szCurArg, "mounts", 7))
      {
         if (!ToggleOption("Mounts", &AS_Config.OptMount)) ResetAllByType(MOUNT);
      }
      else if (!_strnicmp(szCurArg, "corpse", 7))
      {
         if (!ToggleOption("Corpses", &AS_Config.OptCorpse)) ResetAllByType(CORPSE);
      }
      else if (!_strnicmp(szCurArg, "target", 7))
      {
         PSPAWNINFO pTheTarget = (PSPAWNINFO)pTarget;
         if (pTheTarget && GetGameState() == GAMESTATE_INGAME && pTheTarget->SpawnID)
         {
            ChangeSize(pTheTarget, AS_Config.SizeTarget);
            char szTarName[MAX_STRING] = { 0 };
            sprintf_s(szTarName, "%s", pTheTarget->DisplayedName);
            WriteChatf("\ay%s\aw:: Resized \ay%s\ax to \ag%.0f\ax", MODULE_NAME, szTarName, AS_Config.SizeTarget);
         }
         else
         {
            WriteChatf("\ay%s\aw:: \arYou must have a target to use this parameter.", MODULE_NAME);
         }
         return;
      }
      else if (!_strnicmp(szCurArg, "self", 5))
      {
         if (!ToggleOption("Self", &AS_Config.OptSelf))
         {
            if (((PSPAWNINFO)pLocalPlayer)->Mount) ChangeSize((PSPAWNINFO)pLocalPlayer, ZERO_SIZE);
            else ChangeSize((PSPAWNINFO)pCharSpawn, ZERO_SIZE);
         }
      }
      else if (!_strnicmp(szCurArg, "help", 5))
      {
         OutputHelp();
         return;
      }
      else if (!_strnicmp(szCurArg, "status", 7))
      {
         OutputStatus();
         return;
      }
      else
      {
         WriteChatf("\ay%s\aw:: \arInvalid command parameter.", MODULE_NAME);
         return;
      }

      // if size change or everything, pets, mercs,mounts toggled and won't be handled onpulse
      if (AS_Config.OptByZone) ResizeAll();
   }
}

PLUGIN_API void InitializePlugin()
{
   addrChangeHeight = PlayerZoneClient__ChangeHeight;

   if (addrChangeHeight)
   {
      AddCommand("/autosize", AutoSizeCmd);
      LoadINI();
      //ResizeAll();
   }
   else
   {
      WriteChatf("\ay%s\aw:: \arError:\ax Couldn't find offset. Unloading.", MODULE_NAME);
      EzCommand("/timed 1 /plugin mq2autosize unload");
   }
}

PLUGIN_API void ShutdownPlugin()
{
   if (addrChangeHeight)
   {
      RemoveCommand("/autosize");
      ResetAll();
      SaveINI();
   }
}