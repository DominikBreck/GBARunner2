#include "vram.h"
#include "vramheap.h"
#include "sd_access.h"
#include "string.h"
#include "fat/ff.h"
#include "INIReader.h"
#include "INIWriter.h"
#include "settings.h"

static const char* sSettingsFilePath = "0:/_gba/gbarunner2.ini";

static const char* sEmulationSectionName = "emulation";

//settings are globals such that they can be easily accessed from assembly
static const char* sEmuSettingUseBottomScreenName = "useBottomScreen";
u32 gEmuSettingUseBottomScreen;

static const char* sEmuSettingAutoSaveName = "autoSave";
u32 gEmuSettingAutoSave;

static const char* sEmuSettingFrameName = "frame";
u32 gEmuSettingFrame;

static const char* sEmuSettingCenterMaskName = "centerMask";
u32 gEmuSettingCenterMask;

static const char* sEmuSettingGbaColorsName = "gbaColors";
u32 gEmuSettingGbaColors;

static const char* sEmuSettingMainMemICacheName = "mainMemICache";
u32 gEmuSettingMainMemICache;

static const char* sEmuSettingWramICacheName = "wramICache";
u32 gEmuSettingWramICache;

static const char* sEmuSettingSkipIntroName = "skipIntro";
u32 gEmuSettingSkipIntro;

static const char* sLinkSectionName = "link";
static const char* sLinkSettingMasterMac = "masterMac";
static const char* sLinkSettingSlaveMac = "slaveMac";

static const char* sInputSectionName = "input";
settings_input gInputSettings;
static const char* sInputSettingAButtonName = "buttonA";
static const char* sInputSettingBButtonName = "buttonB";
static const char* sInputSettingLButtonName = "buttonL";
static const char* sInputSettingRButtonName = "buttonR";
static const char* sInputSettingStartButtonName = "buttonStart";
static const char* sInputSettingSelectButtonName = "buttonSelect";

static void loadDefaultSettings()
{
    gEmuSettingUseBottomScreen = false;
    gEmuSettingAutoSave = true;
    gEmuSettingFrame = true;
    gEmuSettingCenterMask = true;
    gEmuSettingMainMemICache = true;
    gEmuSettingWramICache = true;
    gEmuSettingGbaColors = false;
    gEmuSettingSkipIntro = false;

    vram_cd->sioWork.masterMac[0] = 0;
    vram_cd->sioWork.masterMac[1] = 0;
    vram_cd->sioWork.masterMac[2] = 0;
    vram_cd->sioWork.masterMac[3] = 0;
    vram_cd->sioWork.masterMac[4] = 0;
    vram_cd->sioWork.masterMac[5] = 0;

    vram_cd->sioWork.slaveMac[0] = 0;
    vram_cd->sioWork.slaveMac[1] = 0;
    vram_cd->sioWork.slaveMac[2] = 0;
    vram_cd->sioWork.slaveMac[3] = 0;
    vram_cd->sioWork.slaveMac[4] = 0;
    vram_cd->sioWork.slaveMac[5] = 0;

    gInputSettings.buttonA = 0;
    gInputSettings.buttonB = 1;
    gInputSettings.buttonL = 9;
    gInputSettings.buttonR = 8;
    gInputSettings.buttonStart = 3;
    gInputSettings.buttonSelect = 2;
}

static bool parseBoolean(const char* str, bool def = false)
{
    if(!strcmp(str, "true"))
        return true;
    else if(!strcmp(str, "false"))
        return false;
    return def;
}

static void parseMacAddress(const char* str, u8* dst)
{
    int len = strlen(str);
    if(len != 12)
        return;
    for(int i = 0; i < 12; i += 2)
    {
        char topChar = to_upper(str[i]);
        int top;
        if(topChar >= '0' && topChar <= '9')
            top = topChar - '0';
        else
            top = topChar - 'A' + 0xA;
        char bottomChar = to_upper(str[i + 1]);
        int bottom;
        if(bottomChar >= '0' && bottomChar <= '9')
            bottom = bottomChar - '0';
        else
            bottom = bottomChar - 'A' + 0xA;
        dst[i >> 1] = (top << 4) | bottom;
    }
}

static u32 parseUInt(const char* str)
{
    u32 result = 0;
    char c;
    while((c = *str++) != 0)
    {
        result = result * 10 + (c - '0');
    }
    return result;
}

static void iniPropertyCallback(void* arg, const char* section, const char* key, const char* value)
{
    if(!strcmp(section, sEmulationSectionName))
    {
        if(!strcmp(key, sEmuSettingUseBottomScreenName))
            gEmuSettingUseBottomScreen = parseBoolean(value, false);
        else if(!strcmp(key, sEmuSettingAutoSaveName))
            gEmuSettingAutoSave = parseBoolean(value, true);
        else if(!strcmp(key, sEmuSettingFrameName))
            gEmuSettingFrame = parseBoolean(value, true);
        else if(!strcmp(key, sEmuSettingCenterMaskName))
            gEmuSettingCenterMask = parseBoolean(value, true);
        else if(!strcmp(key, sEmuSettingMainMemICacheName))
            gEmuSettingMainMemICache = parseBoolean(value, true);
        else if(!strcmp(key, sEmuSettingWramICacheName))
            gEmuSettingWramICache = parseBoolean(value, true);
        else if(!strcmp(key, sEmuSettingGbaColorsName))
            gEmuSettingGbaColors = parseBoolean(value, false);
        else if(!strcmp(key, sEmuSettingSkipIntroName))
            gEmuSettingSkipIntro = parseBoolean(value, false);
    }
    else if(!strcmp(section, sLinkSectionName))
    {
        if(!strcmp(key, sLinkSettingMasterMac))
            parseMacAddress(value, vram_cd->sioWork.masterMac);
        else if(!strcmp(key, sLinkSettingSlaveMac))
            parseMacAddress(value, vram_cd->sioWork.slaveMac);
    }
    else if(!strcmp(section, sInputSectionName))
    {
        if(!strcmp(key, sInputSettingAButtonName))
            gInputSettings.buttonA = parseUInt(value);
        else if(!strcmp(key, sInputSettingBButtonName))
            gInputSettings.buttonB = parseUInt(value);
        else if(!strcmp(key, sInputSettingLButtonName))
            gInputSettings.buttonL = parseUInt(value);
        else if(!strcmp(key, sInputSettingRButtonName))
            gInputSettings.buttonR = parseUInt(value);
        else if(!strcmp(key, sInputSettingStartButtonName))
            gInputSettings.buttonStart = parseUInt(value);
        else if(!strcmp(key, sInputSettingSelectButtonName))
            gInputSettings.buttonSelect = parseUInt(value);
    }
}

void settings_initialize()
{
    loadDefaultSettings();
    if (f_open(&vram_cd->fil, sSettingsFilePath, FA_OPEN_EXISTING | FA_READ) != FR_OK)
		return;
    INIReader::Parse(&vram_cd->fil, iniPropertyCallback, NULL);
    f_close(&vram_cd->fil);
}

bool settings_save()
{
    if (f_open(&vram_cd->fil, sSettingsFilePath, FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)
        return false;
    INIWriter writer = INIWriter(&vram_cd->fil);
    writer.WriteSection(sEmulationSectionName);
    writer.WriteBooleanProperty(sEmuSettingUseBottomScreenName, gEmuSettingUseBottomScreen);
    //writer.WriteBooleanProperty(sEmuSettingAutoSaveName, gEmuSettingAutoSave);
    writer.WriteBooleanProperty(sEmuSettingFrameName, gEmuSettingFrame);
    writer.WriteBooleanProperty(sEmuSettingCenterMaskName, gEmuSettingCenterMask);
    writer.WriteBooleanProperty(sEmuSettingMainMemICacheName, gEmuSettingMainMemICache);
    writer.WriteBooleanProperty(sEmuSettingWramICacheName, gEmuSettingWramICache);
    writer.WriteBooleanProperty(sEmuSettingGbaColorsName, gEmuSettingGbaColors);
    writer.WriteBooleanProperty(sEmuSettingSkipIntroName, gEmuSettingSkipIntro);

    writer.WriteSection(sLinkSectionName);
    writer.WriteMacAddressProperty(sLinkSettingMasterMac, vram_cd->sioWork.masterMac);
    writer.WriteMacAddressProperty(sLinkSettingSlaveMac, vram_cd->sioWork.slaveMac);

    writer.WriteSection(sInputSectionName);
    writer.WriteIntegerProperty(sInputSettingAButtonName, gInputSettings.buttonA);
    writer.WriteIntegerProperty(sInputSettingBButtonName, gInputSettings.buttonB);
    writer.WriteIntegerProperty(sInputSettingLButtonName, gInputSettings.buttonL);
    writer.WriteIntegerProperty(sInputSettingRButtonName, gInputSettings.buttonR);
    writer.WriteIntegerProperty(sInputSettingStartButtonName, gInputSettings.buttonStart);
    writer.WriteIntegerProperty(sInputSettingSelectButtonName, gInputSettings.buttonSelect);
    f_close(&vram_cd->fil);
    return true;
}