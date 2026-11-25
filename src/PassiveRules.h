#pragma once
#include "PassiveTypes.h"

namespace PassiveRules {
extern char configPath[MAX_PATH];
extern std::map<string,string> rulesSkillNames;
extern const std::pair<const string,ScriptFields>* parsingScript;

const char* rulesName(const string& section);
string readRaw(const string& section,const char* key,const char* fallback="");
string read(const string& section,const char* key,const char* fallback="");
std::set<string> sectionKeys(const string& section);
bool invalidField(const string& id,const string& field);
bool mountOffset(const string& text,CoordStruct& result);
bool actionField(const string& key);
bool activeField(const string& key);
bool positionalAction(std::string_view name);
inline bool stopsSequenceOnFailure(std::string_view name) {
    return name=="consumepassenger" || name=="consumetarget" || name=="removeattachedunit";
}
bool parseState(Skill& skill,const std::set<string>& keys);
bool readAction(Skill& skill,const string& section,const ScriptFields* fields=nullptr);
bool readActions(Skill& skill,const std::set<string>& keys);
bool readAnimations(Skill& skill,const std::set<string>& keys);
bool beforeHealthLoss(const string& trigger);
bool damageEvent(const string& trigger);
bool validStateMultiplier(size_t index,double value);
bool passengerCombatMultiplier(size_t index);

// Read-only lookups into the scenario registry while validating action references.
bool hasPickupType(const string& id);
const vector<string>* scriptFunctions(const string& module);
}
