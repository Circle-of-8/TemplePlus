#include "stdafx.h"
#include "common.h"
#include "d20.h"
#include "temple_functions.h"
#include "obj.h"
#include <temple/dll.h>
#include "feat.h"
#include "util/fixes.h"
#include "spell.h"
#include "dispatcher.h"
#include "condition.h"
#include "inventory.h"
#include "python/python_debug.h"
#include "pathfinding.h"
#include "location.h"
#include "action_sequence.h"
#include "critter.h"
#include "anim.h"
#include "tab_file.h"
#include "combat.h"
#include "float_line.h"
#include "weapon.h"
#include "party.h"
#include "ui/ui_dialog.h"
#include "ui/ui_picker.h"
#include "d20_obj_registry.h"
#include "gamesystems/gamesystems.h"
#include "gamesystems/objects/objsystem.h"
#include "damage.h"
#include "history.h"
#include "anim.h"
#include "python/python_spell.h"
#include "python/python_integration_spells.h"
#include "gamesystems/particlesystems.h"
#include <infrastructure/elfhash.h>
#include "python/python_integration_d20_action.h"
#include <turn_based.h>


static_assert(sizeof(D20SpellData) == (8U), "D20SpellData structure has the wrong size!"); //shut up compiler, this is ok
static_assert(sizeof(D20Actn) == 0x58, "D20Action struct has the wrong size!");
static_assert(sizeof(D20ActionDef) == 0x30, "D20ActionDef struct has the wrong size!");


int (__cdecl *OrgD20Init)(GameSystemConf* conf);

class D20Replacements : public TempleFix {
public:
	static int PerformActivateReadiedAction(D20Actn* d20a);

	void apply() override {
		
		OrgD20Init = (int(__cdecl *)(GameSystemConf* conf))replaceFunction(0x1004C8A0, _D20Init);


		replaceFunction(0x1004CA00, _D20StatusInitItemConditions);
		replaceFunction(0x1004CC00, _D20Query);
		replaceFunction(0x1004CC60, _d20QueryWithData);
		replaceFunction(0x1004CD40, _d20QueryReturnData);
		replaceFunction(0x1004DFC0, _GetAttackWeapon);

		replaceFunction(0x1004E6B0, _d20SendSignal);
		
		replaceFunction(0x1004F910, _D20StatusInitFromInternalFields);
		replaceFunction(0x1004FDB0, _D20StatusInit);
		replaceFunction(0x1004FF30, _D20StatusRefresh);
		
		replaceFunction(0x10077850, D20SpellDataExtractInfo);
		replaceFunction(0x10077830, D20SpellDataSetSpontCast);
		replaceFunction(0x10077800, _d20ActnSetSpellData); 
		

		
		replaceFunction(0x10080220, _CanLevelup);
		
		replaceFunction(0x10089F80, _globD20aSetTypeAndData1);
		replaceFunction(0x1008A450, _GlobD20ActnSetSpellData);
		replaceFunction(0x1008A530, _globD20aSetPerformer);

		replaceFunction(0x1008CE30, _PerformStandardAttack);
		
		replaceFunction(0x100920B0, PerformActivateReadiedAction);

		replaceFunction(0x100949E0, _GlobD20ActnInit);
		

		
		replaceFunction(0x10093810, _D20ActnInitUsercallWrapper); // function takes esi as argument
		
		replaceFunction(0x100FD2D0, _D20StatusInitFeats);
		replaceFunction(0x100FD790, _D20StatusInitRace);
		replaceFunction(0x100FEE60, _D20StatusInitClass); 
		
	}
} d20Replacements;

int D20Replacements::PerformActivateReadiedAction(D20Actn* d20a)
{
	logger->info("Performing Readied Interrupt - cutting sequence short.");
	auto curSeq = *actSeqSys.actSeqCur;
	int curIdx = curSeq->d20aCurIdx;
	if (curIdx < curSeq->d20ActArrayNum 
		&& curSeq->d20ActArray[curIdx+1].d20ActType != D20A_READIED_INTERRUPT){
		curSeq->d20ActArrayNum = curIdx;
	}

	//curSeq->seqOccupied &= ~SEQF_PERFORMING;

	return 0;
	
}


class D20ActionCallbacks {
public:
#define ActionCheck(fname) static ActionErrorCode  ActionCheck ## fname ## (D20Actn* d20a, TurnBasedStatus* tbStat)
#define AddToSeq(fname) static ActionErrorCode AddToSeq ## fname ## (D20Actn* d20a, ActnSeq* actSeq, TurnBasedStatus* tbStat);
#define PerformFunc(fname) static ActionErrorCode  Perform ## fname ## (D20Actn* d20a)
#define ActionCost(fname) static ActionErrorCode ActionCost ## fname ## (D20Actn* d20a, TurnBasedStatus* tbStat, ActionCostPacket* acp);
#define ActionFrame(fname) static ActionErrorCode ActionFrame ## fname ## (D20Actn* d20a)
	// Add to sequence funcs
	AddToSeq(Charge);
	AddToSeq(Python);
	AddToSeq(Simple);
	AddToSeq(WithTarget);
	AddToSeq(WhirlwindAttack);
	static ActionErrorCode AddToSeqTripAttack(D20Actn* d20a, ActnSeq* actSeq, TurnBasedStatus* tbStat);;
	
	

	// Turn Based Status checks
	static ActionErrorCode StdAttackTurnBasedStatusCheck(D20Actn* d20a, TurnBasedStatus* tbStat);
	static ActionErrorCode TurnBasedStatusCheckPython(D20Actn* d20a, TurnBasedStatus* tbStat);

	// Action Checks
	ActionCheck(AidAnotherWakeUp);
	ActionCheck(CastSpell);
	ActionCheck(Disarm);
	ActionCheck(DisarmedWeaponRetrieve);
	ActionCheck(DivineMight);
	ActionCheck(EmptyBody);
	ActionCheck(Python);  // calls python script
	ActionCheck(QuiveringPalm);
	ActionCheck(Sneak);
	ActionCheck(Sunder);
	ActionCheck(TripAttack);
	

	// Action Cost
	ActionCost(Python);
	ActionCost(StandardAttack);
	ActionCost(MoveAction);
	ActionCost(Null);
	ActionCost(StandardAction);
	ActionCost(WhirlwindAttack);

	static ActionErrorCode LocationCheckDisarmedWeaponRetrieve(D20Actn* d20a, TurnBasedStatus* tbStat, LocAndOffsets* loc);
	static ActionErrorCode LocationCheckPython(D20Actn* d20a, TurnBasedStatus* tbStat, LocAndOffsets* loc);
	
	// Perform 
	PerformFunc(AidAnotherWakeUp);
	PerformFunc(Aoo);
	PerformFunc(CastItemSpell);
	PerformFunc(CastSpell); // also used in PerformUseItem
	PerformFunc(Charge);
	PerformFunc(Disarm);
	PerformFunc(DisarmedWeaponRetrieve);
	PerformFunc(DivineMight);
	PerformFunc(EmptyBody);
	PerformFunc(Python);
	PerformFunc(QuiveringPalm);
	PerformFunc(Sneak);
	PerformFunc(StandardAttack);
	PerformFunc(TripAttack);
	PerformFunc(UseItem);

	// Action Frame 
	ActionFrame(AidAnotherWakeUp);
	ActionFrame(Aoo);
	ActionFrame(Charge);
	ActionFrame(Disarm);
	ActionFrame(Python);
	ActionFrame(QuiveringPalm);
	ActionFrame(StandardAttack);
	ActionFrame(Sunder);
	ActionFrame(TouchAttack);
	ActionFrame(TripAttack);
	


} d20Callbacks;


static struct LegacyD20SystemAddresses : temple::AddressTable {

	int (__cdecl*  GlobD20ActnSetTarget)(objHndl objHnd, LocAndOffsets * loc);
	ActionErrorCode (__cdecl* LocationCheckStdAttack)(D20Actn*, TurnBasedStatus*, LocAndOffsets*);
	ActionErrorCode (__cdecl*ActionCostStandardAttack)(D20Actn *d20, TurnBasedStatus *tbStat, ActionCostPacket *acp);
	uint32_t(__cdecl*_PickerFuncTooltipToHitChance)(D20Actn * d20a, int flags);
	uint32_t(__cdecl*AddToSeqStdAttack)(D20Actn*, ActnSeq*, TurnBasedStatus*);
	uint32_t(__cdecl*AiCheckStdAttack)(D20Actn*, TurnBasedStatus*);
	uint32_t(__cdecl*ActionCheckStdAttack)(D20Actn*, TurnBasedStatus*);
	int(__cdecl*TargetWithinReachOfLoc)(objHndl obj, objHndl target, LocAndOffsets* loc);
	int * actSeqTargetsIdx;
	objHndl * actSeqTargets; // size 32

	LegacyD20SystemAddresses()
	{
		rebase(GlobD20ActnSetTarget,0x10092E50); 
		rebase(LocationCheckStdAttack, 0x1008C910);
		rebase(ActionCostStandardAttack, 0x100910F0);
		rebase(_PickerFuncTooltipToHitChance, 0x1008EDF0);
		rebase(AddToSeqStdAttack, 0x100955E0);
		rebase(AiCheckStdAttack, 0x1008C4F0);
		rebase(ActionCheckStdAttack, 0x1008C910);

		rebase(TargetWithinReachOfLoc, 0x100B86C0);
		rebase(actSeqTargetsIdx, 0x118CD2A0);
		rebase(actSeqTargets, 0x118CD2A8);
	}
} addresses;

#pragma region LegacyD20System Implementation
LegacyD20System d20Sys;
D20ActionDef d20ActionDefsNew[1000];
TabFileStatus _d20actionTabFile;

bool LegacyD20System::SpellIsInterruptedCheck(D20Actn* d20a, int invIdx, SpellStoreData* spellData){

	if (spellSys.IsSpellLike(spellData->spellEnum)
		//|| (invIdx != INV_IDX_INVALID) // removed to support miscasting when Casting Defensively with Scrolls
		|| d20QueryWithData(d20a->d20APerformer,
			DK_QUE_Critter_Is_Spell_An_Ability, spellData->spellEnum, 0))
		return false;
	
	if (invIdx != INV_IDX_INVALID) {
		if (d20a->d20ActType != D20A_USE_ITEM)
			return false;
		auto item = inventory.GetItemAtInvIdx(d20a->d20APerformer, invIdx);
		if (!item)
			return false;
		auto itemObj = gameSystems->GetObj().GetObject(item);
		if (itemObj->type != obj_t_scroll)
			return false;
	}

	if (d20a->d20Caf & D20CAF_COUNTERSPELLED)
		return true;
	return d20Sys.d20QueryWithData(d20a->d20APerformer, 
		DK_QUE_SpellInterrupted, (uint32_t)&d20a->d20SpellData, 0) != 0;
}

int LegacyD20System::CastSpellProcessTargets(D20Actn* d20a, SpellPacketBody& spellPkt){

	std::vector<objHndl> targets;

	for (auto i = 0u; i < spellPkt.targetCount; i++) {
		auto &tgt = spellPkt.targetListHandles[i];
		if (!tgt){
			logger->warn("CastSpellProcessTargets: Null target! Idx {}",i);
			continue;
		}		
		auto tgtObj = gameSystems->GetObj().GetObject(tgt);
		
		
		if (!tgtObj->IsCritter()){
			targets.push_back(tgt);
			continue;
		}

		// check target spell immunity
		DispIoImmunity dispIoImmunity;
		dispIoImmunity.flag = 1;
		dispIoImmunity.spellPkt = &spellPkt;
		if (dispatch.Dispatch64ImmunityCheck(tgt, &dispIoImmunity))
			continue;
		
		// check spell resistance for hostiles
		if (critterSys.IsFriendly(d20a->d20APerformer, tgt)){
			targets.push_back(tgt);
			continue;
		}

		SpellEntry spEntry(spellPkt.spellEnum);
		if (spEntry.spellResistanceCode != 1){
			targets.push_back(tgt);
			continue;
		}

		DispIOBonusListAndSpellEntry dispIoSr;
		BonusList casterLvlBonlist;
		auto casterLvl = spellPkt.casterLevel; //dispatch.Dispatch35CasterLevelModify(d20a->d20APerformer, &spellPkt);  the caster level has already been modified at this point!
		casterLvlBonlist.AddBonus(casterLvl, 0, 203);
		if (feats.HasFeatCountByClass(d20a->d20APerformer, FEAT_SPELL_PENETRATION))	{
			casterLvlBonlist.AddBonusFromFeat(2, 0, 114, FEAT_SPELL_PENETRATION);
		}
		if (feats.HasFeatCountByClass(d20a->d20APerformer, FEAT_GREATER_SPELL_PENETRATION)) {
			casterLvlBonlist.AddBonusFromFeat(2, 0, 114, FEAT_GREATER_SPELL_PENETRATION);
		}
		dispIoSr.spellEntry = &spEntry;
		auto dispelDc = dispatch.Dispatch45SpellResistanceMod(tgt, &dispIoSr);
		if (dispelDc <=0){
			targets.push_back(tgt);
			continue;
		}
		auto rollHistId = 0;
		if (spellSys.DispelRoll(d20a->d20APerformer, &casterLvlBonlist, 0, dispelDc, combatSys.GetCombatMesLine(5048), &rollHistId) < 0){
			logger->info("CastSpellProcessTargets: spell {} cast by {} resisted by target {}", spellPkt.GetName(), d20a->d20APerformer, tgt );
			floatSys.FloatSpellLine(tgt, 30008, FloatLineColor::White);
			gameSystems->GetParticleSys().CreateAtObj("Fizzle", tgt);

			auto text = fmt::format("{}{}{}\n\n", combatSys.GetCombatMesLine(119), rollHistId, combatSys.GetCombatMesLine(120)); // Spell ~fails~ to overcome Spell Resistance
			histSys.CreateFromFreeText(text.c_str());

			spellSys.UpdateSpellPacket(spellPkt);
			pySpellIntegration.UpdateSpell(spellPkt.spellId);
			continue;
		}
		auto text = fmt::format("{}{}{}\n\n", combatSys.GetCombatMesLine(121), rollHistId, combatSys.GetCombatMesLine(122)); // Spell ~overcomes~ Spell Resistance
		histSys.CreateFromFreeText(text.c_str());

		floatSys.FloatSpellLine(tgt, 30009, FloatLineColor::Red);
		targets.push_back(tgt);

	}
	if (targets.size() > 0){
		memcpy(spellPkt.targetListHandles, &targets[0], min(sizeof spellPkt.targetListHandles, targets.size() * sizeof(objHndl)));
	}
	
	for (int i = targets.size(); i < 32; i++){
		spellPkt.targetListHandles[i] = 0;
	}
	spellPkt.targetCount = min(32u, targets.size());

	spellSys.UpdateSpellPacket(spellPkt);
	pySpellIntegration.UpdateSpell(spellPkt.spellId);
	return spellPkt.targetCount;
}

void LegacyD20System::NewD20ActionsInit()
{
	tabSys.tabFileStatusInit(d20ActionsTabFile, d20actionTabLineParser);
	if (tabSys.tabFileStatusBasicFormatter(d20ActionsTabFile, "tprules//d20actions.tab"))
	{
		tabSys.tabFileStatusDealloc(d20ActionsTabFile);
	}
	else
	{
		tabSys.tabFileParseLines(d20ActionsTabFile);
	}
	mesFuncs.Open("tpmes//combat.mes", &combatSys.combatMesNew);

	d20Defs[D20A_PYTHON_ACTION].actionCost = d20Callbacks.ActionCostPython;
	d20Defs[D20A_PYTHON_ACTION].actionCheckFunc = d20Callbacks.ActionCheckPython;
	d20Defs[D20A_PYTHON_ACTION].addToSeqFunc = d20Callbacks.AddToSeqPython;
	d20Defs[D20A_PYTHON_ACTION].performFunc = d20Callbacks.PerformPython;
	d20Defs[D20A_PYTHON_ACTION].actionFrameFunc = d20Callbacks.ActionFramePython;
	d20Defs[D20A_PYTHON_ACTION].turnBasedStatusCheck = d20Callbacks.TurnBasedStatusCheckPython;
	d20Defs[D20A_PYTHON_ACTION].locCheckFunc = d20Callbacks.LocationCheckPython;
	d20Defs[D20A_PYTHON_ACTION].projectilePerformFunc = nullptr;// d20Callbacks.ProjectilePerformFunc;
	d20Defs[D20A_PYTHON_ACTION].flags = D20ADF::D20ADF_Python;

	d20Defs[D20A_DIVINE_MIGHT].addToSeqFunc = d20Callbacks.AddToSeqSimple;
	d20Defs[D20A_DIVINE_MIGHT].actionCheckFunc = d20Callbacks.ActionCheckDivineMight;
	d20Defs[D20A_DIVINE_MIGHT].performFunc = d20Callbacks.PerformDivineMight;
	// d20Defs[D20A_DIVINE_MIGHT].actionFrameFunc = _DivineMightPerform;
	d20Defs[D20A_DIVINE_MIGHT].actionCost = d20Callbacks.ActionCostNull;
	d20Defs[D20A_DIVINE_MIGHT].flags = D20ADF::D20ADF_None;
	
	D20ActionType d20Type;

	
	d20Type = D20A_STANDARD_ATTACK;
	d20Defs[d20Type].performFunc = d20Callbacks.PerformStandardAttack;
	d20Defs[d20Type].actionFrameFunc = d20Callbacks.ActionFrameStandardAttack;

	d20Type = D20A_CAST_SPELL;
	d20Defs[d20Type].performFunc = d20Callbacks.PerformCastSpell;
	d20Defs[d20Type].actionCheckFunc = d20Callbacks.ActionCheckCastSpell;

	d20Type = D20A_USE_ITEM;
	d20Defs[d20Type].performFunc = d20Callbacks.PerformUseItem;
	d20Defs[d20Type].flags = D20ADF(D20ADF_QueryForAoO | D20ADF_MagicEffectTargeting);

	d20Type = D20A_USE_POTION;
	d20Defs[d20Type].performFunc = d20Callbacks.PerformUseItem;

	d20Type = D20A_ACTIVATE_DEVICE_SPELL;
	d20Defs[d20Type].performFunc = d20Callbacks.PerformUseItem;


	d20Type = D20A_TRIP;
	d20Defs[d20Type].addToSeqFunc = d20Callbacks.AddToSeqTripAttack;
	d20Defs[d20Type].actionCheckFunc = d20Callbacks.ActionCheckTripAttack;
	d20Defs[d20Type].turnBasedStatusCheck = d20Callbacks.StdAttackTurnBasedStatusCheck;
	d20Defs[d20Type].actionFrameFunc = d20Callbacks.ActionFrameTripAttack;


	d20Type = D20A_ATTACK_OF_OPPORTUNITY;
	d20Defs[d20Type].performFunc = d20Callbacks.PerformAoo;
	d20Defs[d20Type].actionFrameFunc = d20Callbacks.ActionFrameAoo;


	d20Type = D20A_CHARGE;
	d20Defs[d20Type].performFunc = d20Callbacks.PerformCharge;
	d20Defs[d20Type].actionFrameFunc = d20Callbacks.ActionFrameCharge;


	d20Type = D20A_TOUCH_ATTACK;
	d20Defs[d20Type].actionFrameFunc = d20Callbacks.ActionFrameTouchAttack;

	d20Type = D20A_DISARM;
	d20Defs[d20Type].addToSeqFunc = d20Callbacks.AddToSeqWithTarget;
	d20Defs[d20Type].turnBasedStatusCheck = d20Callbacks.StdAttackTurnBasedStatusCheck;
	d20Defs[d20Type].actionCheckFunc = d20Callbacks.ActionCheckDisarm;
	d20Defs[d20Type].locCheckFunc = addresses.LocationCheckStdAttack;
	d20Defs[d20Type].performFunc = d20Callbacks.PerformDisarm;
	d20Defs[d20Type].actionFrameFunc = d20Callbacks.ActionFrameDisarm;
	d20Defs[d20Type].actionCost = d20Callbacks.ActionCostStandardAttack;
	d20Defs[d20Type].seqRenderFunc = addresses._PickerFuncTooltipToHitChance;
	d20Defs[d20Type].flags = (D20ADF)(D20ADF_TargetSingleExcSelf | D20ADF_TriggersAoO | D20ADF_QueryForAoO | D20ADF_TriggersCombat	| D20ADF_UseCursorForPicking | D20ADF_SimulsCompatible ); // 0x28908; // same as Trip // note : queryForAoO is used for resetting a flag


	d20Type = D20A_DISARMED_WEAPON_RETRIEVE;
	d20Defs[d20Type].addToSeqFunc = d20Callbacks.AddToSeqSimple;
	d20Defs[d20Type].turnBasedStatusCheck = nullptr;
	d20Defs[d20Type].actionCheckFunc = d20Callbacks.ActionCheckDisarmedWeaponRetrieve;
	d20Defs[d20Type].locCheckFunc = d20Callbacks.LocationCheckDisarmedWeaponRetrieve;
	d20Defs[d20Type].performFunc = d20Callbacks.PerformDisarmedWeaponRetrieve;
	d20Defs[d20Type].actionFrameFunc = nullptr;
	d20Defs[d20Type].actionCost = d20Callbacks.ActionCostMoveAction;
	d20Defs[d20Type].seqRenderFunc = nullptr;
	d20Defs[d20Type].flags = (D20ADF)( D20ADF_TriggersAoO 	| 0*D20ADF_UseCursorForPicking
		| D20ADF_SimulsCompatible | D20ADF_Breaks_Concentration); // 0x28908; // largely same as Pick Up Object
	*(int*)&d20Defs[D20A_PICKUP_OBJECT].flags |= (int) (D20ADF_TriggersAoO);

	d20Type = D20A_SUNDER;
	d20Defs[d20Type].addToSeqFunc = d20Callbacks.AddToSeqWithTarget;
	d20Defs[d20Type].turnBasedStatusCheck = d20Callbacks.StdAttackTurnBasedStatusCheck;
	d20Defs[d20Type].actionCheckFunc = d20Callbacks.ActionCheckSunder;
	d20Defs[d20Type].locCheckFunc = addresses.LocationCheckStdAttack;
	d20Defs[d20Type].performFunc = d20Callbacks.PerformDisarm;
	d20Defs[d20Type].actionFrameFunc = d20Callbacks.ActionFrameSunder;
	d20Defs[d20Type].actionCost = d20Callbacks.ActionCostStandardAttack;
	d20Defs[d20Type].seqRenderFunc = addresses._PickerFuncTooltipToHitChance;
	d20Defs[d20Type].flags = (D20ADF)(D20ADF_TargetSingleExcSelf | D20ADF_TriggersAoO | D20ADF_TriggersCombat| D20ADF_UseCursorForPicking | D20ADF_SimulsCompatible); // 0x28908; // same as Trip

	d20Type = D20A_AID_ANOTHER_WAKE_UP;
	d20Defs[d20Type].addToSeqFunc = d20Callbacks.AddToSeqWithTarget;
	d20Defs[d20Type].turnBasedStatusCheck = d20Callbacks.StdAttackTurnBasedStatusCheck;
	d20Defs[d20Type].actionCheckFunc = d20Callbacks.ActionCheckAidAnotherWakeUp;
	d20Defs[d20Type].locCheckFunc = addresses.LocationCheckStdAttack;
	d20Defs[d20Type].performFunc = d20Callbacks.PerformAidAnotherWakeUp;
	d20Defs[d20Type].actionFrameFunc = d20Callbacks.ActionFrameAidAnotherWakeUp;
	d20Defs[d20Type].actionCost = d20Callbacks.ActionCostStandardAttack;
	d20Defs[d20Type].seqRenderFunc = addresses._PickerFuncTooltipToHitChance;
	d20Defs[d20Type].flags = (D20ADF)(D20ADF_TargetSingleExcSelf | D20ADF_UseCursorForPicking | D20ADF_SimulsCompatible); // 0x28908; // same as Trip // note : queryForAoO is used for resetting a flag


	d20Type = D20A_EMPTY_BODY;
	d20Defs[d20Type].addToSeqFunc = d20Callbacks.AddToSeqSimple;
	d20Defs[d20Type].turnBasedStatusCheck = nullptr;
	d20Defs[d20Type].actionCheckFunc = d20Callbacks.ActionCheckEmptyBody;
	d20Defs[d20Type].locCheckFunc = nullptr;
	d20Defs[d20Type].performFunc = d20Callbacks.PerformEmptyBody;
	d20Defs[d20Type].actionFrameFunc = nullptr;
	d20Defs[d20Type].actionCost = d20Callbacks.ActionCostStandardAction;
	d20Defs[d20Type].seqRenderFunc = nullptr;
	d20Defs[d20Type].flags = (D20ADF)( D20ADF_None);


	d20Type = D20A_QUIVERING_PALM;
	d20Defs[d20Type].addToSeqFunc = d20Callbacks.AddToSeqWithTarget;
	d20Defs[d20Type].turnBasedStatusCheck = d20Callbacks.StdAttackTurnBasedStatusCheck;
	d20Defs[d20Type].actionCheckFunc = d20Callbacks.ActionCheckQuiveringPalm;
	d20Defs[d20Type].locCheckFunc = addresses.LocationCheckStdAttack;
	d20Defs[d20Type].performFunc = d20Callbacks.PerformQuiveringPalm;
	d20Defs[d20Type].actionFrameFunc = d20Callbacks.ActionFrameQuiveringPalm;
	d20Defs[d20Type].actionCost = d20Callbacks.ActionCostStandardAttack;
	d20Defs[d20Type].seqRenderFunc = addresses._PickerFuncTooltipToHitChance;
	d20Defs[d20Type].flags = (D20ADF)(D20ADF_TargetSingleExcSelf | D20ADF_TriggersCombat | D20ADF_UseCursorForPicking);


	d20Type = D20A_SNEAK;
	d20Defs[d20Type].actionCheckFunc = d20Callbacks.ActionCheckSneak;
	d20Defs[d20Type].locCheckFunc = nullptr;
	d20Defs[d20Type].actionCost = d20Callbacks.ActionCostMoveAction;
	d20Defs[d20Type].performFunc = d20Callbacks.PerformSneak;


	d20Type = D20A_WHIRLWIND_ATTACK;
	d20Defs[d20Type].addToSeqFunc = d20Callbacks.AddToSeqWhirlwindAttack;
	d20Defs[d20Type].actionCost = d20Callbacks.ActionCostWhirlwindAttack;


	// *(int*)&d20Defs[D20A_USE_POTION].flags |= (int)D20ADF_SimulsCompatible;  // need to modify the SimulsEnqueue script because it also checks for san_start_combat being null
	// *(int*)&d20Defs[D20A_TRIP].flags -= (int)D20ADF_Unk8000;

	//d20Defs[D20A_DISARM] = d20Defs[D20A_STANDARD_ATTACK];
	//d20Defs[d20Type].actionCost = _ActionCostNull; // just for testing - REMOVE!!!
}

void LegacyD20System::GetPythonActionSpecs(){
	std::vector<int> actEnums;
	pythonD20ActionIntegration.GetActionEnums(actEnums);
	std::sort(actEnums.begin(), actEnums.end());

	for (auto it: actEnums){
		
		PythonActionSpec &pyActSpec = pyactions[it];

		pyActSpec.flags = (D20ADF) pythonD20ActionIntegration.GetActionDefinitionFlags(it);
		pyActSpec.tgtClass = (D20TargetClassification)pythonD20ActionIntegration.GetTargetingClassification(it);
		pyActSpec.name = pythonD20ActionIntegration.GetActionName(it);
		pyActSpec.costType = pythonD20ActionIntegration.GetActionCostType(it);

	}
}

std::string & LegacyD20System::GetPythonActionName(D20DispatcherKey key) const
{
	return d20Sys.pyactions[key].name;
}

ActionErrorCode LegacyD20System::GetPyActionCost(D20Actn * d20a, TurnBasedStatus * tbStat, ActionCostPacket * acp){
	
	switch (pyactions[d20a->data1].costType){
	
	case ActionCostType::Null:
		return d20Callbacks.ActionCostNull(d20a, tbStat, acp);
	default:
		return d20Callbacks.ActionCostNull(d20a, tbStat, acp);
	}
}

LegacyD20System::LegacyD20System()
{
	pathfinding = &pathfindingSys;
	actSeq = &actSeqSys;
	//d20Class = &d20ClassSys;
	d20Status = &d20StatusSys;
	rebase(D20StatusInitFromInternalFields, 0x1004F910);
	rebase(D20ObjRegistryAppend, 0x100DFAD0);
	rebase(d20EditorMode, 0x10AA3284);
	rebase(globD20Action, 0x1186AC00);
	rebase(ToHitProc, 0x100B7160);
	// rebase(d20Defs, 0x102CC5C8);
	d20Defs = (D20ActionDef*)&d20ActionDefsNew;
		d20ActionsTabFile = &_d20actionTabFile;
		d20actionTabLineParser = &_d20actionTabLineParser;
	//rebase(ToEEd20ActionNames, 0x102CD2BC);
	rebase(_d20aTriggerCombatCheck, 0x1008AE90);//ActnSeq * @<eax>
	rebase(_tumbleCheck, 0x1008AA90);
	rebase(_d20aTriggersAOO, 0x1008A9C0);
	rebase(CreateRollHistory, 0x100DFFF0);
	

}


#pragma region D20 Signal and D20 Query

uint32_t LegacyD20System::d20Query(objHndl objHnd, D20DispatcherKey dispKey)
{
	Dispatcher * dispatcher = objects.GetDispatcher(objHnd);
	if (dispatcher == nullptr || (int32_t)dispatcher == -1){ return 0; }
	DispIoD20Query dispIO;
	dispIO.dispIOType = dispIOTypeQuery;
	dispIO.return_val = 0;
	dispIO.data1 = 0;
	dispIO.data2 = 0;

	objects.dispatch.DispatcherProcessor(dispatcher, dispTypeD20Query, dispKey, &dispIO);
	if (dispKey == DK_QUE_Critter_Is_Charmed || dispKey == DK_QUE_Critter_Is_Afraid || dispKey == DK_QUE_Critter_Is_Held) {
		// in these cases the information stored is an objhandle; make sure it's a valid handle!
		if (gameSystems->GetObj().IsValidHandle(*reinterpret_cast<objHndl*>(&dispIO.data1))) {
			dispIO.return_val;
		}
		else {
			logger->debug("D20Query: an invalid handle was found, overriding result to 0!");
			dispIO.return_val = 0;
			return 0i64;
		}
	}
	return dispIO.return_val;
}

uint32_t LegacyD20System::d20QueryWithData(objHndl objHnd, D20DispatcherKey dispKey, uint32_t arg1, uint32_t arg2)
{
	Dispatcher * dispatcher = objects.GetDispatcher(objHnd);
	if (dispatcher == nullptr || (int32_t)dispatcher == -1){ return 0; }
	DispIoD20Query dispIO;
	dispIO.dispIOType = dispIOTypeQuery;
	dispIO.return_val = 0;
	dispIO.data1 = arg1;
	dispIO.data2 = arg2;
	objects.dispatch.DispatcherProcessor(dispatcher, dispTypeD20Query, dispKey, &dispIO);
	return dispIO.return_val;
}

uint32_t LegacyD20System::d20QueryWithData(objHndl obj, D20DispatcherKey dispKey, objHndl argObj)
{
	return d20QueryWithData(obj, dispKey, argObj.GetHandleLower(), argObj.GetHandleUpper());
}

uint32_t LegacyD20System::d20QueryWithData(objHndl obj, D20DispatcherKey dispKey, CondStruct* cond, uint32_t arg2)
{
	return d20QueryWithData(obj, dispKey, reinterpret_cast<uint32_t>(cond), arg2);
}

uint32_t LegacyD20System::d20QueryWithData(objHndl obj, D20DispatcherKey dispKey, D20SpellData* spellData, uint32_t arg2)
{
	return d20QueryWithData(obj, dispKey, reinterpret_cast<uint32_t>(spellData), arg2);
}

uint32_t LegacyD20System::d20QueryHasSpellCond(objHndl obj, int spellEnum)
{
	auto cond = spellSys.GetCondFromSpellIdx(spellEnum);
	if (!cond)
		return 0;
	return d20QueryWithData(obj, DK_QUE_Critter_Has_Condition, (uint32_t) cond, 0);
}

void LegacyD20System::d20SendSignal(objHndl objHnd, D20DispatcherKey dispKey, int32_t arg1, int32_t arg2)
{
	if (!objHnd){
		logger->warn("D20SendSignal called with null handle! Key was {}, arg1 {}, arg2 {}", (int)dispKey, arg1, arg2);
		return;
	}
	DispIoD20Signal dispIO;
	Dispatcher * dispatcher = objects.GetDispatcher(objHnd);
	if (!dispatch.dispatcherValid(dispatcher))
	{
		logger->info("d20SendSignal(): Object {} ({}) lacks a Dispatcher", description._getDisplayName(objHnd, objHnd), objHnd);
		return;
	}
	dispIO.dispIOType = dispIoTypeSendSignal;
	dispIO.data1 = arg1;
	dispIO.data2 = arg2;
	dispatch.DispatcherProcessor(dispatcher, dispTypeD20Signal, dispKey, &dispIO);
}

void LegacyD20System::d20SendSignal(objHndl objHnd, D20DispatcherKey dispKey, D20Actn* arg1, int32_t arg2)
{
	if (!objHnd) {
		logger->warn("D20SendSignal called with null handle! Key was {}", (int)dispKey);
		return;
	}
	DispIoD20Signal dispIO;
	Dispatcher * dispatcher = objects.GetDispatcher(objHnd);
	if (!dispatch.dispatcherValid(dispatcher))
	{
		logger->info("d20SendSignal(): Object {} ({}) lacks a Dispatcher", description._getDisplayName(objHnd, objHnd), objHnd);
		return;
	}
	dispIO.dispIOType = dispIoTypeSendSignal;
	dispIO.data1 = (int)arg1;
	dispIO.data2 = arg2;
	dispatch.DispatcherProcessor(dispatcher, dispTypeD20Signal, dispKey, &dispIO);
}

void LegacyD20System::d20SendSignal(objHndl objHnd, D20DispatcherKey dispKey, objHndl arg) {
	if (!objHnd) {
		logger->warn("D20SendSignal called with null handle! Key was {}", (int)dispKey);
		return;
	}
	DispIoD20Signal dispIO;
	Dispatcher * dispatcher = objects.GetDispatcher(objHnd);
	if (!dispatch.dispatcherValid(dispatcher))
	{
		logger->info("d20SendSignal(): Object {} ({}) lacks a Dispatcher", description._getDisplayName(objHnd, objHnd), objHnd);
		return;
	}
	dispIO.dispIOType = dispIoTypeSendSignal;
	*(objHndl*)&dispIO.data1 = arg;
	dispatch.DispatcherProcessor(dispatcher, dispTypeD20Signal, dispKey, &dispIO);
}

void LegacyD20System::D20SignalPython(const objHndl& handle, const std::string& queryKey, int arg1, int arg2){

	if (!handle) {
		logger->warn("D20SignalPython called with null handle! Key was {}, arg1 {}, arg2 {}", queryKey, arg1, arg2);
		return;
	}

	Dispatcher * dispatcher = objects.GetDispatcher(handle);
	if (!dispatch.dispatcherValid(dispatcher)) { return ; }

	DispIoD20Query dispIo;
	dispIo.dispIOType = dispIoTypeSendSignal;
	dispIo.return_val = 0;
	dispIo.data1 = arg1;
	dispIo.data2 = arg2;
	dispatcher->Process(enum_disp_type::dispTypePythonSignal, static_cast<D20DispatcherKey>(ElfHash::Hash(queryKey)), &dispIo);
	return;

}
void LegacyD20System::D20SignalPython(const objHndl & handle, int queryKey, int arg1, int arg2){
	if (!handle) {
		logger->warn("D20SignalPython called with null handle! Key was {}, arg1 {}, arg2 {}", queryKey, arg1, arg2);
		return;
	}

	Dispatcher * dispatcher = objects.GetDispatcher(handle);
	if (!dispatch.dispatcherValid(dispatcher)) { return; }

	DispIoD20Query dispIo;
	dispIo.dispIOType = dispIoTypeSendSignal;
	dispIo.return_val = 0;
	dispIo.data1 = arg1;
	dispIo.data2 = arg2;
	dispatcher->Process(enum_disp_type::dispTypePythonSignal, static_cast<D20DispatcherKey>(queryKey), &dispIo);
	return;
}
#pragma endregion

void LegacyD20System::D20ActnInit(objHndl objHnd, D20Actn* d20a)
{
	d20a->d20APerformer = objHnd;
	d20a->d20ActType = D20A_NONE;
	d20a->data1=0 ;
	d20a->d20ATarget=0i64;
	objects.loc->getLocAndOff(objHnd, &d20a->destLoc);
	PathQueryResult * pq = d20a->path;
	d20a->distTraversed = 0;
	d20a->radialMenuActualArg = 0;
	d20a->spellId = 0;
	d20a->d20Caf = 0;

	if (pq && pq >= pathfinding->pathQArray && pq < (pathfinding->pathQArray + PQR_CACHE_SIZE))
	{
		pq->occupiedFlag = 0;
	}
	d20a->path = nullptr;
	d20a->d20SpellData.spellEnumOrg = 0;
	d20a->animID = 0;
	//d20a->animID = -1;  // was 0 in vanilla, probably bug?
	d20a->rollHist1 = -1;
	d20a->rollHist2 = -1;
	d20a->rollHist3 = -1;
	
}

#pragma region Global D20 Action
void LegacyD20System::GlobD20ActnSetTypeAndData1(D20ActionType d20type, uint32_t data1){
	globD20Action->d20ActType = d20type;
	globD20Action->data1 = data1;
}

void LegacyD20System::globD20ActnSetPerformer(objHndl objHnd)
{
	if (objHnd != (*globD20Action).d20APerformer)
	{
		*actSeqSys.seqPickerTargetingType = D20TC_Invalid;
		*actSeqSys.seqPickerD20ActnType = D20A_UNSPECIFIED_ATTACK;
		*actSeqSys.seqPickerD20ActnData1 = 0;
	}
	(*globD20Action).d20APerformer = objHnd;
}

int LegacyD20System::GlobD20ActnSetTarget(objHndl objHnd, LocAndOffsets * loc)
{
	return addresses.GlobD20ActnSetTarget(objHnd, loc);
}

void LegacyD20System::GlobD20ActnInit()
{
	D20ActnInit(globD20Action->d20APerformer, globD20Action);
}

void LegacyD20System::GlobD20ActnSetSpellData(D20SpellData* d20SpellData)
{
	d20Sys.globD20Action->d20SpellData = *d20SpellData;
}
#pragma endregion

void LegacyD20System::d20aTriggerCombatCheck(ActnSeq* actSeq, int32_t idx)
{
	__asm{
		push esi;
		push ecx;
		mov ecx, this;
		mov esi, idx;
		push esi;
		mov esi, [ecx]._d20aTriggerCombatCheck;
		mov eax, actSeq;
		call esi;
		add esp, 4;

		pop ecx;
		pop esi;
	}
	//void d20aTriggerCombatCheck(ActnSeq* actSeq, int32_t idx);//1008AE90    ActnSeq * @<eax>
}

int32_t LegacyD20System::D20ActionTriggersAoO(D20Actn* d20a, TurnBasedStatus* tbStat)
{
	//uint32_t result = 0;
	ActnSeq * actSeq = *actSeqSys.actSeqCur;
	if (actSeq->tbStatus.tbsFlags & TBSF_CritterSpell)
		return 0;



	if ( (d20Defs[d20a->d20ActType].flags & D20ADF::D20ADF_QueryForAoO)
		&& d20QueryWithData(d20a->d20APerformer, DK_QUE_ActionTriggersAOO, (int)d20a, 0))
	{
		if (d20a->d20ActType == D20A_DISARM)
			return feats.HasFeatCountByClass(d20a->d20APerformer, FEAT_IMPROVED_DISARM) == 0;
		return 1;
	}
		
	
	if (!(d20Defs[d20a->d20ActType].flags & D20ADF::D20ADF_TriggersAoO))
		return 0;

	if (d20a->d20ActType == D20A_TRIP){
		auto weaponUsed = d20Sys.GetAttackWeapon(d20a->d20APerformer, d20a->data1, (D20CAF)d20a->d20Caf);
		if (inventory.IsTripWeapon(weaponUsed))
			return FALSE;
		return feats.HasFeatCountByClass(d20a->d20APerformer, FEAT_IMPROVED_TRIP) == 0;
	}
		


	if (d20a->d20ActType == D20A_SUNDER)
		return feats.HasFeatCountByClass(d20a->d20APerformer, FEAT_IMPROVED_SUNDER) == 0;


	if (d20a->d20Caf & D20CAF_TOUCH_ATTACK
		|| d20Sys.GetAttackWeapon(d20a->d20APerformer, d20a->data1, (D20CAF)d20a->d20Caf) 
		|| dispatch.DispatchD20ActionCheck(d20a, tbStat, dispTypeGetCritterNaturalAttacksNum))
		return 0;

	return feats.HasFeatCountByClass(d20a->d20APerformer, FEAT_IMPROVED_UNARMED_STRIKE) == 0;

	/*
	__asm{
		push esi;
		push ecx;
		push ebx;
		mov ecx, this;
		mov esi, iO;
		push esi;
		mov ebx, [ecx]._d20aTriggersAOO;
		mov esi, d20a;
		call ebx;
		add esp, 4;
		mov result, eax;
		pop ebx;
		pop ecx;
		pop esi;
	}
	return result; //_d20aTriggersAOO(void * iO); // d20a @<esi> // 1008A9C0
	*/
}

uint32_t LegacyD20System::tumbleCheck(D20Actn* d20a)
{
	if (d20QueryWithData(d20a->d20ATarget, DK_QUE_Critter_Has_Spell_Active, 407, 0)) return 0; // spell_sanctuary active
	if (actSeq->isPerforming(d20a->d20APerformer))
	{
		logger->info("movement aoo while performing...\n");
		return 0;
	}
	if (!d20QueryWithData(d20a->d20ATarget, DK_QUE_AOOIncurs, d20a->d20APerformer.GetHandleLower(), d20a->d20APerformer.GetHandleUpper())) {
		return 0; 
	}
	if (!d20QueryWithData(d20a->d20APerformer, DK_QUE_AOOPossible, d20a->d20ATarget.GetHandleLower(), d20a->d20ATarget.GetHandleUpper())) {
		return 0;
	}
	if (!d20QueryWithData(d20a->d20APerformer, DK_QUE_AOOWillTake, d20a->d20ATarget.GetHandleLower(), d20a->d20ATarget.GetHandleUpper())) {
		return 0;
	}
	// not fully implemented yet, but that should cover 90% of the cases anyway ;) TODO: complete this function
	return _tumbleCheck(d20a);
}

void LegacyD20System::D20ActnSetSpellData(D20SpellData* d20SpellData, uint32_t spellEnumOrg, uint32_t spellClassCode, uint32_t spellSlotLevel, uint32_t itemSpellData, uint32_t metaMagicData)
{
	*(uint32_t *)&d20SpellData->metaMagicData = metaMagicData;
	d20SpellData->spellEnumOrg = spellEnumOrg;
	d20SpellData->spellClassCode = spellClassCode;
	d20SpellData->itemSpellData = itemSpellData;
	d20SpellData->spontCastType = (SpontCastType)0;
	d20SpellData->spellSlotLevel = spellSlotLevel;
}

void LegacyD20System::ExtractSpellInfo(D20SpellData* d20spellData, uint32_t* spellEnum, uint32_t* spellEnumOrg, uint32_t* spellClassCode, uint32_t* spellSlotLevel, uint32_t* itemSpellData, MetaMagicData* metaMagicData)
{
	D20SpellDataExtractInfo(d20spellData, spellEnum, spellEnumOrg, spellClassCode, spellSlotLevel, itemSpellData, (uint32_t*)metaMagicData);
}


bool LegacyD20System::UsingSecondaryWeapon(D20Actn* d20a)
{
	return UsingSecondaryWeapon(d20a->d20APerformer, d20a->data1);
}

bool LegacyD20System::UsingSecondaryWeapon(objHndl obj, int attackCode)
{
	if (attackCode == ATTACK_CODE_OFFHAND + 2 || attackCode == ATTACK_CODE_OFFHAND + 4 || attackCode == ATTACK_CODE_OFFHAND + 6)
	{
		if (attackCode == ATTACK_CODE_OFFHAND + 2)
		{
			return 1;
		}
		if (attackCode == ATTACK_CODE_OFFHAND + 4)
		{
			if (feats.HasFeatCount(obj, FEAT_IMPROVED_TWO_WEAPON_FIGHTING)
				|| feats.HasFeatCountByClass(obj, FEAT_IMPROVED_TWO_WEAPON_FIGHTING_RANGER, (Stat)0,0))
				return 1;
		}
		else if (attackCode == ATTACK_CODE_OFFHAND + 6)
		{
			if (feats.HasFeatCount(obj, FEAT_GREATER_TWO_WEAPON_FIGHTING)
				|| feats.HasFeatCountByClass(obj, FEAT_GREATER_TWO_WEAPON_FIGHTING_RANGER, (Stat)0, 0))
				return 1;
		}
	}
	return 0;
}

void LegacyD20System::ExtractAttackNumber(objHndl obj, int attackCode, int* attackNumber, int * dualWielding)
{
	if (attackCode >= ATTACK_CODE_NATURAL_ATTACK)
	{
		*attackNumber = attackCode - ATTACK_CODE_NATURAL_ATTACK;
		*dualWielding = 0 ;
	}
	else if (attackCode >= ATTACK_CODE_OFFHAND)
	{
		*dualWielding = 1;
		int attackIdx = attackCode - (ATTACK_CODE_OFFHAND+1);
		int numOffhandExtraAttacks = critterSys.NumOffhandExtraAttacks(obj);
		if (d20Sys.UsingSecondaryWeapon(obj, attackCode))
		{
			if (attackIdx % 2 && (attackIdx - 1) / 2 < numOffhandExtraAttacks )
				*attackNumber = 1 + (attackIdx - 1) / 2;
		}
		else
		{
			if ( !(attackIdx % 2 ) && (attackIdx  / 2 < numOffhandExtraAttacks) )
				*attackNumber = 1 + attackIdx  / 2;
			else
				*attackNumber = 1 + numOffhandExtraAttacks + (attackIdx - 2*numOffhandExtraAttacks);
		}
		assert(*attackNumber > 0);
	}
	else // regular case (just primary hand)
	{
		*attackNumber = attackCode - ATTACK_CODE_PRIMARY;
		if (*attackNumber <= 0) // seems to be the case for charge attack
		{
			*attackNumber = 1;
		}
		*dualWielding = 0;
	}
}

objHndl LegacyD20System::GetAttackWeapon(objHndl obj, int attackCode, D20CAF flags)
{
	if (flags & D20CAF_TOUCH_ATTACK && !(flags & D20CAF_THROWN_GRENADE))
	{
		return objHndl::null;
	}

	if (flags & D20CAF_SECONDARY_WEAPON)
		return inventory.ItemWornAt(obj, EquipSlot::WeaponSecondary);

	if (UsingSecondaryWeapon(obj, attackCode))
		return inventory.ItemWornAt(obj, EquipSlot::WeaponSecondary);

	if (attackCode > ATTACK_CODE_NATURAL_ATTACK)
		return objHndl::null;

	return inventory.ItemWornAt(obj, EquipSlot::WeaponPrimary);
}

ActionErrorCode D20ActionCallbacks::PerformStandardAttack(D20Actn* d20a)
{
	int hitAnimIdx = templeFuncs.RNG(0, 2);

	int d20data = d20a->data1;
	int playCritFlag = 0;
	int useSecondaryAnim = 0;
	if (d20Sys.UsingSecondaryWeapon(d20a))
	{
		d20a->d20Caf |= D20CAF_SECONDARY_WEAPON; 
		useSecondaryAnim = 1;
	}
	else if (d20a->data1 >= ATTACK_CODE_NATURAL_ATTACK + 1)
	{
		useSecondaryAnim = templeFuncs.RNG(0, 1);
		hitAnimIdx = (d20a->data1 - (ATTACK_CODE_NATURAL_ATTACK + 1)) % 3;
	}

	d20Sys.ToHitProc(d20a);

	int caflags = d20a->d20Caf;
	if (caflags & D20CAF_CRITICAL
		|| d20Sys.d20QueryWithData(d20a->d20APerformer, DK_QUE_Play_Critical_Hit_Anim, caflags, 0)) {
		playCritFlag = 1;
	}
	
	if (animationGoals.PushAttackAnim(d20a->d20APerformer, d20a->d20ATarget, 0xFFFFFFFF, hitAnimIdx, playCritFlag, useSecondaryAnim))
	{
		d20a->animID = animationGoals.GetActionAnimId(d20a->d20APerformer);
		d20a->d20Caf |= D20CAF_NEED_ANIM_COMPLETED;
	}
	return AEC_OK;
}

ActionErrorCode D20ActionCallbacks::PerformTripAttack(D20Actn* d20a)
{
	if (!d20a->d20ATarget)
		return AEC_TARGET_INVALID;

	auto performer = d20a->d20APerformer;

	objHndl weaponUsed = d20Sys.GetAttackWeapon(performer, d20a->data1, (D20CAF)d20a->d20Caf);

	
	d20a->d20Caf |= D20CAF_TOUCH_ATTACK;
	combatSys.ToHitProcessing(*d20a);
	//d20Sys.ToHitProc(d20a);
	if (animationGoals.PushAttemptAttack(d20a->d20APerformer, d20a->d20ATarget)){
		d20a->animID = animationGoals.GetActionAnimId(d20a->d20APerformer);
		d20a->d20Caf |= D20CAF_NEED_ANIM_COMPLETED;
	}
	return AEC_OK;
}

ActionErrorCode D20ActionCallbacks::PerformUseItem(D20Actn* d20a){
	dispatch.DispatchD20ActionCheck(d20a, nullptr, dispTypeD20ActionPerform);
	return PerformCastSpell(d20a);
}

int LegacyD20System::TargetWithinReachOfLoc(objHndl obj, objHndl target, LocAndOffsets* loc)
{
	return addresses.TargetWithinReachOfLoc(obj, target, loc);
}

void LegacyD20System::D20ActnSetSetSpontCast(D20SpellData* d20SpellData, SpontCastType spontCastType)
{
	d20SpellData->spontCastType = spontCastType;
	d20SpellData->metaMagicData.metaMagicFlags = 0;
	d20SpellData->metaMagicData.metaMagicEmpowerSpellCount = 0;
	d20SpellData->metaMagicData.metaMagicEnlargeSpellCount = 0;
	d20SpellData->metaMagicData.metaMagicExtendSpellCount = 0;
	d20SpellData->metaMagicData.metaMagicHeightenSpellCount = 0;
	d20SpellData->metaMagicData.metaMagicWidenSpellCount = 0;
}

D20TargetClassification LegacyD20System::TargetClassification(D20Actn* d20a)
{
	auto d20DefFlags = d20Defs[d20a->d20ActType].flags;

	if (d20DefFlags & D20ADF_Python){
		return pyactions[d20a->data1].tgtClass;
	}

	if (d20DefFlags & D20ADF::D20ADF_Movement)
	{
		return D20TargetClassification::D20TC_Movement;
	} 
	if (d20DefFlags & D20ADF_TargetSingleIncSelf)
		return D20TargetClassification::D20TC_SingleIncSelf;
	if (d20DefFlags & D20ADF_TargetSingleExcSelf)
		return D20TargetClassification::D20TC_SingleExcSelf;
	if (d20DefFlags & D20ADF_MagicEffectTargeting)
		return D20TargetClassification::D20TC_CastSpell;
	if (d20DefFlags & D20ADF_CallLightningTargeting)
		return D20TargetClassification::D20TC_CallLightning;
	if (d20DefFlags & D20ADF_TargetContainer)
		return D20TargetClassification::D20TC_ItemInteraction;
	if (d20DefFlags * D20ADF_TargetingBasedOnD20Data)
	{
		switch (d20a->data1){
		case BM_FASCINATE:
		case BM_INSPIRE_COMPETENCE:
		case BM_SUGGESTION:
		case BM_INSPIRE_GREATNESS:
			return D20TargetClassification::D20TC_SingleExcSelf;
		case BM_INSPIRE_HEROICS:
			return D20TargetClassification::D20TC_SingleIncSelf;
		default:
			return D20TargetClassification::Target0;
		}
	}
	return D20TargetClassification::Target0;
}

int LegacyD20System::TargetCheck(D20Actn* d20a)
{

	auto target = d20a->d20ATarget;
	ObjectType tgtType;
	if (target)
		tgtType = objects.GetType(target);

	auto curSeq = (*actSeqSys.actSeqCur);
	SpellEntry spellEntry;
	switch( TargetClassification(d20a))
	{
		case D20TC_SingleExcSelf:
			if (target == d20a->d20APerformer)
				return 0;
		case D20TC_SingleIncSelf:
			if (!target)
				return 0;
			if (tgtType == obj_t_pc || tgtType == obj_t_npc)
				return 1; 
			else
				return 0;
			break;
		case D20TC_ItemInteraction:
			if (!target)
				return 0;
			if (tgtType == obj_t_container)
				return 1;
			if (objects.IsCritterType(tgtType))
				return critterSys.IsDeadNullDestroyed(target);
			if (tgtType == obj_t_portal)
				return 1;
			return 0;
		case D20TC_CallLightning:
			return (*addresses.actSeqTargetsIdx) >= 0;

		case D20TC_CastSpell:
			curSeq->d20Action = d20a;
			if (curSeq->spellPktBody.caster || curSeq->spellPktBody.spellEnum)
				return TRUE;

			unsigned spellEnum, spellEnumOrg, spellClassCode, spellSlotLevel, itemSpellData, spellMetaMagicData;
			D20SpellDataExtractInfo(&d20a->d20SpellData, &spellEnum, &spellEnumOrg, &spellClassCode, &spellSlotLevel, &itemSpellData, &spellMetaMagicData);
			spellSys.spellPacketBodyReset(&curSeq->spellPktBody);
			curSeq->spellPktBody.spellEnum = spellEnum;
			curSeq->spellPktBody.spellEnumOriginal= spellEnumOrg;
			curSeq->spellPktBody.caster = d20a->d20APerformer;
			curSeq->spellPktBody.spellClass = spellClassCode;
			curSeq->spellPktBody.spellKnownSlotLevel = spellSlotLevel;
			curSeq->spellPktBody.metaMagicData = spellMetaMagicData;
			curSeq->spellPktBody.invIdx = itemSpellData;

			if (!spellSys.spellRegistryCopy(spellEnum, &spellEntry))
			{
				logger->warn("Perform Cast Spell: failed to retrieve spell entry %d!\n", spellEnum);
				return 1;
			}
			if (itemSpellData == 255)
				spellSys.SpellPacketSetCasterLevel(&curSeq->spellPktBody);
			else
				curSeq->spellPktBody.casterLevel = max(1, 2 * static_cast<int>(spellSlotLevel) - 1);

			curSeq->spellPktBody.spellRange = spellSys.GetSpellRange(&spellEntry, curSeq->spellPktBody.casterLevel, curSeq->spellPktBody.caster);
			
			if ((spellEntry.modeTargetSemiBitmask & 0xFF) != static_cast<unsigned>(UiPickerType::Personal)
				|| spellEntry.radiusTarget < 0
				|| (spellEntry.flagsTargetBitmask & UiPickerFlagsTarget::Radius))
				return 0;
			curSeq->spellPktBody.orgTargetCount = 1;
			curSeq->spellPktBody.targetCount = 1;
			curSeq->spellPktBody.targetListHandles[0] = curSeq->spellPktBody.caster;
			curSeq->spellPktBody.aoeCenter.location =
				objects.GetLocationFull(curSeq->spellPktBody.caster);
			curSeq->spellPktBody.aoeCenter.off_z =
				objects.GetOffsetZ(curSeq->spellPktBody.caster);
			if (spellEntry.radiusTarget > 0)
				curSeq->spellPktBody.spellRange = spellEntry.radiusTarget;
			return 1;

		default:
			return 1;
	}
}

BOOL LegacyD20System::IsActionOffensive(D20ActionType actionType, objHndl obj) const
{
	auto d20ADF = d20Defs[actionType].flags;
	if (d20ADF & D20ADF::D20ADF_TriggersCombat){
		if (actionType != D20A_LAY_ON_HANDS_USE)
			return 1;
		if (critterSys.IsUndead(obj))
			return 1;
	}
	return 0;
}


uint64_t LegacyD20System::d20QueryReturnData(objHndl objHnd, D20DispatcherKey dispKey, uint32_t arg1, ::uint32_t arg2)
{
	Dispatcher * dispatcher = objects.GetDispatcher(objHnd);
	if (!dispatch.dispatcherValid(dispatcher)){ return 0; }
	DispIoD20Query dispIO;
	dispIO.dispIOType = dispIOTypeQuery;
	dispIO.return_val = 0;
	dispIO.data1 = arg1;
	dispIO.data2 = arg2;
	objects.dispatch.DispatcherProcessor(dispatcher, dispTypeD20Query, dispKey, &dispIO);
	if ( dispKey == DK_QUE_Critter_Is_Charmed || dispKey == DK_QUE_Critter_Is_Afraid || dispKey == DK_QUE_Critter_Is_Held){
		// in these cases the information stored is an objhandle; make sure it's a valid handle!
		if (gameSystems->GetObj().IsValidHandle(*reinterpret_cast<objHndl*>(&dispIO.data1))){
			auto result = *reinterpret_cast<uint64_t*>(&dispIO.data1);
			return result;
		} else	{
			logger->debug("D20QueryReturnData: someone just tried to get an invalid handle!");
			return 0i64;
		}
	}
	auto result = *reinterpret_cast<uint64_t*>(&dispIO.data1);
	return result;
}

uint64_t LegacyD20System::d20QueryReturnData(objHndl objHnd, D20DispatcherKey dispKey, CondStruct* arg1, uint32_t arg2){
	return d20QueryReturnData(objHnd, dispKey, (uint32_t)arg1, arg2);
}

int LegacyD20System::D20QueryPython(const objHndl& handle, const string& queryKey, int arg1, int arg2){
	
	Dispatcher * dispatcher = objects.GetDispatcher(handle);
	if (!dispatch.dispatcherValid(dispatcher)) { return 0; }
	DispIoD20Query dispIo;
	dispIo.dispIOType = dispIOTypeQuery;
	dispIo.return_val = 0;
	dispIo.data1 = arg1;
	dispIo.data2 = arg2;
	dispatcher->Process(enum_disp_type::dispTypePythonQuery, static_cast<D20DispatcherKey>(ElfHash::Hash(queryKey)), &dispIo);
	return dispIo.return_val;
}

D20ADF LegacyD20System::GetActionFlags(D20ActionType d20ActionType){
	auto flags = d20Defs[d20ActionType].flags;
	if (flags & D20ADF_Python){
		return pyactions[d20Sys.globD20ActionKey].flags;
	}
	return flags;
}

bool LegacyD20System::D20QueryWithDataDefaultTrue(objHndl obj, D20DispatcherKey dispKey, const D20Actn* d20a, int arg2){
	auto dispatcher = gameSystems->GetObj().GetObject(obj)->GetDispatcher();
	if (!dispatcher->IsValid())
		return true;
	DispIoD20Query dispIo;
	dispIo.return_val = 1;
	dispIo.data1 = reinterpret_cast<uint32_t>(d20a);
	dispatcher->Process(dispTypeD20Query, dispKey, &dispIo);
	return dispIo.return_val != 0;
}
#pragma endregion 





#pragma region D20 Spell Stuff

void D20SpellDataExtractInfo
  (D20SpellData * d20SpellData	, uint32_t * spellEnum		, uint32_t * spellEnumOriginal	, 
   uint32_t * spellClassCode	, uint32_t * spellSlotLevel	, uint32_t * itemSpellData		, 
   uint32_t * metaMagicData)
{
	if ( ! (spellEnumOriginal == nullptr) )
	{
		*spellEnumOriginal = d20SpellData->spellEnumOrg;
	}

	if (!(spellSlotLevel == nullptr ) )
	{
		*spellSlotLevel = d20SpellData->spellSlotLevel;
	}

	if (! (spellEnum == nullptr))
	{
		if ((SpontCastType) d20SpellData->spontCastType == spontCastGoodCleric)
		{
			*spellEnum = spontCastSpellLists.spontCastSpellsGoodCleric[d20SpellData->spellSlotLevel];
		}
		else if ((SpontCastType)d20SpellData->spontCastType == spontCastEvilCleric)
		{
			*spellEnum = spontCastSpellLists.spontCastSpellsEvilCleric[d20SpellData->spellSlotLevel];
		} 
		else if ( (SpontCastType) d20SpellData->spontCastType == spontCastDruid)
		{
			*spellEnum = spontCastSpellLists.spontCastSpellsDruid[d20SpellData->spellSlotLevel];
		}
 else
 {
	 *spellEnum = d20SpellData->spellEnumOrg;
 }
	}

	if (!(spellClassCode == nullptr))
	{
		*spellClassCode = d20SpellData->spellClassCode;
	}

	if (!(itemSpellData == nullptr))
	{
		*itemSpellData = d20SpellData->itemSpellData;
	}

	if (!(metaMagicData == nullptr))
	{
		*metaMagicData = (*((uint32_t*)&(d20SpellData->metaMagicData))) & (0xFFFFFF); // sue me! it WORKS
	}

	return;
}


void __cdecl D20SpellDataSetSpontCast(D20SpellData* d20SpellData, SpontCastType spontCastType)
{
	if (!(d20SpellData == nullptr))
	{
		d20SpellData->spontCastType = spontCastType;
	}
};

#pragma endregion



#pragma region D20Status Init Functions

void _D20StatusInit(objHndl objHnd)
{
	d20Sys.d20Status->D20StatusInit(objHnd);
	}

void _D20StatusRefresh(objHndl objHnd)
{
	d20StatusSys.D20StatusRefresh(objHnd);
}

void _D20StatusInitFromInternalFields(objHndl objHnd, Dispatcher * dispatcher)
{
	d20StatusSys.D20StatusInitFromInternalFields(objHnd,  dispatcher);
}

void _D20StatusInitRace(objHndl objHnd)
{
	d20Sys.d20Status->initRace(objHnd);
};


void _D20StatusInitClass(objHndl objHnd)
{
	d20Sys.d20Status->initClass(objHnd);
};

void _D20StatusInitDomains(objHndl objHnd)
{
	d20Sys.d20Status->initDomains(objHnd);
}

void _D20StatusInitFeats(objHndl objHnd)
{
	d20Sys.d20Status->initFeats(objHnd);
};

void _D20StatusInitItemConditions(objHndl objHnd)
{
	d20Sys.d20Status->initItemConditions(objHnd);
}

#pragma endregion


uint32_t _D20Query(objHndl objHnd, D20DispatcherKey dispKey)
{
	return d20Sys.d20Query(objHnd, dispKey);
}

void _d20SendSignal(objHndl objHnd, D20DispatcherKey dispKey, int32_t arg1, int32_t arg2)
{
	d20Sys.d20SendSignal(objHnd, dispKey, arg1, arg2);
}

void __cdecl _D20aInitCdecl(objHndl objHnd, D20Actn* d20a)
{
	d20Sys.D20ActnInit(objHnd, d20a);
}


void __declspec(naked) _D20ActnInitUsercallWrapper(objHndl objHnd)
{
	__asm{ // esi is D20Actn * d20a
		push ebp; 
		mov ebp, esp; // ebp = &var4  ebp+4 = &retaddr  ebp+8 = &arg1

		push esi; 
		mov eax, [ebp + 12];
		push eax;
		mov eax, [ebp + 8];
		push eax;
		mov eax, _D20aInitCdecl;
		call eax;
		add esp, 8;

		pop esi;
		mov esp, ebp;
		pop ebp;
		retn;
	}
}

void _d20ActnSetSpellData(D20SpellData* d20SpellData, uint32_t spellEnumOrg, uint32_t spellClassCode, uint32_t spellSlotLevel, uint32_t itemSpellData, uint32_t metaMagicData)
{
	d20Sys.D20ActnSetSpellData(d20SpellData, spellEnumOrg, spellClassCode, spellSlotLevel, itemSpellData, metaMagicData);
}

void _globD20aSetTypeAndData1(D20ActionType d20type, uint32_t data1)
{
	d20Sys.GlobD20ActnSetTypeAndData1(d20type, data1);
}

uint32_t _d20QueryWithData(objHndl objHnd, D20DispatcherKey dispKey, uint32_t arg1, uint32_t arg2)
{
	return d20Sys.d20QueryWithData(objHnd, dispKey, arg1, arg2);
}

uint64_t _d20QueryReturnData(objHndl objHnd, D20DispatcherKey dispKey, uint32_t arg1, uint32_t arg2)
{
	return d20Sys.d20QueryReturnData(objHnd, dispKey, arg1, arg2);
}

void _globD20aSetPerformer(objHndl objHnd)
{
	d20Sys.globD20ActnSetPerformer(objHnd);
}

void _GlobD20ActnInit()
{
	d20Sys.GlobD20ActnInit();
}

void _GlobD20ActnSetSpellData(D20SpellData* d20SpellData)
{
	d20Sys.GlobD20ActnSetSpellData(d20SpellData);
}

int _PerformStandardAttack(D20Actn* d20a)
{
	return d20Callbacks.PerformStandardAttack(d20a);
}

objHndl _GetAttackWeapon(objHndl obj, int attackCode, D20CAF flags){
	return d20Sys.GetAttackWeapon(obj, attackCode, flags);
}

uint32_t _d20actionTabLineParser(TabFileStatus*, uint32_t n, const char** strings)
{
	D20ActionType d20ActionType = (D20ActionType)atoi(strings[0]);
	if (d20ActionType <= D20A_USE_POTION)
	{
		for (int i = 0; i < sizeof(D20ActionDef) / sizeof(int); i++)
		{
			*((int*)&d20Sys.d20Defs[d20ActionType] + i) = atoi(strings[i + 1]);
		}
	}
	return 0;

}

int _D20Init(GameSystemConf* conf){
	d20Sys.NewD20ActionsInit();
	return OrgD20Init(conf);
}

ActionErrorCode D20ActionCallbacks::ActionCheckDivineMight(D20Actn* d20a, TurnBasedStatus* tbStat)
{
	DispIoD20ActionTurnBased dispIo;
	dispIo.dispIOType = dispIOTypeD20ActionTurnBased;
	dispIo.returnVal = 0;
	dispIo.d20a = d20a;
	dispIo.tbStatus = tbStat;
	auto dispatcher = objects.GetDispatcher(d20a->d20APerformer);
	auto alignmentchoice = objects.getInt32(d20a->d20APerformer, obj_f_critter_alignment_choice);
	if (alignmentchoice == 2)
		d20a->data1 = 1;

	if (dispatch.dispatcherValid(dispatcher))
	{
		dispatch.DispatcherProcessor(dispatcher, dispTypeD20ActionCheck, DK_D20A_TURN_UNDEAD , &dispIo);
	}

	return static_cast<ActionErrorCode>(dispIo.returnVal);
}

ActionErrorCode D20ActionCallbacks::PerformDivineMight(D20Actn* d20a)
{
	DispIoD20ActionTurnBased dispIo;
	dispIo.dispIOType = dispIOTypeD20ActionTurnBased;
	dispIo.returnVal = 0;
	dispIo.d20a = d20a;
	dispIo.tbStatus = nullptr;
	auto dispatcher = objects.GetDispatcher(d20a->d20APerformer);
	if (dispatch.dispatcherValid(dispatcher))
	{
		dispatch.DispatcherProcessor(dispatcher, dispTypeD20ActionPerform, DK_D20A_DIVINE_MIGHT, &dispIo); // reduces the turn undead charge by 1
	}

	auto chaScore = objects.StatLevelGet(d20a->d20APerformer, stat_charisma);
	auto chaMod = objects.GetModFromStatLevel(chaScore);
	conds.AddTo(d20a->d20APerformer, "Divine Might Bonus", { chaMod, 0 });
	
	return static_cast<ActionErrorCode>(dispIo.returnVal);
}

ActionErrorCode D20ActionCallbacks::PerformEmptyBody(D20Actn* d20a)
{
	DispIoD20ActionTurnBased dispIo;
	dispIo.dispIOType = dispIOTypeD20ActionTurnBased;
	dispIo.returnVal = 0;
	dispIo.d20a = d20a;
	dispIo.tbStatus = nullptr;
	
	auto dispatcher = objects.GetDispatcher(d20a->d20APerformer);

	int numRounds =( d20Sys.d20QueryReturnData(d20a->d20APerformer, DK_QUE_Empty_Body_Num_Rounds, 1, 0) & 0xffFFffFF00000000) >> 32;
	dispIo.d20a->data1 = numRounds; // number of rounds to deduct from max possible

	if (dispatch.dispatcherValid(dispatcher)){
		dispatch.DispatcherProcessor(dispatcher, dispTypeD20ActionPerform, DK_D20A_EMPTY_BODY, &dispIo); // reduces the number of remaining rounds by 1
	}

	conds.AddTo(d20a->d20APerformer, "Ethereal", { 0, 0 , numRounds});

	return static_cast<ActionErrorCode>(dispIo.returnVal);
}

ActionErrorCode D20ActionCallbacks::PerformPython(D20Actn* d20a){
	DispIoD20ActionTurnBased dispIo(d20a);
	dispIo.DispatchPythonActionPerform((D20DispatcherKey)d20a->data1);
	
	return (ActionErrorCode)dispIo.returnVal; 
}

ActionErrorCode D20ActionCallbacks::PerformQuiveringPalm(D20Actn* d20a){
	DispIoD20ActionTurnBased dispIo(d20a);
	dispIo.DispatchPerform(DK_D20A_QUIVERING_PALM);

	if (dispIo.returnVal != AEC_OK)
		return static_cast<ActionErrorCode>(dispIo.returnVal);

	auto d20data = d20a->data1;
	auto playCritFlag = 0;
	d20Sys.ToHitProc(d20a);
	auto caflags = d20a->d20Caf;
	if (caflags & D20CAF_CRITICAL
		|| d20Sys.d20QueryWithData(d20a->d20APerformer, DK_QUE_Play_Critical_Hit_Anim, caflags, 0))
	{
		playCritFlag = 1;
	}
	auto attackAnimSubid = templeFuncs.RNG(0, 2);
	
	
	if (animationGoals.PushAttackAnim(d20a->d20APerformer, d20a->d20ATarget, 0xFFFFFFFF, attackAnimSubid, playCritFlag, 0))	{
		d20a->animID = animationGoals.GetActionAnimId(d20a->d20APerformer);
		d20a->d20Caf |= D20CAF_NEED_ANIM_COMPLETED;
	}

	return AEC_OK;
	
}

ActionErrorCode D20ActionCallbacks::PerformSneak(D20Actn* d20a){
	auto performer = d20a->d20APerformer;
	
	auto newSneakState = 1 - critterSys.IsMovingSilently(performer);
	animationGoals.Interrupt(performer, AnimGoalPriority::AGP_5);

	if (newSneakState && combatSys.isCombatActive()){ // entering sneak while in combat

		auto hasHideInPlainSight = d20Sys.D20QueryPython(performer, "Can Hide In Plain Sight");

		auto N = combatSys.GetInitiativeListLength();

		BonusList sneakerBon;
		sneakerBon.AddBonus(-20, 0, 349); // Hiding in Combat
		auto sneakerHide = dispatch.dispatch1ESkillLevel(performer, SkillEnum::skill_hide, &sneakerBon, performer, 1);
		auto hideRoll = Dice(1, 20, 0).Roll();

		for (auto i = 0; i < N; i++){
			auto combatant = combatSys.GetInitiativeListMember(i);
			if (!combatant || combatant == performer)
				continue;
			
			if (critterSys.IsFriendly(combatant, performer) || critterSys.AllegianceShared(combatant, performer))
				continue;

			if (critterSys.HasLineOfSight(combatant, performer) == 0)	{ // note: the function actually returns obstacles

				if (!hasHideInPlainSight)
					return AEC_INVALID_ACTION;

				BonusList spotterBon;
				auto combatantSpot = dispatch.dispatch1ESkillLevel(combatant, SkillEnum::skill_spot, &spotterBon, combatant, 1);
				auto spotRoll = Dice(1, 20, 0).Roll();
				if (combatantSpot + spotRoll > hideRoll + sneakerHide){
					auto rollHistId = histSys.RollHistoryAddType6OpposedCheck(performer, combatant, hideRoll, spotRoll, &sneakerBon, &spotterBon, 5123, 103, 1);
					histSys.CreateRollHistoryString(rollHistId);
					return AEC_INVALID_ACTION;
				}
					
			}

		}
	}

	critterSys.SetMovingSilently(performer, newSneakState);
	return AEC_OK;
}

ActionErrorCode D20ActionCallbacks::ActionFrameQuiveringPalm(D20Actn* d20a){

	objHndl performer = d20a->d20APerformer;
	
	d20Sys.d20Defs[D20A_STANDARD_ATTACK].actionFrameFunc(d20a);
	if (!(d20a->d20Caf & D20CAF_HIT))
		return AEC_OK;


	auto monkLvl = objects.StatLevelGet(performer, stat_level_monk);
	auto wisScore = objects.StatLevelGet(performer, stat_wisdom);
	auto dc = 10 + monkLvl / 2 + (wisScore-10)/2;
	if (!damage.SavingThrow( d20a->d20ATarget, performer, dc, SavingThrowType::Fortitude, D20STF_NONE)){
		critterSys.KillByEffect(d20a->d20ATarget, performer);
		histSys.CreateFromFreeText(fmt::format("{} killed by Quivering Palm!",  description.getDisplayName(d20a->d20ATarget)).c_str() );
		combatSys.FloatTextBubble(performer, 215);
	} 
		
	return AEC_OK;
}

ActionErrorCode D20ActionCallbacks::ActionFrameStandardAttack(D20Actn* d20a){

	if (d20Sys.d20Query(d20a->d20APerformer, DK_QUE_Prone))	{
		//histSys.CreateFromFreeText(fmt::format("{} aborted attack (prone).", description.getDisplayName(d20a->d20APerformer)).c_str());
		return AEC_CANT_WHILE_PRONE;
	}
	histSys.CreateRollHistoryString(d20a->rollHist1);
	histSys.CreateRollHistoryString(d20a->rollHist2);
	histSys.CreateRollHistoryString(d20a->rollHist3);
	auto makeAttack = temple::GetRef<int(__cdecl)(objHndl, objHndl, int, D20CAF, D20ActionType)>(0x100B7950);
	makeAttack(d20a->d20APerformer, d20a->d20ATarget, d20a->data1, static_cast<D20CAF>(d20a->d20Caf), d20a->d20ActType);
	return AEC_NOT_ENOUGH_TIME1;
}

ActionErrorCode D20ActionCallbacks::ActionCheckDisarm(D20Actn* d20a, TurnBasedStatus* tbStat)
{
	objHndl performer = d20a->d20APerformer;
	if (d20Sys.d20Query(performer, DK_QUE_Prone) || d20Sys.d20Query(performer, DK_QUE_Unconscious))
		return AEC_CANT_WHILE_PRONE;
	

	objHndl weapon = inventory.ItemWornAt(d20a->d20APerformer, 3);
	int weapFlags; 
		
	if (weapon && (weapFlags = objects.getInt32(weapon, obj_f_weapon_flags), weapFlags & OWF_RANGED_WEAPON)) // ranged weapon - Need Melee Weapon error
	{
		return AEC_NEED_MELEE_WEAPON;
	}

	if (d20a->d20ATarget){
		objHndl targetWeapon = inventory.ItemWornAt(d20a->d20ATarget, 3);
		if (!targetWeapon)
		{
			targetWeapon = inventory.ItemWornAt(d20a->d20ATarget, 4);
			if (!targetWeapon)
				return AEC_TARGET_INVALID;
			if (objects.GetType(targetWeapon) != obj_t_weapon)
				return AEC_TARGET_INVALID;
		}
	}
	else
		return AEC_TARGET_INVALID;

	return AEC_OK;
}

ActionErrorCode D20ActionCallbacks::ActionCheckEmptyBody(D20Actn* d20a, TurnBasedStatus* tbStat){
	if (d20Sys.d20Query(d20a->d20APerformer, DK_QUE_Is_Ethereal))
		return AEC_INVALID_ACTION;
	int numRounds = d20Sys.d20QueryReturnData(d20a->d20APerformer, DK_QUE_Empty_Body_Num_Rounds, 2, 0) >> 32;
	if (numRounds <= 0)
		return AEC_OUT_OF_CHARGES;

	if (d20a->data1 >  numRounds) {
		d20a->data1 = numRounds;
	}

	return AEC_OK;
}

ActionErrorCode D20ActionCallbacks::ActionCheckPython(D20Actn* d20a, TurnBasedStatus* tbStat) {

	DispIoD20ActionTurnBased evtObj(d20a);

	evtObj.DispatchPythonActionCheck((D20DispatcherKey)d20a->data1);

	return (ActionErrorCode)evtObj.returnVal;

}

ActionErrorCode D20ActionCallbacks::ActionCheckQuiveringPalm(D20Actn* d20a, TurnBasedStatus* tbStat)
{

	if (!d20a->d20ATarget){
		return AEC_TARGET_INVALID;
	}
	
	if ( ! combatSys.IsUnarmed(d20a->d20APerformer)){
		return AEC_INVALID_ACTION;
	}

	if (!d20Sys.d20Query(d20a->d20APerformer, DK_QUE_Quivering_Palm_Can_Perform))
		return AEC_OUT_OF_CHARGES;

	if (d20Sys.d20Query(d20a->d20ATarget, DK_QUE_Critter_Is_Immune_Critical_Hits)){
		return AEC_TARGET_INVALID;
	}

	return AEC_OK;
}

ActionErrorCode D20ActionCallbacks::ActionCheckSneak(D20Actn* d20a, TurnBasedStatus* tbStat){
	if (critterSys.IsMovingSilently(d20a->d20APerformer)) // will cause to stop sneaking
		return AEC_OK;

	return AEC_OK; // used to be possible only outside of combat, but now you can attempt it in combat too
}

ActionErrorCode D20ActionCallbacks::PerformCharge(D20Actn* d20a){

	int crit = 0, isSecondary = 0;
	auto performer = d20a->d20APerformer;
	conds.AddTo(performer, "Charging", {0});
	d20a->d20Caf |= D20CAF_CHARGE;

	D20Actn d20aCopy = *d20a;

	auto weapon = inventory.ItemWornAt(performer, EquipSlot::WeaponPrimary);
	if (!weapon) {
		weapon = inventory.ItemWornAt(performer, EquipSlot::WeaponSecondary);
		if (weapon) {
			d20a->d20Caf |= D20CAF_SECONDARY_WEAPON;
			d20a->data1 = ATTACK_CODE_OFFHAND + 1;
		}
		else if (dispatch.DispatchD20ActionCheck(&d20aCopy, nullptr, dispTypeGetCritterNaturalAttacksNum))
		{
			d20a->data1 = ATTACK_CODE_NATURAL_ATTACK + 1;
		}
		else {
			d20a->data1 = ATTACK_CODE_PRIMARY + 1;
		}
	}

	return PerformStandardAttack(d20a);
}

ActionErrorCode D20ActionCallbacks::PerformDisarm(D20Actn* d20a){

	if (animationGoals.PushAttemptAttack(d20a->d20APerformer, d20a->d20ATarget))
	{
		d20a->animID = animationGoals.GetActionAnimId(d20a->d20APerformer);
		d20a->d20Caf |= D20CAF_NEED_ANIM_COMPLETED;
	}
	return AEC_OK;
};


ActionErrorCode D20ActionCallbacks::ActionFrameCharge(D20Actn* d20a){
	ActionFrameStandardAttack(d20a);
	return AEC_OK;
}

ActionErrorCode D20ActionCallbacks::ActionFrameDisarm(D20Actn* d20a){
	objHndl performer = d20a->d20APerformer;
	int failedOnce = 0;
	if (!d20Sys.d20Query(d20a->d20APerformer, DK_QUE_Can_Perform_Disarm))
	{
		objects.floats->FloatCombatLine(d20a->d20APerformer, 195); //fail!
		failedOnce = 1;
	}
		
	else if (combatSys.DisarmCheck(d20a->d20APerformer, d20a->d20ATarget, d20a))
	{
		objHndl weapon = inventory.ItemWornAt(d20a->d20ATarget, 3);
		objHndl attackerWeapon = inventory.ItemWornAt(performer, 3);
		objHndl attackerOffhand = inventory.ItemWornAt(performer, 4);
		objHndl attackerShield = inventory.ItemWornAt(performer, 11);
		if (!weapon)
			weapon = inventory.ItemWornAt(d20a->d20ATarget, 4);
		if (!attackerWeapon && !attackerOffhand)
		{
			if ( (inventory.GetWieldType(d20a->d20APerformer, weapon) != 2 || !attackerShield
				|| objects.GetItemWearFlags(attackerShield) & OIF_WEAR_BUCKLER ) 
				&& objects.StatLevelGet(d20a->d20APerformer, stat_level_monk) == 0)
				inventory.ItemGetAdvanced(weapon, d20a->d20APerformer, 203, 0);
			else
			{
				inventory.ItemDrop(weapon);
				inventory.SetItemParent(weapon, d20a->d20APerformer, 32);
			}
			objects.floats->FloatCombatLine(d20a->d20ATarget, 202);
		} 
		else if (weapon)
		{
			inventory.ItemDrop(weapon);
			objects.floats->FloatCombatLine(d20a->d20ATarget, 198);
		}
		
		struct DisarmedArgs
		{
			objHndl weapon;
		} disarmedArgs;
		disarmedArgs.weapon = weapon;

		conds.AddTo(d20a->d20ATarget, "Disarmed", { ((int*)&disarmedArgs)[0], ((int*)&disarmedArgs)[1],0,0,0,0,0,0 });
		return AEC_OK;
	} 

	

	// counter attempt
	if (!feats.HasFeatCountByClass(d20a->d20APerformer, FEAT_IMPROVED_DISARM))
	{
		D20Actn d20aCopy;
		memcpy(&d20aCopy, d20a, sizeof(D20Actn));
		d20aCopy.d20APerformer = d20a->d20ATarget;
		d20aCopy.d20ATarget = d20a->d20APerformer;
		if (!d20Sys.d20Defs[D20A_DISARM].actionCheckFunc(&d20aCopy, nullptr))
		{
			if( animationGoals.PushAttemptAttack(d20aCopy.d20APerformer, d20aCopy.d20ATarget))
				d20aCopy.animID = animationGoals.GetActionAnimId(d20aCopy.d20APerformer);
			if (combatSys.DisarmCheck(d20a->d20ATarget, d20a->d20APerformer, d20a))
			{

				objHndl weapon = inventory.ItemWornAt(d20a->d20APerformer, 3);
				if (!weapon)
					weapon = inventory.ItemWornAt(d20a->d20APerformer, 4);
				if (weapon)
				{
					inventory.ItemDrop(weapon);
				}
				struct DisarmedArgs
				{
					objHndl weapon;
				} disarmedArgs;
				disarmedArgs.weapon = weapon;
				objects.floats->FloatCombatLine(d20a->d20APerformer, 200); // Counter Disarmed!
				conds.AddTo(d20a->d20APerformer, "Disarmed", { ((int*)&disarmedArgs)[0], ((int*)&disarmedArgs)[1], 0,0,0,0,0,0 });
				return AEC_OK;
			}
			else if (!failedOnce)
			{
				objects.floats->FloatCombatLine(d20a->d20APerformer, 195);
			}
		}
		else if (!failedOnce)
		{
			objects.floats->FloatCombatLine(d20a->d20APerformer, 195);
		}
		
	} else if (!failedOnce)
	{
		objects.floats->FloatCombatLine(d20a->d20APerformer, 195);
	}

	
	

	return AEC_OK;
};

ActionErrorCode D20ActionCallbacks::ActionFramePython(D20Actn* d20a){
	return AEC_OK; // TODO
}

#pragma region Retrieve Disarmed Weapon
ActionErrorCode D20ActionCallbacks::LocationCheckDisarmedWeaponRetrieve(D20Actn* d20a, TurnBasedStatus* tbStat, LocAndOffsets* loc)
{
	if (!combatSys.isCombatActive())
		return AEC_OK;
	if (d20a->d20ATarget)
		return d20Sys.TargetWithinReachOfLoc(d20a->d20APerformer, d20a->d20ATarget, loc) != 0 ? AEC_OK : AEC_TARGET_TOO_FAR;
	objHndl weapon{ d20Sys.d20QueryReturnData(d20a->d20APerformer, DK_QUE_Disarmed) };
	if (weapon)
		return d20Sys.TargetWithinReachOfLoc(d20a->d20APerformer, weapon, loc) != 0 ? AEC_OK : AEC_TARGET_TOO_FAR;
	return AEC_TARGET_INVALID;
}

ActionErrorCode D20ActionCallbacks::LocationCheckPython(D20Actn* d20a, TurnBasedStatus* tbStat, LocAndOffsets* loc){
	return AEC_OK; // TODO
};

ActionErrorCode D20ActionCallbacks::ActionCheckDisarmedWeaponRetrieve(D20Actn* d20a, TurnBasedStatus* tbStat){
	int dummy = 1;
	return AEC_OK;
};

ActionErrorCode D20ActionCallbacks::PerformDisarmedWeaponRetrieve(D20Actn* d20a){
	d20Sys.d20SendSignal(d20a->d20APerformer, DK_SIG_Disarmed_Weapon_Retrieve, (int)d20a, 0);
	return AEC_OK;
};

#pragma endregion

ActionErrorCode D20ActionCallbacks::ActionCheckSunder(D20Actn* d20a, TurnBasedStatus* tbStat){
	objHndl weapon = inventory.ItemWornAt(d20a->d20APerformer, 3);
	int weapFlags;

	if (!weapon)
		return AEC_NEED_MELEE_WEAPON;
	weapFlags = objects.getInt32(weapon, obj_f_weapon_flags);
	if ( weapFlags & OWF_RANGED_WEAPON) // ranged weapon - Need Melee Weapon error
	{
		return AEC_NEED_MELEE_WEAPON;
	}
	
	if (!weapons.IsSlashingOrBludgeoning(weapon))
		return AEC_WRONG_WEAPON_TYPE;

	if (d20a->d20ATarget)
	{
		objHndl targetWeapon = inventory.ItemWornAt(d20a->d20ATarget, 3);
		if (!targetWeapon)
		{
			targetWeapon = inventory.ItemWornAt(d20a->d20ATarget, 4);
			if (targetWeapon)
				return AEC_OK;
			targetWeapon = inventory.ItemWornAt(d20a->d20ATarget, 11); // shield
			if (!targetWeapon)
				return AEC_TARGET_INVALID;
		}
	}
	else
		return AEC_TARGET_INVALID;

	return AEC_OK;
}

ActionErrorCode D20ActionCallbacks::ActionCheckTripAttack(D20Actn* d20a, TurnBasedStatus* tbStat){

	auto weapon = inventory.ItemWornAt(d20a->d20APerformer, EquipSlot::WeaponPrimary);
	if (weapon)
	{
		auto weapFlags = gameSystems->GetObj().GetObject(weapon)->GetInt32(obj_f_weapon_flags);
		if (weapFlags & OWF_RANGED_WEAPON)
		{
			return AEC_NEED_MELEE_WEAPON;
		}
	}

	// doing trip on a full attack
	//if (tbStat->tbsFlags & TBSF_FullAttack && !d20a->d20ATarget)
	//	return AEC_OK;


	if (!d20a->d20ATarget )
		return AEC_TARGET_INVALID;

	if (d20Sys.d20Query(d20a->d20ATarget, DK_QUE_Prone))
		return AEC_INVALID_ACTION;

	return AEC_OK;
}

ActionErrorCode D20ActionCallbacks::ActionFrameSunder(D20Actn* d20a){

	if (combatSys.SunderCheck(d20a->d20APerformer, d20a->d20ATarget, d20a))
	{

		objHndl weapon = inventory.ItemWornAt(d20a->d20ATarget, 3);
		if (!weapon)
			weapon = inventory.ItemWornAt(d20a->d20ATarget, 4);
		if (weapon)
			inventory.ItemDrop(weapon);
		//objects.floats->FloatCombatLine(d20a->d20ATarget, 198);
	}


	return AEC_OK;
}

ActionErrorCode D20ActionCallbacks::ActionFrameTouchAttack(D20Actn* d20a){

	histSys.CreateRollHistoryString(d20a->rollHist1);
	histSys.CreateRollHistoryString(d20a->rollHist2);
	histSys.CreateRollHistoryString(d20a->rollHist3);

	if (d20a->d20Caf & D20CAF_RANGED)
		return AEC_OK;


	auto curSeq = *actSeqSys.actSeqCur;
	if (!(d20a->d20Caf & D20CAF_HIT))// touch attack charges should remain until discharged
	{
		curSeq->tbStatus.tbsFlags &= ~TBSF_TouchAttack;
		d20a->d20Caf &= ~D20CAF_FREE_ACTION;
		return AEC_OK;
	}
		

	d20Sys.d20SendSignal(d20a->d20APerformer, DK_SIG_TouchAttack, d20a, 0);
	
	if (curSeq){
		curSeq->tbStatus.tbsFlags &= ~TBSF_TouchAttack;
		d20a->d20Caf &= ~D20CAF_FREE_ACTION;
	}

	return AEC_OK;
}

ActionErrorCode D20ActionCallbacks::ActionFrameTripAttack(D20Actn* d20a){

	if (!d20a->d20ATarget)
		return AEC_TARGET_INVALID;

	auto tgt = d20a->d20ATarget;
	auto performer = d20a->d20APerformer;

	if (!(d20a->d20Caf & D20CAF_HIT))	{
		combatSys.FloatCombatLine(d20a->d20APerformer, 29); //miss
		return AEC_OK;
	}

	histSys.CreateRollHistoryString(d20a->rollHist1);
	histSys.CreateRollHistoryString(d20a->rollHist2);
	histSys.CreateRollHistoryString(d20a->rollHist3);

	// auto tripCheck = temple::GetRef<BOOL(__cdecl)(objHndl, objHndl)>(0x100B6230);
	if (combatSys.TripCheck(d20a->d20APerformer, tgt))	{
		conds.AddTo(d20a->d20ATarget, "Prone", {});
		animationGoals.PushAnimate(tgt, 64);
		histSys.CreateRollHistoryLineFromMesfile(44, performer, tgt);
		combatSys.FloatCombatLine(tgt, 104);

		if (feats.HasFeatCountByClass(performer, FEAT_IMPROVED_TRIP))	{
			/*if(d20a->d20Caf & D20CAF_ATTACK_OF_OPPORTUNITY || d20a->d20ActType == D20A_ATTACK_OF_OPPORTUNITY)
			{
				return AEC_OK;
			}*/
			auto insertD20Action = temple::GetRef<void(__cdecl)(objHndl, D20ActionType, int, objHndl, LocAndOffsets, int)>(0x10094B40);
			insertD20Action(performer, D20A_STANDARD_ATTACK, d20a->data1, d20a->d20ATarget, d20a->destLoc, 0);
			auto curSeq = *actSeqSys.actSeqCur;
			curSeq->d20ActArray[curSeq->d20aCurIdx + 1].d20Caf |= D20CAF_FREE_ACTION;
			return AEC_OK;
		}
	} else // counter attempt
	{
		if (combatSys.TripCheck(tgt, performer))
		{
			conds.AddTo(performer, "Prone", {});
			animationGoals.PushAnimate(performer, 64);
			combatSys.FloatCombatLine(performer, 104);
			histSys.CreateRollHistoryLineFromMesfile(44, tgt, performer);
			return AEC_OK;
		}
		combatSys.FloatCombatLine(performer, 103);
	}

	return AEC_OK;
}

ActionErrorCode D20ActionCallbacks::PerformAidAnotherWakeUp(D20Actn* d20a){

	if (animationGoals.PushAttemptAttack(d20a->d20APerformer, d20a->d20ATarget))
	{
		animationGoals.PushUseSkillOn(d20a->d20APerformer, d20a->d20ATarget, SkillEnum::skill_heal);
		d20a->animID = animationGoals.GetActionAnimId(d20a->d20APerformer);
		d20a->d20Caf |= D20CAF_NEED_ANIM_COMPLETED;

		//if (!party.IsInParty(d20a->d20APerformer) )
		{
			char blargh[1000];
			memcpy(blargh, "Wake up!", sizeof("Wake up!"));
			uiDialog.ShowTextBubble(d20a->d20APerformer, d20a->d20APerformer, { blargh }, -1);
			
		}
	}
	return AEC_OK;
}

ActionErrorCode D20ActionCallbacks::PerformAoo(D20Actn* d20a)
{
	if (!d20a->d20APerformer)
		return AEC_INVALID_ACTION;

	auto performer = d20a->d20APerformer;

	if (!d20a->d20ATarget)
		return AEC_TARGET_INVALID;

	auto tgt = d20a->d20ATarget;

	combatSys.FloatCombatLine(performer, 43); // attack of opportunity
	histSys.CreateRollHistoryLineFromMesfile(1, performer, tgt);

	if (d20Sys.d20Query(d20a->d20APerformer, DK_QUE_Trip_AOO) && !d20Sys.d20Query(d20a->d20ATarget, DK_QUE_Prone) )	{
		return PerformTripAttack(d20a);
	} 
	// else do standard attack
	return PerformStandardAttack(d20a);
}

ActionErrorCode D20ActionCallbacks::PerformCastItemSpell(D20Actn* d20a){

	int spellEnum = 0, spellClass, spellLvl, invIdx;
	MetaMagicData mmData;
	d20a->d20SpellData.Extract(&spellEnum, nullptr, &spellClass, &spellLvl, &invIdx, &mmData);
	if (invIdx == INV_IDX_INVALID)
		return AEC_OK;

	auto item = inventory.GetItemAtInvIdx(d20a->d20APerformer, invIdx);
	if (!item)
		return AEC_OK;
	auto itemObj = gameSystems->GetObj().GetObject(item);
	auto itemFlags = (ItemFlag)itemObj->GetInt32(obj_f_item_flags);

	if (d20a->d20ActType == D20A_ACTIVATE_DEVICE_SPELL || !(item))
		return AEC_OK;

	auto caster = d20a->d20APerformer;

	auto useMagicDeviceBase = critterSys.SkillBaseGet(d20a->d20APerformer, SkillEnum::skill_use_magic_device);
	int resultDeltaFromDc;
	if (!inventory.IsIdentified(item) && itemObj->type != obj_t_food){ // blind use of magic item
		if (itemObj->type == obj_t_scroll || !useMagicDeviceBase)
			return AEC_CANNOT_CAST_SPELLS;
		auto umdRoll = skillSys.SkillRoll(d20a->d20APerformer, SkillEnum::skill_use_magic_device, 25, &resultDeltaFromDc, 1);
		if (!umdRoll){
			skillSys.FloatError(d20a->d20APerformer, 5); /// casting mishap
			if (resultDeltaFromDc >= 0)	{
				skillSys.FloatError(d20a->d20APerformer, 6);
				inventory.ItemSpellChargeConsume(item);
				return AEC_CANNOT_CAST_SPELLS;
			}
		}
	}


	// check if item requires knowing the spell
	if  (itemObj->type == obj_t_scroll || (itemFlags & OIF_NEEDS_SPELL ) && (itemObj->type == obj_t_generic || itemObj->type == obj_t_weapon)){

		auto isOk = false;

		if (critterSys.HashMatchingClassForSpell(d20a->d20APerformer, spellEnum)){
			isOk = true;
		}
		else if (spellSys.IsArcaneSpellClass(spellClass) ){
			auto clrLvl = objects.StatLevelGet(caster, stat_level_cleric);
			if (clrLvl > 0 && critterSys.HasDomain(caster, Domain_Magic)
				&& spellLvl <= max(1,clrLvl/2) )
				isOk = true;
		}

		if (!isOk){

			// do Use Magic Device roll
			if (!useMagicDeviceBase)
				return AEC_CANNOT_CAST_SPELLS;

			if (itemObj->type == obj_t_scroll) {

				auto umdRoll = skillSys.SkillRoll(d20a->d20APerformer, SkillEnum::skill_use_magic_device, 20, &resultDeltaFromDc, 1);
				if (!umdRoll) {
					skillSys.FloatError(d20a->d20APerformer, 7); // Use Scroll Failed
					if (*actSeqSys.actSeqCur) {
						(*actSeqSys.actSeqCur)->spellPktBody.Reset();
					}
					return AEC_CANNOT_CAST_SPELLS;
				}
			}
			else {
				auto umdRoll = skillSys.SkillRoll(d20a->d20APerformer, SkillEnum::skill_use_magic_device, 20, &resultDeltaFromDc, 1);
				if (!umdRoll) {
					skillSys.FloatError(d20a->d20APerformer, 8); // Use Wand Failed
					return AEC_CANNOT_CAST_SPELLS;
				}
			}
		}
	}
		

	if (itemObj->type == obj_t_scroll){
		if (!spellSys.CheckAbilityScoreReqForSpell(d20a->d20APerformer, spellEnum, -1))	{
			if (!useMagicDeviceBase){
				if (*actSeqSys.actSeqCur) {
					(*actSeqSys.actSeqCur)->spellPktBody.Reset();
				}
				return AEC_CANNOT_CAST_SPELLS;
			}

			auto umdRoll = skillSys.SkillRoll(d20a->d20APerformer, SkillEnum::skill_use_magic_device, 20, &resultDeltaFromDc, 1);
			if (!umdRoll) {
				skillSys.FloatError(d20a->d20APerformer, 9); // Emulate Ability Score Failed
				return AEC_CANNOT_CAST_SPELLS;
			}
		}
	}

	auto weapFlags = (WeaponFlags)0;
	if (itemObj->type == obj_t_weapon){
		weapFlags = (WeaponFlags)itemObj->GetInt32(obj_f_weapon_flags);
	}

	auto chargesUsedUp = 1;
	if ( (weapFlags & OWF_MAGIC_STAFF) && spellEnum == 379){ // raise dead
		chargesUsedUp = 5;
	}

	inventory.ItemSpellChargeConsume(item, chargesUsedUp);

	return AEC_OK;
}

ActionErrorCode D20ActionCallbacks::PerformCastSpell(D20Actn* d20a){
	
	int spellEnum = 0, spellClass, spellLvl, invIdx;
	MetaMagicData mmData;
	d20a->d20SpellData.Extract(&spellEnum, nullptr, &spellClass, &spellLvl, &invIdx, &mmData);
	SpellStoreData spellData(spellEnum, spellLvl, spellClass, mmData );

	objHndl item = objHndl::null;
	

	auto &curSeq = *actSeqSys.actSeqCur;
	auto &spellPkt = curSeq->spellPktBody;

	// if it's an item spell
	if (invIdx != INV_IDX_INVALID){
		spellPkt.invIdx = invIdx;
		spellPkt.spellEnumOriginal = spellEnum;
		item = inventory.GetItemAtInvIdx(spellPkt.caster, invIdx);
	}

	SpellEntry spellEntry(spellPkt.spellEnum);

	// spell interruption
	auto spellInterruptApply = [](int spellSchool, objHndl &caster, int invIdx)->BOOL{
		conds.AddTo(caster, "Spell Interrupted", { 0,0,0 });
		auto item = objHndl::null;
		if (invIdx != INV_IDX_INVALID){
			item = inventory.GetItemAtInvIdx(caster, invIdx);
		}
		return animationGoals.PushSpellInterrupt(caster, item, ag_attempt_spell_w_cast_anim, spellSchool);
	};

	if (d20Sys.SpellIsInterruptedCheck(d20a, invIdx, &spellData)){
		if (invIdx == INV_IDX_INVALID){
			spellPkt.MemorizedUseUp(spellData);
			spellPkt.Debit();
		}
		spellInterruptApply(spellEntry.spellSchoolEnum, spellPkt.caster, invIdx);
		if (curSeq)
			curSeq->spellPktBody.Reset();
		return AEC_OK;
	}

	auto result = PerformCastItemSpell(d20a);

	if (result){
		if (curSeq)
			curSeq->spellPktBody.Reset();
		return result;
	}

	
	// acquire D20Action target from the spell packet if none is present
	if (!d20a->d20ATarget && !spellEntry.projectileFlag){
		if (spellPkt.targetCount > 0){
			d20a->d20ATarget = spellPkt.targetListHandles[0];
		}
	}

	// charge GP spell component
	if (spellPkt.invIdx == INV_IDX_INVALID && spellEntry.costGP > 0){
		if (party.IsInParty(spellPkt.caster)){
			party.DebitMoney(0, spellEntry.costGP, 0, 0);
		}
	}


	static auto blinkSpellHandler = [](D20Actn *d20a, SpellEntry &spellEntry)->bool{
		if (!d20Sys.d20QueryWithData(d20a->d20APerformer, DK_QUE_Critter_Has_Condition, conds.GetByName("sp-Blink"), 0))
			return false;

		auto modeTgt = (UiPickerType) spellEntry.modeTargetSemiBitmask;
		if (!spellEntry.IsBaseModeTarget(UiPickerType::Single))
			return false;
		// "Spell failure due to Blink" roll
		auto rollRes = Dice(1, 100, 0).Roll();
		if (rollRes >= 50){ 
			histSys.RollHistoryType5Add(d20a->d20APerformer, d20a->d20ATarget, 50, 111, rollRes, 62, 192);
			return false;
		} 
		else{
			floatSys.FloatSpellLine(d20a->d20APerformer, 30015, FloatLineColor::White);
			gameSystems->GetParticleSys().CreateAtObj("Fizzle", d20a->d20ATarget);
			histSys.RollHistoryType5Add(d20a->d20APerformer, d20a->d20ATarget, 50, 111, rollRes, 112, 192); // Miscast (Blink)!
			if (*actSeqSys.actSeqCur){
				(*actSeqSys.actSeqCur)->spellPktBody.Reset();
				return false; // this was the original code, not sure if ok
			}
		}
		return false;
	};

	if (spellPkt.targetCount > 0) {
		int targetsAftedProcessing = d20Sys.CastSpellProcessTargets(d20a, spellPkt);
		blinkSpellHandler(d20a, spellEntry); // originally this cheked the result, but the result was always 0 anyway
		auto filterResult = actSeqSys.SpellTargetsFilterInvalid(*d20a);
		auto modeTgt = (UiPickerType)spellEntry.modeTargetSemiBitmask;
		if ((!filterResult || !targetsAftedProcessing)
			&& !spellEntry.IsBaseModeTarget(UiPickerType::Area)
			&& !spellEntry.IsBaseModeTarget(UiPickerType::Cone)
			&& spellEntry.IsBaseModeTarget(UiPickerType::Location)) {
			spellPkt.Debit();
			spellInterruptApply(spellEntry.spellSchoolEnum, spellPkt.caster, invIdx); // note: perhaps the current sequence changes due to the applied interrupt
			if (!party.IsInParty(curSeq->spellPktBody.caster)) {
				auto leader = party.GetConsciousPartyLeader();
				auto targetRot = objects.GetRotationTowards(curSeq->spellPktBody.caster, leader);
				animationGoals.PushRotate(curSeq->spellPktBody.caster, targetRot);
			}
			if (curSeq) {
				curSeq->spellPktBody.Reset();
			}
			return AEC_OK;
		}
	}


	if (temple::GetRef<BOOL(__cdecl)(SpellPacketBody &, objHndl)>(0x10079790)(spellPkt, item) ){ // PushSpellcastAnim
		d20a->d20Caf |= D20CAF_NEED_ANIM_COMPLETED;
		d20a->animID = animationGoals.GetActionAnimId(d20a->d20APerformer);
	}

	// provoke hostility
	for (auto i = 0u; i < curSeq->spellPktBody.targetCount; i++){
		auto &tgt = curSeq->spellPktBody.targetListHandles[i];
		if (!tgt)
			continue;
		auto tgtObj = gameSystems->GetObj().GetObject(tgt);
		if (!tgtObj->IsCritter())
			continue;
		if (spellSys.IsSpellHarmful(curSeq->spellPktBody.spellEnum, curSeq->spellPktBody.caster, tgt)){
			critterSys.Attack(curSeq->spellPktBody.caster, tgt, 1, 0);
		}
	}

	auto spellId = d20a->spellId  = curSeq->d20Action->spellId = curSeq->spellPktBody.spellId;
	d20Sys.d20SendSignal(d20a->d20APerformer, DK_SIG_Spell_Cast, spellId, 0);

	for (auto i = 0u; i < curSeq->spellPktBody.targetCount; i++) {
		auto &tgt = curSeq->spellPktBody.targetListHandles[i];
		if (!tgt)
			continue;
		d20Sys.d20SendSignal(tgt, DK_SIG_Spell_Cast, spellId, 0);
	}
	

	if (curSeq)
		curSeq->spellPktBody.Reset();

	return AEC_OK;
}

ActionErrorCode D20ActionCallbacks::ActionFrameAidAnotherWakeUp(D20Actn* d20a)
{
	
	// objects.floats->FloatCombatLine(d20a->d20ATarget, 204); // woken up // not necessary - already gets applied with the removal of the sleep condition I think
	d20Sys.d20SendSignal(d20a->d20ATarget, DK_SIG_AID_ANOTHER_WAKE_UP, d20a, 0);
	
	
	return AEC_OK;
}

ActionErrorCode D20ActionCallbacks::ActionFrameAoo(D20Actn* d20a)
{
	if (d20Sys.d20Query(d20a->d20APerformer, DK_QUE_Trip_AOO) && !d20Sys.d20Query(d20a->d20ATarget, DK_QUE_Prone))
	{
		return ActionFrameTripAttack(d20a);
	}

	return ActionFrameStandardAttack(d20a);
}

ActionErrorCode D20ActionCallbacks::ActionCheckAidAnotherWakeUp(D20Actn* d20a, TurnBasedStatus* tbStat){
	if (!d20a->d20ATarget)	{
		return AEC_TARGET_INVALID;
	}
	return AEC_OK;
};

ActionErrorCode D20ActionCallbacks::ActionCheckCastSpell(D20Actn* d20a, TurnBasedStatus* tbStat) {
	int spEnum, spellEnumOrg, spellClass, spellLvl, invIdx;
	MetaMagicData mmData;
	d20a->d20SpellData.Extract(&spEnum, &spellEnumOrg, &spellClass, &spellLvl, &invIdx, &mmData);
	if (spEnum <= 0 || spellSys.IsLabel(spEnum) || spEnum > SPELL_ENUM_MAX_EXPANDED)
		return AEC_INVALID_ACTION;

	SpellEntry spEntry(spEnum);

	auto actSeqSpellResetter = []() {
		if (*actSeqSys.actSeqCur) {
			spellSys.spellPacketBodyReset(&(*actSeqSys.actSeqCur)->spellPktBody);
		}
	};

	// check casting time
	if (spEntry.castingTimeType == 2 && combatSys.isCombatActive())	{
		actSeqSpellResetter();
		return AEC_OUT_OF_COMBAT_ONLY;
	}

	// if not an item spell
	if (invIdx == INV_IDX_INVALID) {
		tbStat->surplusMoveDistance = 0;
		
		// check CannotCast
		if (d20Sys.d20Query(d20a->d20APerformer, DK_QUE_CannotCast)) {
			actSeqSpellResetter();
			return AEC_CANNOT_CAST_SPELLS;
		}

		if (!spellSys.spellCanCast(d20a->d20APerformer, spellEnumOrg, spellClass, spellLvl))
			return AEC_CANNOT_CAST_SPELLS;


		// check Spell/Sorc slots
		if (!spellSys.isDomainSpell(spellClass)) {
			auto classCode = spellSys.GetCastingClass(spellClass);
			if (d20ClassSys.IsNaturalCastingClass(classCode))
				while (true) {

					auto spellsPerDay = spellSys.GetNumSpellsPerDay(d20a->d20APerformer, classCode, spellLvl);
					auto spellsCastNum = spellSys.NumSpellsInLevel(d20a->d20APerformer, obj_f_critter_spells_cast_idx, spellClass, spellLvl);

					if (spellsCastNum < spellsPerDay) {
						break;
					}

					d20a->d20SpellData.spellSlotLevel += 1;
					if (++spellLvl >= NUM_SPELL_LEVELS) {
						actSeqSpellResetter();
						return AEC_CANNOT_CAST_OUT_OF_AVAILABLE_SPELLS;
					}
				}
		}
		
		// check GP requirement
		if (spEntry.costGP > 0u && party.IsInParty(d20a->d20APerformer) 
			&& party.GetMoney() >=0 
			&& (uint32_t)party.GetMoney() < spEntry.costGP * 100) { // making sure that costGP is interpreted as unsigned in case of some crazy overflow scenario
			actSeqSpellResetter();
			return AEC_CANNOT_CAST_NOT_ENOUGH_GP;
		}
	}

	if (d20Sys.d20QueryWithData(d20a->d20APerformer, DK_QUE_IsActionInvalid_CheckAction, (uint32_t)d20a, 0)) {
		actSeqSpellResetter();
		return AEC_INVALID_ACTION;
	}

	return d20Sys.TargetCheck(d20a) != 0 ? AEC_OK : AEC_TARGET_INVALID;
}


ActionErrorCode D20ActionCallbacks::AddToSeqCharge(D20Actn* d20a, ActnSeq* actSeq, TurnBasedStatus* tbStat){
	auto tgt = d20a->d20ATarget;
	if (!tgt)
		return AEC_TARGET_INVALID;

	auto performer = d20a->d20APerformer;
	if (tbStat->tbsFlags & (TBSF_Movement | TBSF_1))
		return AEC_ALREADY_MOVED;

	D20Actn d20aCopy = *d20a;
	TurnBasedStatus tbStatCopy = *tbStat;

	auto weapon = inventory.ItemWornAt(performer, EquipSlot::WeaponPrimary);
	if (!weapon){
		weapon = inventory.ItemWornAt(performer, EquipSlot::WeaponSecondary);
		if (weapon){
			d20aCopy.d20Caf |= D20CAF_SECONDARY_WEAPON;
			tbStatCopy.attackModeCode = ATTACK_CODE_OFFHAND ;
			d20aCopy.data1 = ATTACK_CODE_OFFHAND + 1;
		} 
		else if (dispatch.DispatchD20ActionCheck(&d20aCopy, &tbStatCopy, dispTypeGetCritterNaturalAttacksNum) )
		{
			tbStatCopy.attackModeCode = ATTACK_CODE_NATURAL_ATTACK;
			d20aCopy.data1 = ATTACK_CODE_NATURAL_ATTACK + 1;
		}
		else{
			tbStatCopy.attackModeCode = ATTACK_CODE_PRIMARY;
			d20aCopy.data1 = ATTACK_CODE_PRIMARY + 1;
		}
	}
		
	return AEC_OK; //TODO complete the rest

}

ActionErrorCode D20ActionCallbacks::AddToSeqPython(D20Actn* d20a, ActnSeq* actSeq, TurnBasedStatus* tbStat){
	
	return pythonD20ActionIntegration.PyAddToSeq((D20DispatcherKey)d20a->data1, d20a, actSeq, tbStat);
	
}

ActionErrorCode D20ActionCallbacks::AddToSeqSimple(D20Actn*d20a, ActnSeq* actSeq, TurnBasedStatus* tbStat){
	return actSeqSys.AddToSeqSimple(d20a, actSeq, tbStat);
}

ActionErrorCode D20ActionCallbacks::AddToSeqWithTarget(D20Actn* d20a, ActnSeq* actSeq, TurnBasedStatus* tbStat){
	return static_cast<ActionErrorCode>(actSeqSys.AddToSeqWithTarget(d20a, actSeq, tbStat));
}

ActionErrorCode D20ActionCallbacks::AddToSeqWhirlwindAttack(D20Actn* d20a, ActnSeq* actSeq, struct TurnBasedStatus* tbStat){
	auto performer = d20a->d20APerformer;
	if (d20Sys.d20Query(performer, DK_QUE_Prone)){
		D20Actn standUp;
		standUp = *d20a;
		standUp.d20ActType = D20A_STAND_UP;
		actSeq->d20ActArray[actSeq->d20ActArrayNum++] = standUp;
	}

	actSeq->d20ActArray[actSeq->d20ActArrayNum++] = *d20a;

	

	auto reach = critterSys.GetReach(performer, D20A_UNSPECIFIED_ATTACK);
	auto perfSizeFeet = objects.GetRadius(performer)/INCH_PER_FEET;

	std::vector<objHndl> enemies;
	combatSys.GetEnemyListInRange(performer, 1.0 + perfSizeFeet + reach, enemies);
	
	for (auto i=0u; i<enemies.size(); i++){
		if (locSys.DistanceToObj(performer, enemies[i]) <= reach) {
			D20Actn stdAttack = *d20a;
			stdAttack.d20ActType = D20A_STANDARD_ATTACK;
			stdAttack.data1 = ATTACK_CODE_PRIMARY + 1;
			stdAttack.d20ATarget = enemies[i];
			stdAttack.d20Caf |= D20CAF_FREE_ACTION;
			actSeq->d20ActArray[actSeq->d20ActArrayNum++] = stdAttack;
		}
	}


	return AEC_OK;
}

ActionErrorCode D20ActionCallbacks::AddToSeqTripAttack(D20Actn* d20a, ActnSeq* actSeq, TurnBasedStatus* tbStat){

	auto tgt = d20a->d20ATarget;
	if (!tgt)
		return AEC_TARGET_INVALID;

	auto actNum = actSeq->d20ActArrayNum;

	auto tbStatCopy = *tbStat;
	auto reach = critterSys.GetReach(d20a->d20APerformer, d20a->d20ActType);
	if (locSys.DistanceToObj(d20a->d20APerformer, d20a->d20ATarget) > reach)
	{
		auto d20aCopy = *d20a;
		d20aCopy.d20ActType = D20A_UNSPECIFIED_MOVE;
		locSys.getLocAndOff(tgt, &d20aCopy.destLoc);
		auto result = static_cast<ActionErrorCode>(actSeqSys.moveSequenceParse(&d20aCopy, actSeq, tbStat, 0.0, reach, 1));
		if (!result)
		{
			auto tbStatusCopy = *tbStat;
			memcpy(&actSeq->d20ActArray[actSeq->d20ActArrayNum++], &d20aCopy, sizeof(D20Actn));
			if (actNum < actSeq->d20ActArrayNum)
			{
				for ( ; actNum < actSeq->d20ActArrayNum; actNum++)
				{
					result = static_cast<ActionErrorCode>(actSeqSys.TurnBasedStatusUpdate(&tbStatusCopy, &actSeq->d20ActArray[actNum]));
					if (result)
					{
						tbStatusCopy.errCode = result;
						return result;
					}
					auto actionCheckFunc = d20Sys.d20Defs[actSeq->d20ActArray[actNum].d20ActType].actionCheckFunc;
					if (actionCheckFunc)
					{
						result = actionCheckFunc(&actSeq->d20ActArray[actNum], &tbStatusCopy);
						if (result)
							return result;
					}
				}
				if (actNum >= actSeq->d20ActArrayNum)
					return AEC_OK;
				tbStatusCopy.errCode = result;
				if (result)
					return result;
			}
			return AEC_OK;
		}

		return result;
	}


	if (tbStat->tbsFlags & TBSF_FullAttack){
		d20a->data1 = tbStatCopy.attackModeCode + 1;
		d20a->d20Caf |= D20CAF_FULL_ATTACK;
	}

	memcpy(&actSeq->d20ActArray[actSeq->d20ActArrayNum++], d20a, sizeof(D20Actn));
	return AEC_OK;
	//return AddToSeqWithTarget(d20a, actSeq, tbStat);
}

ActionErrorCode D20ActionCallbacks::StdAttackTurnBasedStatusCheck(D20Actn* d20a, TurnBasedStatus* tbStat){
	return static_cast<ActionErrorCode>(actSeqSys.StdAttackTurnBasedStatusCheck(d20a, tbStat));
}

ActionErrorCode D20ActionCallbacks::TurnBasedStatusCheckPython(D20Actn* d20a, TurnBasedStatus* tbStat){
	return AEC_OK;
}

ActionErrorCode D20ActionCallbacks::ActionCostPython(D20Actn* d20a, TurnBasedStatus* tbStat, ActionCostPacket* acp) {

	return (ActionErrorCode) d20Sys.GetPyActionCost(d20a, tbStat, acp);
}

ActionErrorCode D20ActionCallbacks::ActionCostStandardAttack(D20Actn* d20a, TurnBasedStatus* tbStat, ActionCostPacket* acp){

	if ( d20Sys.d20Query(d20a->d20APerformer, DK_QUE_HoldingCharge)
		 && (tbStat->tbsFlags & TBSF_TouchAttack)	&& !(d20a->d20Caf & D20CAF_FREE_ACTION)){
		acp->hourglassCost = 0;
		return AEC_OK;
	}

	acp->hourglassCost = 0;
	acp->chargeAfterPicker = 0;
	acp->moveDistCost = 0;

	if (!(d20a->d20Caf & D20CAF_FREE_ACTION) && combatSys.isCombatActive())
	{
		acp->chargeAfterPicker = 1;
		if ( (!feats.HasFeatCountByClass(d20a->d20APerformer, FEAT_SHOT_ON_THE_RUN)
			   || d20a->d20ActType != D20A_STANDARD_RANGED_ATTACK)
		&&   (!feats.HasFeatCountByClass(d20a->d20APerformer, FEAT_SPRING_ATTACK)
				|| d20a->d20ActType != D20A_STANDARD_ATTACK ))
		{
			tbStat->surplusMoveDistance = 0;
		}
	}
	return AEC_OK;
}


ActionErrorCode D20ActionCallbacks::ActionCostMoveAction(D20Actn *d20, TurnBasedStatus *tbStat, ActionCostPacket *acp)
{
	acp->hourglassCost = 0;
	acp->chargeAfterPicker = 0;
	acp->moveDistCost = 0;
	if (!(d20->d20Caf & D20CAF_FREE_ACTION) && combatSys.isCombatActive())
	{
		acp->hourglassCost = 1;
		tbStat->surplusMoveDistance = 0;
		tbStat->numAttacks = 0;
		tbStat->baseAttackNumCode = 0;
		tbStat->attackModeCode = 0;
		tbStat->numBonusAttacks = 0;
	}
	return AEC_OK;
}

ActionErrorCode D20ActionCallbacks::ActionCostNull(D20Actn* d20a, TurnBasedStatus* tbStat, ActionCostPacket* acp){
	acp->hourglassCost = 0;
	acp->chargeAfterPicker = 0;
	acp->moveDistCost = 0;
	return AEC_OK;
}

ActionErrorCode D20ActionCallbacks::ActionCostStandardAction(D20Actn*, TurnBasedStatus*, ActionCostPacket*acp){
	acp->hourglassCost = 2;
	acp->chargeAfterPicker = 0;
	acp->moveDistCost = 0;
	return AEC_OK;
};

ActionErrorCode D20ActionCallbacks::ActionCostWhirlwindAttack(D20Actn* d20a, TurnBasedStatus* tbStat, ActionCostPacket*acp) {
	acp->chargeAfterPicker = 0;
	acp->moveDistCost = 0;
	acp->hourglassCost = 4;
	if ( ( d20a->d20Caf & D20CAF_FREE_ACTION) || !combatSys.isCombatActive()){
		acp->hourglassCost = 0;
	}

	if (tbStat->attackModeCode >= tbStat->baseAttackNumCode && tbStat->hourglassState >= 4 && !tbStat->numBonusAttacks){

		auto performer = d20a->d20APerformer;
		auto reach = critterSys.GetReach(performer, D20A_UNSPECIFIED_ATTACK);
		auto perfSizeFeet = objects.GetRadius(performer) / INCH_PER_FEET;

		std::vector<objHndl> enemies;
		combatSys.GetEnemyListInRange(performer, 1.0 + perfSizeFeet + reach, enemies);
		auto numEnemies = 0;
		for (auto i=0; i<enemies.size(); i++){
			if (locSys.DistanceToObj(performer, enemies[i] ) < reach)
				numEnemies++;
		}
		tbStat->numBonusAttacks = 0;
		tbStat->baseAttackNumCode = 0;
		tbStat->numAttacks = numEnemies;
		tbStat->attackModeCode = ATTACK_CODE_PRIMARY;
		tbStat->surplusMoveDistance = 0;
	}

	return AEC_OK;
}

