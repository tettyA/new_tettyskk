#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <windows.h>
#include <shlobj.h>
#include <urlmon.h>
#include <conio.h>
#include <fcntl.h>
#include <io.h>

#pragma comment(lib, "urlmon.lib")

namespace fs = std::filesystem;

// ==== 構成用定数 ====
// IME DLLの名前
const std::wstring DLL_NAME = L"new_tettyskk.dll";

// インストール先のフォルダ名（Program Files以下）
const std::wstring INSTALL_DIR_NAME = L"new_tettyskk";

// アプリケーションデータのフォルダ名（AppData\Roaming以下）
const std::wstring APPDATA_DIR_NAME = L"new_tettyskk";

// ダウンロードするシステム辞書のURLと保存ファイル名
const std::wstring DICT_URL = L"https://openlab.ring.gr.jp/skk/skk/dic/SKK-JISYO.L";
const std::wstring DICT_FILE_NAME = L"SKK-JISYO.L";

// ユーザー辞書のファイル名
const std::wstring USER_DICT_NAME = L"SKK-JISYO.USER.L";
// =================================

void PrintError(const std::wstring& msg) {
    std::wcerr << L"[エラー] " << msg << L"\n";
}

void PrintSuccess(const std::wstring& msg) {
    std::wcout << L"[成功] " << msg << L"\n";
}

void PrintInfo(const std::wstring& msg) {
    std::wcout << L"[情報] " << msg << L"\n";
}

// Program Filesのパスを取得
fs::path GetProgramFilesPath() {
    wchar_t* path = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_ProgramFiles, 0, NULL, &path))) {
        fs::path p(path);
        CoTaskMemFree(path);
        return p;
    }
    return fs::path(L"C:\\Program Files");
}

// AppData\Roamingのパスを取得
fs::path GetAppDataPath() {
    wchar_t* path = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, NULL, &path))) {
        fs::path p(path);
        CoTaskMemFree(path);
        return p;
    }
    return fs::path();
}

// 現在の実行ファイル（自分自身）のディレクトリを取得
fs::path GetExeDir() {
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(NULL, path, MAX_PATH);
    return fs::path(path).parent_path();
}

// 管理者権限で実行されているか確認
bool IsElevated() {
    bool isElevated = false;
    HANDLE hToken = NULL;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
        TOKEN_ELEVATION Elevation;
        DWORD cbSize = sizeof(TOKEN_ELEVATION);
        if (GetTokenInformation(hToken, TokenElevation, &Elevation, sizeof(Elevation), &cbSize)) {
            isElevated = Elevation.TokenIsElevated;
        }
        CloseHandle(hToken);
    }
    return isElevated;
}

void InstallIME() {
    if (!IsElevated()) {
        PrintError(L"IMEをインストールするには、このプログラムを管理者権限で実行してください。");
        return;
    }

    fs::path exeDir = GetExeDir();
    fs::path sourceDll = exeDir / DLL_NAME;

    if (!fs::exists(sourceDll)) {
        PrintError(L"インストール元のDLLが見つかりません: " + sourceDll.wstring());
        return;
    }

    fs::path targetDir = GetProgramFilesPath() / INSTALL_DIR_NAME;
    fs::path targetDll = targetDir / DLL_NAME;

    PrintInfo(L"IMEを " + targetDir.wstring() + L" にインストールします。");

    bool dirCreated = false;
    std::error_code ec;

    if (!fs::exists(targetDir)) {
        if (!fs::create_directories(targetDir, ec)) {
            PrintError(L"インストール先ディレクトリの作成に失敗しました: " + targetDir.wstring());
            return;
        }
        dirCreated = true;
    }

    fs::copy_file(sourceDll, targetDll, fs::copy_options::overwrite_existing, ec);
    if (ec) {
        std::wstring wmsg(ec.message().begin(), ec.message().end());
        PrintError(L"DLLのコピーに失敗しました: " + wmsg);
        
        // ロールバック
        if (dirCreated) fs::remove(targetDir, ec);
        return;
    }

    // DLLの登録 (regsvr32)
    PrintInfo(L"DLLを登録しています...");
    std::wstring cmd = L"regsvr32.exe /s \"" + targetDll.wstring() + L"\"";
    
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    if (CreateProcessW(NULL, &cmd[0], NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, INFINITE);
        DWORD exitCode = 0;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

        if (exitCode == 0) {
            PrintSuccess(L"IMEのインストールおよび登録が正常に完了しました。");
        } else {
            PrintError(L"DLLの登録に失敗しました (regsvr32 exit code: " + std::to_wstring(exitCode) + L")。変更を元に戻します...");
            fs::remove(targetDll, ec);
            if (dirCreated) fs::remove(targetDir, ec);
        }
    } else {
        PrintError(L"regsvr32.exe の起動に失敗しました。変更を元に戻します...");
        fs::remove(targetDll, ec);
        if (dirCreated) fs::remove(targetDir, ec);
    }
}

void UninstallIME() {
    if (!IsElevated()) {
        PrintError(L"IMEをアンインストールするには、このプログラムを管理者権限で実行してください。");
        return;
    }

    fs::path targetDir = GetProgramFilesPath() / INSTALL_DIR_NAME;
    fs::path targetDll = targetDir / DLL_NAME;

    if (!fs::exists(targetDll)) {
        PrintError(L"インストールされているIME DLLが見つかりません: " + targetDll.wstring());
        return;
    }

    PrintInfo(L"DLLの登録を解除しています...");
    std::wstring cmd = L"regsvr32.exe /u /s \"" + targetDll.wstring() + L"\"";
    
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    if (CreateProcessW(NULL, &cmd[0], NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        PrintSuccess(L"DLLの登録が解除されました。");
    } else {
        PrintError(L"regsvr32.exe の起動に失敗しました。");
    }

    PrintInfo(L"DLLを削除しています...");
    std::error_code ec;
    fs::remove(targetDll, ec);
    if (ec) {
        PrintError(L"DLLの削除に失敗しました。他のプロセス（アプリケーション）で使用中の可能性があります。OSを再起動後に再度実行するか、手動で削除してください。");
    } else {
        PrintSuccess(L"DLLを削除しました。");
        // ディレクトリが空なら削除
        if (fs::is_empty(targetDir, ec)) {
            fs::remove(targetDir, ec);
            PrintSuccess(L"インストールディレクトリを削除しました。");
        }
    }
}

void DownloadDictionary() {
    fs::path appData = GetAppDataPath();
    if (appData.empty()) {
        PrintError(L"AppDataのパスが取得できませんでした。");
        return;
    }

    fs::path targetDir = appData / APPDATA_DIR_NAME;
    std::error_code ec;
    if (!fs::exists(targetDir)) {
        fs::create_directories(targetDir, ec);
    }

    fs::path targetDict = targetDir / DICT_FILE_NAME;
    
    PrintInfo(L"システム辞書をダウンロードしています...");
    PrintInfo(L"URL: " + DICT_URL);
    PrintInfo(L"保存先: " + targetDict.wstring());
    PrintInfo(L"しばらくお待ちください...");

    HRESULT hr = URLDownloadToFileW(NULL, DICT_URL.c_str(), targetDict.c_str(), 0, NULL);
    if (SUCCEEDED(hr)) {
        PrintSuccess(L"辞書のダウンロードが完了しました。");
    } else {
        PrintError(L"辞書のダウンロードに失敗しました。エラーコード: " + std::to_wstring(hr));
    }
}

void ClearUserDictionary() {
    fs::path appData = GetAppDataPath();
    if (appData.empty()) {
        PrintError(L"AppDataのパスが取得できませんでした。");
        return;
    }

    fs::path targetDir = appData / APPDATA_DIR_NAME;
    std::error_code ec;
    if (!fs::exists(targetDir)) {
        fs::create_directories(targetDir, ec);
    }

    fs::path userDict = targetDir / USER_DICT_NAME;

    // ファイルを上書き（または新規作成）して空にする
    HANDLE hFile = CreateFileW(userDict.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        CloseHandle(hFile);
        PrintSuccess(L"ユーザー辞書を初期化（または新規作成）しました: " + userDict.wstring());
    } else {
        PrintError(L"ユーザー辞書の初期化に失敗しました。");
    }
}

void ClearScreen() {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    COORD coordScreen = { 0, 0 };
    DWORD cCharsWritten;
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    DWORD dwConSize;
    if (GetConsoleScreenBufferInfo(hConsole, &csbi)) {
        dwConSize = csbi.dwSize.X * csbi.dwSize.Y;
        FillConsoleOutputCharacterW(hConsole, L' ', dwConSize, coordScreen, &cCharsWritten);
        FillConsoleOutputAttribute(hConsole, csbi.wAttributes, dwConSize, coordScreen, &cCharsWritten);
        SetConsoleCursorPosition(hConsole, coordScreen);
    }
}

void ShowMenu() {
    std::vector<std::wstring> menuItems = {
        L"1. IMEのインストール (要管理者権限)",
        L"2. IMEのアンインストール (要管理者権限)",
        L"3. システム辞書のダウンロード・更新",
        L"4. ユーザー辞書の初期化・新規作成",
        L"5. 終了"
    };
    int selectedIndex = 0;
    
    while (true) {
        ClearScreen();
        std::wcout << L"=======================================\n";
        std::wcout << L"       new_tettySKK Handler ツール     \n";
        std::wcout << L"=======================================\n\n";

        for (size_t i = 0; i < menuItems.size(); ++i) {
            if (i == selectedIndex) {
                std::wcout << L"  > " << menuItems[i] << L" <\n";
            } else {
                std::wcout << L"    " << menuItems[i] << L"\n";
            }
        }
        std::wcout << L"\n[↑][↓] キーで選択、[Enter] で実行、[Esc] で終了します。\n";

        int c = _getch();
        if (c == 224) { // 矢印キーのプレフィックス
            c = _getch();
            if (c == 72) { // 上
                if (selectedIndex > 0) selectedIndex--;
            } else if (c == 80) { // 下
                if (selectedIndex < (int)menuItems.size() - 1) selectedIndex++;
            }
        } else if (c == '\r') { // Enterキー
            ClearScreen();
            switch (selectedIndex) {
                case 0: InstallIME(); break;
                case 1: UninstallIME(); break;
                case 2: DownloadDictionary(); break;
                case 3: ClearUserDictionary(); break;
                case 4: return;
            }
            std::wcout << L"\n何かキーを押すとメニューに戻ります...";
            _getch();
        } else if (c == 27) { // Escキー
            break;
        }
    }
}

int main() {
    // コンソールの出力をUTF-16に変更して、日本語を正常に表示する
    _setmode(_fileno(stdout), _O_U16TEXT);
    _setmode(_fileno(stderr), _O_U16TEXT);

    ShowMenu();
    return 0;
}
