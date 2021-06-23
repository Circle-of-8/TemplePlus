import tpdp
import roll_history
import logbook

debug_enabled = True

def debug_print(*args):
    if debug_enabled:
        for arg in args:
            print arg,
    return

def playSoundEffect(weaponUsed, attacker, tgt, sound_number):
    missing_stub("auto soundId = inventory.GetSoundIdForItemEvent(weaponUsed, attacker, tgt, 6); sound tbd")
    missing_stub("sound.PlaySoundAtObj(soundId, attacker); sound tbd")
    return

def deal_attack_damage(attacker, tgt, d20_data, flags, action_type): 
    # return value is used by Coup De Grace

    #Provoke Hostility
    missing_stub("aiSys.ProvokeHostility(attacker, tgt, 1, 0);")

    #Check if target is alive
    missing_stub("if critterSys.IsDeadNullDestroyed(tgt), is it enough to just query if dead?")
    if tgt.d20_query(Q_Dead):
        return -1

    #Create evt_obj_dam
    evt_obj_dam = tpdp.EventObjDamage()
    evt_obj_dam.attack_packet.action_type = action_type
    evt_obj_dam.attack_packet.attacker = attacker
    evt_obj_dam.attack_packet.target = tgt
    evt_obj_dam.attack_packet.event_key = d20_data
    evt_obj_dam.attack_packet.set_flags(flags)

    #Set weapon used
    unarmed = OBJ_HANDLE_NULL
    if evt_obj_dam.attack_packet.get_flags() & D20CAF_TOUCH_ATTACK:
        evt_obj_dam.attack_packet.set_weapon_used(unarmed)
    elif evt_obj_dam.attack_packet.get_flags() & D20CAF_SECONDARY_WEAPON:
        offhandItem = attacker.item_worn_at(item_wear_weapon_secondary)
        if offhandItem.type != obj_t_weapon:
            evt_obj_dam.attack_packet.set_weapon_used(unarmed)
        else:
            evt_obj_dam.attack_packet.set_weapon_used(offhandItem)
    else:
        mainhandItem = attacker.item_worn_at(item_wear_weapon_primary)
        if mainhandItem.type != obj_t_weapon:
            evt_obj_dam.attack_packet.set_weapon_used(unarmed)
        else:
            evt_obj_dam.attack_packet.set_weapon_used(mainhandItem)

    #Check ammo
    evt_obj_dam.attack_packet.ammo_item = attacker.get_ammo_used()

    #Check Concealment Miss
    #Remark: I can't remember that D20CAF_CONCEALMENT_MISS was actually set
    #in to_hit_processing
    if flags & D20CAF_CONCEALMENT_MISS:
        missing_stub("histSys.CreateRollHistoryLineFromMesfile(11, attacker, tgt); to what mesfile this is referring to?")
        attacker.float_mesfile_line('mes\\combat.mes', 45) # Miss (Concealment)
        playSoundEffect(evt_obj_dam.attack_packet.get_weapon_used(), attacker, tgt, 6)
        missing_stub("attacker.d20_send_signal(S_Attack_Made, int(&evt_obj_dam), 0)")
        return -1

    #Check normal Miss
    if not flags & D20CAF_HIT:
        attacker.float_mesfile_line('mes\\combat.mes', 29) # Miss
        missing_stub("attacker.d20_send_signal(S_Attack_Made, int(&evt_obj_dam), 0)")
        playSoundEffect(evt_obj_dam.attack_packet.get_weapon_used(), attacker, tgt, 6)
        #Check if arrow was deflected
        if flags & D20CAF_DEFLECT_ARROWS:
            tgt.float_mesfile_line('mes\\combat.mes', 5052) #{5052}{Deflect Arrows}
            missing_stub("histSys.CreateRollHistoryLineFromMesfile(12, attacker, tgt)")
        #dodge animation
        if tgt.is_unconscious() or tgt.d20_query(Q_Prone):
            return -1
        else:
            missing_stub("gameSystems->GetAnim().PushDodge(attacker, tgt); #dumb question: where can I lookup those anim_goals?")
        return -1

    #Check Friendly Fire
    #if (tgt && attacker && critterSys.NpcAllegianceShared(tgt, attacker) && combatSys.AffiliationSame(tgt, attacker))
    #if attacker != tgt and attacker.is_friendly(tgt):
    if attacker.allegiance_shared(tgt):
        tgt.float_mesfile_line('mes\\combat.mes', 107) # Friendly Fire

    #Check if is already unconscious
    wasAlreadyUnconscious = tgt.is_unconscious()

    #dispatch damage
    evt_obj_dam.dispatch(attacker, ET_OnDealingDamage, EK_NONE)

    #handle critical hit
    if evt_obj_dam.attack_packet.get_flags() & D20CAF_CRITICAL:
        #create evt_obj_crit_dice
        evt_obj_crit_dice = tpdp.EventObjAttack()
        evt_obj_crit_dice.attack_packet.action_type = action_type
        evt_obj_crit_dice.attack_packet.attacker = attacker
        evt_obj_crit_dice.attack_packet.target = tgt
        evt_obj_crit_dice.attack_packet.event_key = d20_data
        evt_obj_crit_dice.attack_packet.set_flags(flags)
        evt_obj_crit_dice.attack_packet.set_weapon_used(evt_obj_dam.attack_packet.get_weapon_used())
        #Check ammo
        evt_obj_crit_dice.attack_packet.ammo_item = attacker.get_ammo_used()
        #apply extra damage
        extraDamageDice = evt_obj_crit_dice.dispatch(attacker, OBJ_HANDLE_NULL, ET_OnGetCriticalHitExtraDice, EK_NONE)
        debug_print("Debug extraDamageDice: {}".format(extraDamageDice))
        missing_stub("auto critMultiplierApply = temple::GetRef<BOOL(__cdecl)(DamagePacket&, int, int)>(0x100E1640); // damagepacket, multiplier, damage.mes line")
        missing_stub("critMultiplierApply(evtObjDam.damage, extraHitDice + 1, 102);")
        attacker.float_mesfile_line('mes\\combat.mes', 12) #{12}{Critical Hit!}
        #play crit hit sound
        soundIdTarget = tgt.soundmap_critter(0)
        game.sound_local_obj(tgt, soundIdTarget)
        playSoundEffect(evt_obj_crit_dice.attack_packet.get_weapon_used(), attacker, tgt, 7)
        #Logbook increase crit count
        logbook.inc_criticals(attacker)
    else:
        #if no crit only play sound
        playSoundEffect(evt_obj_crit_dice.attack_packet.get_weapon_used(), attacker, tgt, 5)

    #Logbook
    missing_stub("temple::GetRef<int>(0x10BCA8AC) = 1; // physical damage Flag used for logbook recording")

    #Call damage_critter
    damage_critter(attacker, tgt, evt_obj_dam)

    #Play damage particle effects
    missing_stub("pfx")
    
    #Send signals
    missing_stub("attacker.d20_send_signal(S_Attack_Made, int(&evt_obj_dam), 0)")
    if not wasAlreadyUnconscious and tgt.is_unconscious():
        missing_stub("attacker.d20_send_signal(S_Dropped_Enemy, int(&evt_obj_dam), 0)")

    debug_print("Debug: print overall damage: {}".format(evt_obj_dam.damage_packet.get_overall_damage()))
    return evt_obj_dam.damage_packet.get_overall_damage()

def missing_stub(msg):
    print msg
    return 0

def checkAnimationSkip(tgt):
    if tgt.is_unconscious() or tgt.d20_query(Q_Prone):
        return True
    return False

def float_hp_damage(attacker, tgt, damTot, subdualDamTot):

    leaderOfAttacker = attacker.leader_get()
    if leaderOfAttacker in game.party:
        floatColor = tf_yellow
    elif attacker.type == obj_t_pc:
        floatColor = tf_white
    else:
        floatColor = tf_red

    if damTot != 0 and subdualDamTot != 0:
        combatMesLine = game.get_mesline("mes/combat.mes", 1) # HP
        tgt.float_text_line("{} {}".format(damTot, combatMesLine), floatColor)
        combatMesLine = game.get_mesline("mes/combat.mes", 25) # Nonlethal
        tgt.float_text_line("{} {}".format(subdualDamTot, combatMesLine), floatColor)
    elif subdualDamTot !=0: # damTot ==0
        combatMesLine = game.get_mesline("mes/combat.mes", 25) # Nonlethal
        tgt.float_text_line("{} {}".format(subdualDamTot, combatMesLine), floatColor)
    #if attack deals 0 damage to due damage reduction/resistance
    else: # subdualDamTot ==0; damTot can be 0 or not
        combatMesLine = game.get_mesline("mes/combat.mes", 1) # HP
        tgt.float_text_line("{} {}".format(damTot, combatMesLine), floatColor)
    return

def damage_critter(attacker, tgt, evt_obj_dam):
    if not tgt:
        return

    #skipHitAnim
    skipHitAnim = checkAnimationSkip(tgt)

    #Check Invulnerability
    tgtFlags = tgt.object_flags_get()
    if tgtFlags & OF_INVULNERABLE:
        evt_obj_dam.damage_packet.add_mod_factor(0.0, D20DT_UNSPECIFIED, 104)

    #Dispatch Damage
    evt_obj_dam.damage_packet.calc_final_damage()
    evt_obj_dam.dispatch(tgt, ET_OnTakingDamage, EK_NONE)
    if evt_obj_dam.attack_packet.get_flags() & D20CAF_TRAP:
        attacker = OBJ_HANDLE_NULL
    elif attacker != OBJ_HANDLE_NULL:
        evt_obj_dam.dispatch(attacker, ET_OnDealingDamage2, EK_NONE)
    evt_obj_dam.dispatch(tgt, ET_OnTakingDamage2, EK_NONE)

    #Set last hit by
    if attacker != OBJ_HANDLE_NULL:
        tgt.obj_set_obj(obj_f_last_hit_by, attacker)

    #Assign damage
    damTot = max(evt_obj_dam.damage_packet.get_overall_damage(), 0)
    hpDam = tgt.obj_get_int(obj_f_hp_damage)
    hpDam += damTot
    tgt.set_hp_damage(hpDam)

    #History Roll Window
    history_roll_id = roll_history.add_damage_roll(attacker, tgt, evt_obj_dam.damage_packet)
    game.create_history_from_id(history_roll_id)

    #Send Signal HP_Changed
    tgt.d20_send_signal(S_HP_Changed, -damTot, -1 if damTot > 0 else 0)

    #Triggers for dealing damage
    if damTot:
        #Add Damaged condition
        tgt.condition_add("Damaged", damTot)
        if attacker and tgt:
            #Log Highest Damage Record
            isWeaponDamage = logbook.is_weapon_damage
            logbook.record_highest_damage(isWeaponDamage, damTot, attacker, tgt)
            #Check for friendly fire
            if attacker != tgt and attacker.is_friendly(tgt):
                tgt.sound_play_friendly_fire(attacker)

    #Subdual Damage
    subdualDamTot = evt_obj_dam.damage_packet.get_overall_damage_by_type(D20DT_SUBDUAL)
    if subdualDamTot > 0 and tgt != OBJ_HANDLE_NULL:
        tgt.condition_add("Damaged", subdualDamTot)
    subdual_dam = tgt.obj_get_int(obj_f_critter_subdual_damage)
    tgt.set_subdual_damage(subdual_dam + subdualDamTot)
    tgt.d20_send_signal(S_HP_Changed, subdualDamTot, -1 if subdualDamTot > 0 else 0)

    #Create Floatline
    float_hp_damage(attacker, tgt, damTot, subdualDamTot)

    #Push hit Animation
    if attacker:
        if not skipHitAnim:
            tgt.anim_goal_push_hit_by_weapon(attacker)
    return