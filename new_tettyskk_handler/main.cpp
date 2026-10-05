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
const std::wstring KMP_FILE_NAME = L"kmp.kmp";

// インストール先のフォルダ名（Program Files以下）
const std::wstring INSTALL_DIR_NAME = L"new_tettyskk";

// アプリケーションデータのフォルダ名（AppData\Roaming以下）
const std::wstring APPDATA_DIR_NAME = L"new_tettyskk";

// ダウンロードするシステム辞書のURLと保存ファイル名
const std::wstring DICT_URL = L"http://openlab.ring.gr.jp/skk/skk/dic/SKK-JISYO.L";
const std::wstring DICT_FILE_NAME = L"SKK-JISYO.L";

// ユーザー辞書のファイル名
const std::wstring USER_DICT_NAME = L"SKK-JISYO.USER";
// 変換履歴
const std::wstring HISTORY_FILE_NAME = L"SKK-JISYO.USER.HIST";
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
    fs::path sourceKmp = exeDir / KMP_FILE_NAME;

    if (!fs::exists(sourceDll)) {
        PrintError(L"インストール元のDLLが見つかりません: " + sourceDll.wstring());
        return;
    }
    if (!fs::exists(sourceKmp)) {
        PrintError(L"インストール元のKMPファイルが見つかりません: " + sourceKmp.wstring());
        return;
    }

    fs::path targetDir = GetProgramFilesPath() / INSTALL_DIR_NAME;
    fs::path targetDll = targetDir / DLL_NAME;

    fs::path appData = GetAppDataPath();
    if (appData.empty()) {
        PrintError(L"AppDataのパスが取得できませんでした。");
        return;
    }
    fs::path targetKmpDir = appData / APPDATA_DIR_NAME;
    fs::path targetKmp = targetKmpDir / KMP_FILE_NAME;

    PrintInfo(L"IMEを " + targetDir.wstring() + L" にインストールします。");
    PrintInfo(L"KMPファイルを " + targetKmpDir.wstring() + L" にコピーします。");

    bool dirCreated = false;
    bool kmpDirCreated = false;
    std::error_code ec;

    if (!fs::exists(targetDir)) {
        if (!fs::create_directories(targetDir, ec)) {
            PrintError(L"インストール先ディレクトリの作成に失敗しました: " + targetDir.wstring());
            return;
        }
        dirCreated = true;
    }

    if (!fs::exists(targetKmpDir)) {
        if (!fs::create_directories(targetKmpDir, ec)) {
            PrintError(L"KMP用ディレクトリの作成に失敗しました: " + targetKmpDir.wstring());
            if (dirCreated) fs::remove(targetDir, ec);
            return;
        }
        kmpDirCreated = true;
    }

    fs::copy_file(sourceDll, targetDll, fs::copy_options::overwrite_existing, ec);
    if (ec) {
        std::wstring wmsg(ec.message().begin(), ec.message().end());
        PrintError(L"DLLのコピーに失敗しました: " + wmsg);
        
        // ロールバック
        if (kmpDirCreated) fs::remove(targetKmpDir, ec);
        if (dirCreated) fs::remove(targetDir, ec);
        return;
    }

    fs::copy_file(sourceKmp, targetKmp, fs::copy_options::overwrite_existing, ec);
    if (ec) {
        std::wstring wmsg(ec.message().begin(), ec.message().end());
        PrintError(L"KMPファイルのコピーに失敗しました: " + wmsg);
        
        // ロールバック
        fs::remove(targetDll, ec);
        if (kmpDirCreated) fs::remove(targetKmpDir, ec);
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
            fs::remove(targetKmp, ec);
            if (kmpDirCreated) fs::remove(targetKmpDir, ec);
            if (dirCreated) fs::remove(targetDir, ec);
        }
    } else {
        PrintError(L"regsvr32.exe の起動に失敗しました。変更を元に戻します...");
        fs::remove(targetDll, ec);
        fs::remove(targetKmp, ec);
        if (kmpDirCreated) fs::remove(targetKmpDir, ec);
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

    fs::path appData = GetAppDataPath();
    fs::path targetKmp;
    if (!appData.empty()) {
        targetKmp = appData / APPDATA_DIR_NAME / KMP_FILE_NAME;
    }

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

    PrintInfo(L"DLLと関連ファイルを削除しています...");
    std::error_code ec;
    fs::remove(targetDll, ec);
    if (ec) {
        PrintError(L"DLLの削除に失敗しました。他のプロセス（アプリケーション）で使用中の可能性があります。OSを再起動後に再度実行するか、手動で削除してください。");
    } else {
        PrintSuccess(L"DLLを削除しました。");
        
        if (!targetKmp.empty()) {
            fs::remove(targetKmp, ec);
            if (!ec) {
                PrintSuccess(L"KMPファイルを削除しました。");
            }
            // ディレクトリが空なら削除
            if (fs::is_empty(appData / APPDATA_DIR_NAME, ec)) {
                fs::remove(appData / APPDATA_DIR_NAME, ec);
                PrintSuccess(L"辞書用ディレクトリを削除しました。");
            }
        }
        
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

void ClearUserHistory() {
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

    fs::path userDict = targetDir / HISTORY_FILE_NAME;

    // ファイルを上書き（または新規作成）して空にする
    HANDLE hFile = CreateFileW(userDict.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        CloseHandle(hFile);
        PrintSuccess(L"ユーザー変換履歴を初期化（または新規作成）しました: " + userDict.wstring());
    }
    else {
        PrintError(L"ユーザー変換履歴の初期化に失敗しました。");
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

std::wstring FormatFileTime(const FILETIME& ft) {
    SYSTEMTIME stUTC, stLocal;
    FileTimeToSystemTime(&ft, &stUTC);
    SystemTimeToTzSpecificLocalTime(NULL, &stUTC, &stLocal);
    
    wchar_t buf[256];
    swprintf_s(buf, 256, L"%04d/%02d/%02d %02d:%02d:%02d",
        stLocal.wYear, stLocal.wMonth, stLocal.wDay,
        stLocal.wHour, stLocal.wMinute, stLocal.wSecond);
    return std::wstring(buf);
}

void PrintFileInfo(const std::wstring& label, const fs::path& p) {
    std::wcout << L"[" << label << L"]\n";
    std::wcout << L"  場所: " << p.wstring() << L"\n";
    
    WIN32_FILE_ATTRIBUTE_DATA fileInfo;
    if (GetFileAttributesExW(p.c_str(), GetFileExInfoStandard, &fileInfo)) {
        std::wcout << L"  状態: 存在します\n";
        std::wcout << L"  作成日時: " << FormatFileTime(fileInfo.ftCreationTime) << L"\n";
        std::wcout << L"  更新日時: " << FormatFileTime(fileInfo.ftLastWriteTime) << L"\n";
    } else {
        std::wcout << L"  状態: 存在しません\n";
    }
    std::wcout << L"\n";
}

void PrintStatus() {
    std::wcout << L"--- 現在のステータス ---\n";
    
    fs::path targetDll = GetProgramFilesPath() / INSTALL_DIR_NAME / DLL_NAME;
    PrintFileInfo(L"IME DLL", targetDll);

    fs::path appData = GetAppDataPath();
    if (!appData.empty()) {
        fs::path targetKmp = appData / APPDATA_DIR_NAME / KMP_FILE_NAME;
        PrintFileInfo(L"KMP ファイル", targetKmp);

        fs::path systemDict = appData / APPDATA_DIR_NAME / DICT_FILE_NAME;
        PrintFileInfo(L"システム辞書", systemDict);

        fs::path userDict = appData / APPDATA_DIR_NAME / USER_DICT_NAME;
        PrintFileInfo(L"ユーザー辞書", userDict);

		fs::path userHistory = appData / APPDATA_DIR_NAME / HISTORY_FILE_NAME;
		PrintFileInfo(L"ユーザー変換履歴", userHistory);
    } else {
        std::wcout << L"[辞書・KMP] AppDataのパスが取得できないためステータスを表示できません。\n\n";
    }
    std::wcout << L"------------------------\n\n";
}

void ShowMenu() {
    std::vector<std::wstring> menuItems = {
        L"1. IMEのインストール (要管理者権限)",
        L"2. IMEのアンインストール (要管理者権限)",
        L"3. システム辞書のダウンロード・更新",
        L"4. ユーザー辞書の初期化・新規作成",
		L"5. ユーザー変換履歴の初期化・新規作成",
        L"6 終了"
    };
    int selectedIndex = 0;
    
    while (true) {
        ClearScreen();
        std::wcout << L"=======================================\n";
        std::wcout << L"       new_tettySKK Handler ツール     \n";
        std::wcout << L"=======================================\n\n";
        
        PrintStatus();

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
				case 4: ClearUserHistory(); break;
                case 5: return;
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
