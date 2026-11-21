// Exercise the real extension save stream, including rejection before commit.
void checkResearchRoundSave(int house) {
    IStream* stream=nullptr;
    check(SUCCEEDED(CreateStreamOnHGlobal(nullptr,TRUE,&stream)) && saveState(stream),"research round memory save");
    STATSTG stat{};stream->Stat(&stat,STATFLAG_NONAME);
    vector<BYTE> saved(stat.cbSize.LowPart);
    stream->Seek({},STREAM_SEEK_SET,nullptr);stream->Read(saved.data(),stat.cbSize.LowPart,nullptr);stream->Release();
    auto load=[](const vector<BYTE>& data) {
        IStream* input=nullptr;CreateStreamOnHGlobal(nullptr,TRUE,&input);
        input->Write(data.data(),static_cast<ULONG>(data.size()),nullptr);input->Seek({},STREAM_SEEK_SET,nullptr);
        const bool ok=loadState(input);input->Release();return ok;
    };
    auto word=[](vector<BYTE>& bytes,size_t offset,DWORD value) {memcpy(bytes.data()+offset,&value,4);};
    const auto rounds=Research::completedRounds;
    const auto owned=Research::unlocked;
    const auto points=Research::points;
    const auto previousSeed=Research::raritySequenceSeed;
    const auto sequenceSeed=previousSeed.value_or(Research::Detail::matchSeed());
    check(rounds.size()==1 && rounds.at(house)==4,"four ordinary purchases recorded");
    auto rejected=[&](const vector<BYTE>& bad) {
        return !load(bad) && Research::completedRounds==rounds && Research::unlocked==owned && Research::points==points &&
            Research::raritySequenceSeed==previousSeed;
    };
    auto bad=saved;word(bad,bad.size()-4,5);
    check(rejected(bad),"excess research rounds rejected atomically");
    bad=saved;word(bad,bad.size()-8,128);
    check(rejected(bad),"invalid research house rejected atomically");
    bad=saved;word(bad,bad.size()-12,129);
    check(rejected(bad),"excess research houses rejected atomically");
    bad=saved;bad.pop_back();word(bad,4,static_cast<DWORD>(bad.size()-8));
    check(rejected(bad),"truncated research rounds rejected atomically");
    auto old=saved;old.resize(old.size()-16);word(old,0,pickupSaveTag);word(old,4,static_cast<DWORD>(old.size()-8));
    Research::completedRounds.clear();
    check(load(old) && Research::completedRounds==rounds,"legacy save resumes after owned projects");
    Research::completedRounds.clear();
    const auto launchSeed=Game::Seed;Game::Seed^=0x12345678;
    check(load(saved) && Research::completedRounds==rounds && Research::raritySequenceSeed==sequenceSeed,"research rounds restored from stream");
    Game::Seed=launchSeed;
    Research::ensureReady();
}
