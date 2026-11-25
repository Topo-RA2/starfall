bool appendTooltipNumber(std::wstring& line,double value,int decimals) {
    char digits[320]; // Full fixed representation of a finite double, plus six decimals.
    auto [end,error]=std::to_chars(digits,digits+sizeof(digits),value,std::chars_format::fixed,decimals<0?2:decimals);
    if(error!=std::errc{}) return false;
    if(decimals<0) {
        while(end>digits && end[-1]=='0') --end;
        if(end>digits && end[-1]=='.') --end;
    }
    const char* first=digits;
    if(*first=='-' && std::none_of(first+1,static_cast<const char*>(end),[](char c){return c>='1' && c<='9';})) ++first;
    while(first!=end) line.push_back(*first++);
    return true;
}

const wchar_t* unitHoverText(TechnoClass* unit,const wchar_t* nativeName) {
    if(!unit || !nativeName || !*nativeName || activeTooltips.empty()) return nativeName;
    auto it=activeTooltips.find(unit->UniqueID);
    if(it==activeTooltips.end() || !alive(unit) || unit->InLimbo || !unit->IsOnMap) return nativeName;
    if(unit->Disguised && !HouseClass::IsCurrentPlayerObserver() &&
       (!HouseClass::CurrentPlayer || !unit->IsClearlyVisibleTo(HouseClass::CurrentPlayer))) return nativeName;

    // ToolTipManager::Process copies the returned text into this fixed native buffer.
    constexpr size_t limit=std::extent_v<decltype(ToolTipManagerData::HelpText)>-1;
    static std::wstring text;
    text=nativeName;
    const auto baseLength=text.size();
    const auto hp=UnitHealth::snapshot(unit);
    for(auto skill:it->second) {
        if(text.size()+1>=limit) break;
        std::wstring line;
        for(const auto& part:skill->tooltipText) {
            line+=part.literal;
            if(part.number.expression.empty()) continue;
            auto value=stateFormulaValue(part.number,unit,unit->Target,hp,skill->id,true);
            if(!value || !appendTooltipNumber(line,*value,skill->tooltipDecimals)) {line.clear();break;}
        }
        auto count=std::min(line.size(),limit-text.size()-1);
        if(count && line[count-1]>=0xD800 && line[count-1]<=0xDBFF) --count;
        if(count) {text+=L'\n';text.append(line,0,count);}
    }
    return text.size()==baseLength?nativeName:text.c_str();
}
