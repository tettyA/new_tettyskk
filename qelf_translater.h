#pragma once
#include <string>

#include <vector>
#include <unordered_map>

//濁音
#define SAMEHAND 1
//清音
#define ANOTHERHAND 0


#define ex_smalltu  (0x97)
#define ex_nn       (0x98)
#define ex_youon    (0x99)
#define ex_boin     (0x9C)
#define ex_sakujo   (0x9A)

using  KeyCode = WCHAR;
struct KMP {
    enum class Dir {
        None, L, R,
    };

    // あるキーに割り当てられているのKMP要素
    struct KMPelm {
        enum class Cmd
        {
            Send,
            DSend
        };

        Cmd cmd;  // このキーがSendか，DSendであるか
		KMP::Dir dir;  // このキーのDir (L/R/None)
        KeyCode to_samedir;  // 同じDirの時，送られるKeyCode
		KeyCode to_otherdir;// 違うDirの時，送られるKeyCode
        KeyCode to_secboin; // 二度目に押された時，送られる母音(だいたい)のKeyCode
    };

    struct SpecialKeyStates {
        bool sokuon;
        bool youon;
        bool shift;
    }is_key_pushed;

    std::unordered_map<KeyCode, KMPelm>KMPs;
};



class qelf_translater
{
public:
	qelf_translater();
	~qelf_translater();


	/// <summary>
    /// QWERTY配列からQ-ELF配列への変換を行います。
	/// 全ての引数は大文字で指定し，返却されます。
    /// </summary>
    /// <param name="key">QWERTY配列時のキー</param>
    /// <param name="output">戻り値がtrueの時，変換した仮名がKanaModeに応じて入ります</param>
    /// <returns>trueの時，変換を行った。falseの時，変換に達しなかった</returns>
	bool translate_qwerty_to_qelf(const KeyCode& key, std::wstring& output);

	void load_KMP_table(const std::wstring& filename);
private:
	//QWERTY配列からQ-ELF配列への変換テーブル
	KMP m_KMP;

    int DSendCnt;
    struct DsendElements {
        KMP::Dir dir;
        union {
            KeyCode samedirkc;
            KeyCode dirkc;
        }dir1;
        KeyCode otherdirkc;
    }Send1st;
};

