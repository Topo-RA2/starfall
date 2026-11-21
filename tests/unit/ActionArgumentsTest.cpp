#include "../../src/Starfall.cpp"
#include <cstdio>

int parseArguments(lua_State* L) {
    ScriptFields fields{{"action","givecredits"},{"action.target","self"}};
    std::optional<DWORD> target;std::optional<CellStruct> point;
    bool fromSection=lua_toboolean(L,2)!=0;
    ScriptActions::arguments(L,1,fields,target,point,fromSection);
    lua_pushstring(L,fields.contains("action.amount")?fields.at("action.amount").c_str():fields.at("action.target").c_str());
    return 1;
}
int main() {
    Skill step{};step.id="review";step.trigger="onsuperweapon";
    ScriptFields valid{{"action","givecredits"},{"action.amount","$(100+50)"},{"action.target","self"}};
    auto typo=valid;typo["action.amunt"]="999";
    if(!readAction(step,"review",&valid) || readAction(step,"review",&typo)) return 5;
    ScriptFields research{{"action","grantrandomresearch"}};
    if(!readAction(step,"review",&research) || step.actions.back().researchRarity) return 9;
    for(auto [name,expected]:{std::pair{"Common",Research::Rarity::Common},{"Rare",Research::Rarity::Rare},
        {"Epic",Research::Rarity::Epic},{"Insane",Research::Rarity::Insane}}) {
        research["action.rarity"]=name;
        if(!readAction(step,"review",&research) || step.actions.back().researchRarity!=expected) return 10;
    }
    for(auto rarity:{"","Mythic","Common,Rare"}) {
        research["action.rarity"]=rarity;
        if(readAction(step,"review",&research)) return 11;
    }
    research["action.rarity"]="All";research["action.amount"]="2";
    if(readAction(step,"review",&research)) return 12;
    research.erase("action.amount");research["action.target"]="Owner";
    if(!ActionSequence::prepare(research) || !readAction(step,"review",&research) || step.actions.back().researchRarity) return 13;
    valid["action.rarity"]="Rare";
    if(readAction(step,"review",&valid)) return 14;
    ScriptFields motion{{"action","forcemove"},{"action.forcemove.mode","launch"},{"action.forcemove.duration","20"}};
    if(!readAction(step,"review",&motion) || step.actions.back().forceMove.airborneHeight!=256) return 6;
    for(auto [value,expected]:{std::pair{"0",0},{"0.5",128},{"2",512},{"512",131072}}) {
        motion["action.forcemove.airborneheight"]=value;
        if(!readAction(step,"review",&motion) || step.actions.back().forceMove.airborneHeight!=expected) return 7;
    }
    for(auto value:{"-1","nan","inf","512.1","1x",""}) {
        motion["action.forcemove.airborneheight"]=value;
        if(readAction(step,"review",&motion)) return 8;
    }
    int checked=0;
    for(int state=0;state<128;++state) {
        auto L=luaL_newstate();if(!L)return 1;
        for(auto expression:{"{Amount=1,amount=2}","{Amount=1,['Action.Amount']=2}",
            "{Offset='1,2',['Action.ForceMove.Offset']='3,4'}","{Duration=1,['Action.ForceMove.Duration']=2}",
            "{Target='Self',target='Victim'}","{AirborneHeight=1,['Action.ForceMove.AirborneHeight']=2}"}) {
            lua_pushcfunction(L,parseArguments);
            string code="return "+string(expression);
            if(luaL_loadstring(L,code.c_str()) || lua_pcall(L,0,1,0)) return 2;
            lua_pushboolean(L,0);
            if(lua_pcall(L,2,1,0)==LUA_OK || !strstr(lua_tostring(L,-1),"duplicate action field")) return 3;
            lua_pop(L,1);++checked;
        }
        for(bool fromSection:{false,true}) {
            lua_pushcfunction(L,parseArguments);
            lua_newtable(L);
            lua_pushstring(L,fromSection?"Victim":"7");lua_setfield(L,-2,fromSection?"Target":"Amount");
            lua_pushboolean(L,fromSection);
            if(lua_pcall(L,2,1,0)!=LUA_OK || string(lua_tostring(L,-1))!=(fromSection?"Victim":"7")) return 4;
            lua_pop(L,1);++checked;
        }
        lua_close(L);
    }
    std::printf("PASS %d action argument checks across 128 Lua states\n",checked);
    return 0;
}
