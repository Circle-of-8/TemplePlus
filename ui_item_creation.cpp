
#include "stdafx.h"
#include "ui.h"
#include "fixes.h"
#include "tig_mes.h"
#include "temple_functions.h"

struct UiConf {
	bool editor; // True for worlded it seems
	int width; // Screen width
	int height; // Screen height
	uint32_t field_c;
	uint32_t field_10;
	uint32_t field_14;
};

typedef int (__cdecl *UiSystemInit)(const UiConf *conf);
typedef void(__cdecl *UiSystemReset)();
typedef bool (__cdecl *UiSystemLoadModule)();
typedef void (__cdecl *UiSystemUnloadModule)();
typedef void (__cdecl *UiSystemShutdown)();
typedef bool (__cdecl *UiSystemSaveGame)(void *tioFile);
typedef bool (__cdecl *UiSystemLoadGame)(void *sth);

struct UiSystemSpec {
	const char *name;
	UiSystemInit init;
	UiSystemReset reset;
	UiSystemLoadModule loadModule;
	UiSystemUnloadModule unloadModule;
	UiSystemShutdown shutdown;
	UiSystemSaveGame savegame;
	UiSystemLoadGame loadgame;
	uint32_t field_20;
};

struct ImgFile : public TempleAlloc {
	int tilesX;
	int tilesY;
	int width;
	int height;
	int textureIds[16];
	int field_50;
};

enum class UiAssetType : uint32_t {
	Portraits = 0,
	Inventory,
	Generic, // Textures
	GenericLarge // IMG files
};

enum class UiGenericAsset : uint32_t {
	AcceptHover = 0,
	AcceptNormal,
	AcceptPressed,
	DeclineHover,
	DeclineNormal,
	DeclinePressed,
	DisabledNormal,
	GenericDialogueCheck
};

struct UiFuncs : AddressTable {
	ImgFile *(__cdecl *LoadImg)(const char *filename);

	bool GetAsset(UiAssetType assetType, UiGenericAsset assetIndex, int *textureIdOut) {
		return _GetAsset(assetType, (uint32_t)assetIndex, textureIdOut, 0) == 0;
	}

	void rebase(Rebaser rebase) override {
		rebase(LoadImg, 0x101E8320);
		rebase(_GetAsset, 0x1004A360);
	}

private:
	int(__cdecl *_GetAsset)(UiAssetType assetType, uint32_t assetIndex, int *textureIdOut, int offset);
} uiFuncs;

struct UiSystemSpecs {
	UiSystemSpec systems[43];
};
static GlobalStruct<UiSystemSpecs, 0x102F6C10> templeUiSystems;

class UiSystem {
public:

	static UiSystemSpec *getUiSystem(const char *name) {
		// Search for the ui system to replace
		for (auto &system : templeUiSystems->systems) {
			if (!strcmp(name, system.name)) {
				return &system;
			}
		}

		LOG(error) << "Couldn't find UI system " << name << "! Replacement failed.";
		return nullptr;
	}
};

struct ButtonStateTextures {
	int normal;
	int hover;
	int pressed;
	ButtonStateTextures() : normal(-1), hover(-1), pressed(-1) {}

	void loadAccept() {
		uiFuncs.GetAsset(UiAssetType::Generic, UiGenericAsset::AcceptNormal, &normal);
		uiFuncs.GetAsset(UiAssetType::Generic, UiGenericAsset::AcceptHover, &hover);
		uiFuncs.GetAsset(UiAssetType::Generic, UiGenericAsset::AcceptPressed, &pressed);
	}

	void loadDecline() {
		uiFuncs.GetAsset(UiAssetType::Generic, UiGenericAsset::DeclineNormal, &normal);
		uiFuncs.GetAsset(UiAssetType::Generic, UiGenericAsset::DeclineHover, &hover);
		uiFuncs.GetAsset(UiAssetType::Generic, UiGenericAsset::DeclinePressed, &pressed);
	}
};

static MesHandle mesItemCreationText;
static MesHandle mesItemCreationRules;
static MesHandle mesItemCreationNamesText;
static ImgFile *background = nullptr;
static ButtonStateTextures acceptBtnTextures;
static ButtonStateTextures declineBtnTextures;
static int disabledBtnTexture;

enum class StringTokenType : uint32_t {
	Unk0 = 0,
	Identifier,
	Number,
	QuotedString,
	Unk4,
	Unk5
};

struct StringToken {
	StringTokenType type;
	uint32_t field_4;
	double numberFloat;
	int numberInt;
	const char *text;
	uint32_t field_18;
	uint32_t tokenStart;
	uint32_t tokenEnd; // Exclusive
	const char *originalStr;
};

struct StringTokenizerFuncs : AddressTable {
	int (__cdecl *Create)(const char *line, int *strTokenId);
	int (__cdecl *NextToken)(int *tokenizerId, StringToken *tokenOut);
	int (__cdecl *Destroy)(int *tokenizerId);

	void rebase(Rebaser rebase) override {
		rebase(Create, 0x101F2350);
		rebase(NextToken, 0x101F25F0);
		rebase(Destroy, 0x101F2560);
	}
} stringTokenizerFuncs;

// Uses the ToEE internal string tokenizer
class StringTokenizer {
public:
	StringTokenizer(const string &line) : mTokenizerId(0) {
		int errorCode;
		if ((errorCode = stringTokenizerFuncs.Create(line.c_str(), &mTokenizerId)) != 0) {
			LOG(error) << "Unable to create tokenizer for string " << line << ": " << errorCode;
		}
	}

	~StringTokenizer() {
		stringTokenizerFuncs.Destroy(&mTokenizerId);
		mTokenizerId = 0;
	}

	bool next() {
		return !stringTokenizerFuncs.NextToken(&mTokenizerId, &mToken);
	}

	const StringToken &token() const {
		return mToken;
	}
private:
	int mTokenizerId;
	StringToken mToken;
};

enum class ItemCreationType : uint32_t {
	Alchemy = 0,
	Potion,
	Scroll,
	Wand,
	Rod,
	Wondrous,
	ArmsAndArmor,
	Unk7,
	Unk8
};

static vector<uint64_t> craftingProtoHandles[8];

const char *getProtoName(uint64_t protoHandle) {
	/*
	 // gets item creation proto id
  if ( sub_1009C950((ObjHndl)protoHandle) )
    v1 = sub_100392E0(protoHandle);
  else
    v1 = sub_10039320((ObjHndl)protoHandle);

  line.key = v1;
  if ( tig_mes_get_line(ui_itemcreation_names, &line) )
    result = line.value;
  else
    result = Obj_Get_DisplayName((ObjHndl)protoHandle, (ObjHndl)protoHandle);
  return result;
  */

	return templeFuncs.Obj_Get_DisplayName(protoHandle, protoHandle);
}

static void loadProtoIds(MesHandle mesHandle) {

	for (uint32_t i = 0; i < 8; ++i) {
		auto protoLine = mesFuncs.GetLineById(mesHandle, i);
		if (!protoLine) {
			continue;
		}

		auto &protoHandles = craftingProtoHandles[i];

		StringTokenizer tokenizer(protoLine);
		while (tokenizer.next()) {
			auto handle = templeFuncs.GetProtoHandle(tokenizer.token().numberInt);
			protoHandles.push_back(handle);
		}

		// Sort by prototype name
		sort(protoHandles.begin(), protoHandles.end(), [](uint64_t a, uint64_t b)
		{
			auto nameA = getProtoName(a);
			auto nameB = getProtoName(b);
			return _strcmpi(nameA, nameB);
		});
		LOG(info) << "Loaded " << craftingProtoHandles[i].size() << " prototypes for crafting type " << i;
	}
	
}

static int __cdecl systemInit(const UiConf *conf) {

	mesFuncs.Open("mes\\item_creation.mes", &mesItemCreationText);
	mesFuncs.Open("mes\\item_creation_names.mes", &mesItemCreationNamesText);
	mesFuncs.Open("rules\\item_creation.mes", &mesItemCreationRules);
	loadProtoIds(mesItemCreationRules);

	acceptBtnTextures.loadAccept();
	declineBtnTextures.loadDecline();
	uiFuncs.GetAsset(UiAssetType::Generic, UiGenericAsset::DisabledNormal, &disabledBtnTexture);

	background = uiFuncs.LoadImg("art\\interface\\item_creation_ui\\item_creation.img");

	// TODO !sub_10150F00("rules\\item_creation.mes")

	/*
	tig_texture_register("art\\interface\\item_creation_ui\\craftarms_0.tga", &dword_10BEE38C)
    || tig_texture_register("art\\interface\\item_creation_ui\\craftarms_1.tga", &dword_10BECEE8)
    || tig_texture_register("art\\interface\\item_creation_ui\\craftarms_2.tga", &dword_10BED988)
    || tig_texture_register("art\\interface\\item_creation_ui\\craftarms_3.tga", &dword_10BECEEC)
    || tig_texture_register("art\\interface\\item_creation_ui\\invslot_selected.tga", &dword_10BECDAC)
    || tig_texture_register("art\\interface\\item_creation_ui\\invslot.tga", &dword_10BEE038)
    || tig_texture_register("art\\interface\\item_creation_ui\\add_button.tga", &dword_10BEE334)
    || tig_texture_register("art\\interface\\item_creation_ui\\add_button_grey.tga", &dword_10BED990)
    || tig_texture_register("art\\interface\\item_creation_ui\\add_button_hover.tga", &dword_10BEE2D8)
    || tig_texture_register("art\\interface\\item_creation_ui\\add_button_press.tga", &dword_10BED79C) )
	*/

	return 0;
}

static void __cdecl systemReset() {
}

static void __cdecl systemExit() {
}

class ItemCreation : public TempleFix {
public:
	const char* name() override {
		return "Item Creation UI";
	}
	void apply() override {
		// auto system = UiSystem::getUiSystem("ItemCreation-UI");		
		// system->init = systemInit;
	}
} itemCreation;
