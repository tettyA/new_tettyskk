#include "pch.h"
#include "qelf_translater.h"
#include "globals.h"
#include <locale>
#include <fstream>
#include <sstream>
#include <filesystem>

namespace {
    void split(std::wstring string, char separator, std::vector<std::wstring>& list) {
        int separator_length = 1;

        list = std::vector<std::wstring>();

        auto offset = std::wstring::size_type(0);
        while (1) {
            auto pos = string.find(separator, offset);
            if (pos == std::wstring::npos) {
                list.push_back(string.substr(offset));
                break;
            }
            list.push_back(string.substr(offset, pos - offset));
            offset = pos + separator_length;
        }
    }
}

qelf_translater::qelf_translater(){
	m_KMP = KMP();
    DSendCnt = 0;
    m_KMP.is_key_pushed = { false,false };
    m_KMP.is_shift_pressed = false;
	Send1st = DsendElements{ KMP::Dir::None, {0}, 0 };
}

qelf_translater::~qelf_translater()
{}

bool qelf_translater::translate_qwerty_to_qelf(const KeyCode& key, std::wstring& output)
{
	// 変換表に存在するか確認
    if (!m_KMP.KMPs.contains(key)) {
		DebugTrace((L"key not found:[" + std::to_wstring(key) + L"]"));
        return false;
    }

    output.clear();
	KMP::KMPelm selected = m_KMP.KMPs[key];

    if (_IsShiftKeyPressed()) {
        m_KMP.is_shift_pressed = true;
    }

    if (selected.cmd == KMP::KMPelm::Cmd::Send) {
        switch (selected.to_samedir)
        {
        case ex_smalltu:
			m_KMP.is_key_pushed.sokuon = true;
            return false;
        case ex_nn:
            output = L"NN";
            return true;
        case ex_youon:
            m_KMP.is_key_pushed.youon = true;
            return false;
        case ex_boin:
            return false;
        case ex_sakujo:
            if (DSendCnt > 0) {
                DSendCnt = 0;
                reset_kmp_state();
            }
            else {
                reset_kmp_state();
                INPUT input = { 0 };
                input.type = INPUT_KEYBOARD;
                input.ki.wVk = VK_BACK;
                SendInput(1, &input, sizeof(INPUT));
            }
            return false;
        default:
            output = std::wstring(1, (WCHAR)selected.to_samedir);
            return true;
        }
    }
    else if (selected.cmd == KMP::KMPelm::Cmd::DSend) {
        if (DSendCnt == 0) {
            DSendCnt++;
            
            // 一回目の情報を保存
            Send1st = DsendElements{ selected.dir,selected.to_samedir,selected.to_otherdir };
		
        }
        else if (DSendCnt >= 1) {
            DsendElements    Send2nd = DsendElements{ selected.dir,selected.to_samedir,selected.to_otherdir };
		
            if (Send1st.dir1.dirkc != ex_boin) {
                // 子音の処理 
                if (Send1st.dir == KMP::Dir::None || Send2nd.dir == KMP::Dir::None) {
                    output = std::wstring(1, WCHAR(Send1st.dir1.dirkc));
                }
                // 濁音
                else if (Send1st.dir == Send2nd.dir) {
                    output = std::wstring(1, WCHAR(Send1st.dir1.samedirkc));
                }
                // 清音
                else {
                    output = std::wstring(1, WCHAR(Send1st.otherdirkc));
                }
            }
            
            if(m_KMP.is_key_pushed.youon) {
                output += L"Y";
            }

            // 母音の処理
			output += std::wstring(1, WCHAR(selected.to_secboin));
			// 促音の処理
            if (m_KMP.is_key_pushed.sokuon) {
                output += WCHAR_XTU;
            }
			DebugTrace(L"output: " + output);
            DSendCnt = 0;
            m_KMP.is_key_pushed.sokuon = false;
            m_KMP.is_key_pushed.youon = false;
            m_KMP.is_shift_pressed = false;
            return true;
        }
    }

    return false;
}

void qelf_translater::load_KMP_table(const std::wstring& filename){
	std::locale::global(std::locale("japanese"));

	m_KMP = KMP();

    std::wifstream kmpfile(filename);
    kmpfile.imbue(std::locale("japanese"));
	
    enum CMode
    {
        def, keymap
    };

    CMode currentmode = def;


    std::unordered_map<std::wstring, KeyCode>ids;
    std::unordered_map<std::wstring, KMP::Dir>dirs;

    std::wstring line;
    
    while (std::getline(kmpfile, line)){

        auto newwend = std::remove_if(line.begin(), line.end(), ::isspace);
        
        line.erase(newwend, line.end());
        if (line.empty())continue;
        if (line[0] == L'-')continue;
  

        // 現在のモードの変更
        if (line[0] == L'@') {
            line = line.substr(1);

            if (line == L"def") {
                currentmode = def;
            }
            else if (line == L"keymap") {
                currentmode = keymap;
            }
            continue;
        }

        if (currentmode == def) {
            if (line[0] == L'#') {
                line = line.substr(1);
                std::vector<std::wstring>sepl;
                split(line, L':', sepl);

                if (sepl[0] == L"!iddef") {
                    ids[sepl[1]] = std::stoi(sepl[2], nullptr, 0);
                    dirs[sepl[1]] = KMP::Dir::None;
                }
                else if (sepl[0] == L"dirdef") {
                    if (sepl[2] == L"R") dirs[sepl[1]] = KMP::Dir::R;
                    else if (sepl[2] == L"L") dirs[sepl[1]] = KMP::Dir::L;

                }
                continue;
            }
        }
        else if (currentmode == keymap) {
            std::vector<std::wstring>sepl;
            split(line, L':', sepl);

            KMP::KMPelm telm;
            KeyCode from = 0;

            telm.dir = dirs[sepl[0]];

            if (sepl[1] == L"Send") {
                telm.cmd = KMP::KMPelm::Cmd::Send;
                telm.to_samedir = ids[sepl[2]];
            }
            else if (sepl[1] == L"DSend") {
                telm.cmd = KMP::KMPelm::Cmd::DSend;

                std::vector<std::wstring>seplfs;
                split(sepl[2], L'-', seplfs);

                std::vector<std::wstring>seplfrl;
                split(seplfs[0], L'/', seplfrl);

                
                
                if (seplfrl.size() >= 2) {
                    telm.to_otherdir = ids[seplfrl[0]];
                    telm.to_samedir = ids[seplfrl[1]];
                }
                else {
                    telm.to_samedir = ids[seplfrl[0]];
                    telm.to_otherdir = ids[seplfrl[0]];
                }
                telm.to_secboin = ids[seplfs[1]];
            }

            m_KMP.KMPs[ids[sepl[0]]] = telm;
        }
    }
    kmpfile.close();

	DebugTrace((L"load_KMP_table: " + std::to_wstring(m_KMP.KMPs.size()) + L" elements loaded."));
	for (auto& [key, value] : m_KMP.KMPs) {
		DebugTrace((L"Key: " + std::to_wstring(key) + L", Cmd: " + std::to_wstring(static_cast<int>(value.cmd)) + L", Dir: " + std::to_wstring(static_cast<int>(value.dir)) + L", to_samedir: " + std::to_wstring(value.to_samedir) + L", to_otherdir: " + std::to_wstring(value.to_otherdir) + L", to_secboin: " + std::to_wstring(value.to_secboin)));
	}
	

    return;
}

inline void qelf_translater::get_key_state(KMP::SpecialKeyStates& key_states) const
{
	key_states = m_KMP.is_key_pushed;
}

void qelf_translater::reset_kmp_state()
{
	DSendCnt = 0;
	m_KMP.is_key_pushed = { false,false };
	m_KMP.is_shift_pressed = false;
    Send1st = DsendElements{ KMP::Dir::None, {0}, };
}
