
/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#include "ScriptMgr.h"
#include "Player.h"
#include "Config.h"
#include "Chat.h"
#include "Creature.h"
#include "ScriptedAI/ScriptedCreature.h"
#include "MapInstanced.h"



enum Misc
{
    ID_KEY          	= 90000,
    ENTRY_ENDBOSS   	= 510005,
	CREATURE_AURA_ID	= 600001,
	PLAYER_AURA_ID		= 600002,
};



class TimedDungeonStart : public CreatureScript
{
public:
    TimedDungeonStart() : CreatureScript("TimedDungeonStart") { }
    bool TimedDungeonEnable = sConfigMgr->GetOption<bool>("TimedDungeon.Enable", true);



    bool OnGossipSelectCode(Player* player, Creature* creature, uint32 sender, uint32 action, const char* code) override
    {
        if (TimedDungeonEnable) {
            //start run depending of input
            if (isNumeric(code)) {
                std::stringstream ss(code);
                int num;
                ss >> num;
                if (num > 0) {
                    if (num <= player->GetItemCount(ID_KEY)) {
                        auto tstart = std::chrono::system_clock::now();
                        std::ostringstream ss;
                        ss << "Starting run for leader " << player->GetName() << ". Difficulty:" << num << ". tstart: " << tstart;
                        
                        Group* myGroup = player->GetGroup();
                        if (myGroup) {
                            //party run
                            Map* mymap = player->GetMap();
                            Map::PlayerList pList = mymap->GetPlayers();
                            for (auto i = pList.begin(); i != pList.end(); ++i)
                            {
                                Player* myPlayer = i->GetSource();
                                ChatHandler(myPlayer->GetSession()).SendSysMessage(ss.str().c_str());
                            }
                            Group::MemberSlotList const& groupMembers = myGroup->GetMemberSlots();
                            std::string pNames[5] = { "", "", "", "", "" };
                            auto counter = 0;
                            for (auto i = groupMembers.begin(); i != groupMembers.end(); i++)
                            {
                                pNames[counter] = i->name;
                                counter++;
                            }
                            std::ostringstream ssa;
                            ssa << tstart;
                            WorldDatabase.Query("INSERT INTO TimedDungeon_runs(player1, player2, player3, player4, player5, dungeon, tstart, InstanceID, level, deaths) VALUES('{}', '{}', '{}', '{}', '{}', '{}', '{}', '{}', '{}', 0)", pNames[0], pNames[1], pNames[2], pNames[3], pNames[4], player->GetMap()->GetMapName(), ssa.str().c_str(), player->GetInstanceId(), num);
                        }
                        else {
                            ChatHandler(player->GetSession()).SendSysMessage(ss.str().c_str());
                            //solo run
                            std::ostringstream ssa;
                            ssa << tstart;
                            WorldDatabase.Query("INSERT INTO TimedDungeon_runs(player1, dungeon, tstart, InstanceID, level, deaths) VALUES('{}', '{}', '{}', '{}', '{}', 0)", player->GetName(), player->GetMap()->GetMapName(), ssa.str().c_str(), player->GetInstanceId(), num);
                        }
                        //start run
                    }
                    else {
                        ChatHandler(player->GetSession()).SendSysMessage("Your key is not high enough!");
                    }
                }
                else {
                    ChatHandler(player->GetSession()).SendSysMessage("Please insert a valid number!");
                }
            }
            else {
                ChatHandler(player->GetSession()).SendSysMessage("Please insert a valid number!");
            }
        }
        return true;
    }
    
    bool isNumeric(const char* str) {
        for (int i = 0; str[i] != '\0'; i++) {
            if (!isdigit(str[i])) {
                return false;
            }
        }
        return true;
    }
	
	void Init(){
	}
	
	void Reset(){
	}
	
	void CalculateScore(){
	}
	

};

class TimedDungeonAllCreature : public AllCreatureScript
{
public: TimedDungeonAllCreature() : AllCreatureScript("TimedDungeonAllCreature") { }
    bool TimedDungeonEnable = sConfigMgr->GetOption<bool>("TimedDungeon.Enable", true);

    void OnAllCreatureUpdate(Creature* creature, uint32 /*diff*/) {
        if (TimedDungeonEnable) {
			//solo dungeon map id
            if (creature->GetMapId() == 189) {
                uint32 instanceID = creature->GetInstanceId();
                if (!creature->HasAura(CREATURE_AURA_ID)) {
                    
                    QueryResult qr = WorldDatabase.Query("SELECT level FROM TimedDungeon_runs WHERE InstanceID = '{}'", instanceID);
                    if (qr) {
                        creature->SetAuraStack(CREATURE_AURA_ID, creature, (*qr)[0].Get<uint32>());
                    }
                }

            }
        }
    }
};


class TimedDungeonPlayer : public PlayerScript
{
public:
    TimedDungeonPlayer() : PlayerScript("TimedDungeonPlayer") { }

    bool TimedDungeonEnable = sConfigMgr->GetOption<bool>("TimedDungeon.Enable", true);

    void OnLogin(Player* player) override
    {
        if (TimedDungeonEnable)
        {
            ChatHandler(player->GetSession()).SendSysMessage("This Server is running the TimedDungeon module.");
        }
    }

    void OnCreatureKill(Player* killer, Creature* killed)
    {
        if (TimedDungeonEnable && killer->HasAura(PLAYER_AURA_ID)) {
            // add score; update counter; check for last kill
            if (killed->GetEntry() == ENTRY_ENDBOSS) {
                Map* mymap = killer->GetMap();
                Map::PlayerList pList = mymap->GetPlayers();

                for (auto i = pList.begin(); i != pList.end(); ++i)
                {
                    Player* myPlayer = i->GetSource();
                    ChatHandler(myPlayer->GetSession()).SendSysMessage("You have finished your run!");
					WorldDatabase.Query("SELECT level FROM TimedDungeon_runs WHERE InstanceID = '{}'", instanceID);
                }
            }
        }
        
    }

    void OnCreatureKilledByPet(Player* PetOwner, Creature* killed)
    {
        if (TimedDungeonEnable) {
            if (killed->GetEntry() == ENTRY_ENDBOSS) {
                Map* mymap = PetOwner->GetMap();

                Map::PlayerList pList = mymap->GetPlayers();
                for (auto i = pList.begin(); i != pList.end(); ++i)
                {
                    Player* myPlayer = i->GetSource();
                    ChatHandler(myPlayer->GetSession()).SendSysMessage("This Server is running the TimedDungeon module.");
                }
            }
        }
    }

    void OnPlayerKilledByCreature(Creature* killer, Player* killed)
    {
        // time loss; revive at start after 5 seconds
		WorldDatabase.Query("UPDATE TimedDungeon_runs SET deaths = deaths + 1 WHERE InstanceID = '{}'", instanceID);
    }

    void OnChat(Player* player, uint32 type, uint32 lang, std::string& msg)
    {
        // check current run; info commands
    }

    void OnMapChanged(Player* player)
    {
        //end run
        if (player->GetMap()->IsHeroic()) {
            ChatHandler(player->GetSession()).SendSysMessage("detected hc");
        }
        else {
            ChatHandler(player->GetSession()).SendSysMessage("no hc");
        }
    }
};


// Add all scripts in one
void AddTimedDungeonScripts()
{
    new TimedDungeonStart();
    new TimedDungeonAllCreature();
    new TimedDungeonPlayer();
}
