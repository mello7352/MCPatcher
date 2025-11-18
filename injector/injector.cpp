#include <windows.h>
#include <tlhelp32.h>
#include <iostream>
#include <string>
#include <fstream>
#include <cctype>

using namespace std;

// 通过进程名查找进程ID
DWORD FindProcessId(const string& processName) {
    DWORD processId = 0;
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    if (hSnapshot == INVALID_HANDLE_VALUE) {
        return 0;
    }

    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);

    if (Process32First(hSnapshot, &pe32)) {
        do {
            char narrowExeName[MAX_PATH];
            WideCharToMultiByte(CP_ACP, 0, pe32.szExeFile, -1, narrowExeName, MAX_PATH, NULL, NULL);
            if (_stricmp(narrowExeName, processName.c_str()) == 0) {
                processId = pe32.th32ProcessID;
                break;
            }
        } while (Process32Next(hSnapshot, &pe32));
    }

    CloseHandle(hSnapshot);
    return processId;
}

// 检查文件是否存在
bool FileExists(const string& path) {
    ifstream file(path);
    return file.good();
}

// 执行DLL注入
bool InjectDLL(DWORD processId, const string& dllPath) {
    cout << "[+] 正在打开目标文件..." << endl;

    // 打开目标进程
    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processId);
    if (hProcess == NULL) {
        cerr << "[-] 打开进程失败: " << GetLastError() << endl;
        return false;
    }

    cout << "[+] 在目标进程分配内存..." << endl;

    // 在目标进程中分配内存
    SIZE_T pathLen = dllPath.length() + 1;
    LPVOID pRemotePath = VirtualAllocEx(hProcess, NULL, pathLen,
        MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (pRemotePath == NULL) {
        cerr << "[-] 分配内存失败: " << GetLastError() << endl;
        CloseHandle(hProcess);
        return false;
    }

    cout << "[+] 写入DLL路径..." << endl;

    // 写入DLL路径到目标进程
    if (!WriteProcessMemory(hProcess, pRemotePath, dllPath.c_str(), pathLen, NULL)) {
        cerr << "[-] 写入内存失败: " << GetLastError() << endl;
        VirtualFreeEx(hProcess, pRemotePath, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    cout << "[+] Getting LoadLibraryA address..." << endl;

    // 获取LoadLibraryA的地址
    HMODULE hKernel32 = GetModuleHandleA("kernel32.dll");
    LPVOID pLoadLibrary = (LPVOID)GetProcAddress(hKernel32, "LoadLibraryA");

    if (pLoadLibrary == NULL) {
        cerr << "[-] 获取LoadLibrary地址失败" << endl;
        VirtualFreeEx(hProcess, pRemotePath, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    cout << "[+] 执行远程线程..." << endl;

    // 在目标进程中创建远程线程执行LoadLibraryA
    HANDLE hThread = CreateRemoteThread(hProcess, NULL, 0,
        (LPTHREAD_START_ROUTINE)pLoadLibrary,
        pRemotePath, 0, NULL);
    if (hThread == NULL) {
        cerr << "[-] Failed to create remote thread. Error: " << GetLastError() << endl;
        VirtualFreeEx(hProcess, pRemotePath, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    cout << "[+] 注入成功" << endl;
    cout << "[+] 等待线程结束..." << endl;

    // 等待远程线程完成
    WaitForSingleObject(hThread, INFINITE);

    cout << "[+] 远程进程完成!" << endl;

    // 清理
    CloseHandle(hThread);
    VirtualFreeEx(hProcess, pRemotePath, 0, MEM_RELEASE);
    CloseHandle(hProcess);

    return true;
}

// 判断字符串是否为数字
bool IsNumeric(const string& str) {
    for (char c : str) {
        if (!isdigit(c)) {
            return false;
        }
    }
    return !str.empty();
}

int main(int argc, char* argv[]) {
    cout << "========================================" << endl;
    cout << "       DLL 注入      " << endl;
    cout << "========================================" << endl << endl;
    string target;
    string dllPath;
    bool dump = 0;
    if (argc < 2) {
		cout << "运行时模式" << endl;
        target = "Minecraft.Windows.exe";
        dllPath = "MCpatcher2.dll";
		system("start purchase.html");
    }
    else {
        cout << "复制游戏模式" << endl;
        target = "Minecraft.Windows.exe";
        dllPath = "FileCopy.dll";
        dump = 1;
    }
    

    // 检查DLL文件是否存在
    cout << "[*] 检查 DLL 文件..." << endl;
    if (!FileExists(dllPath)) {
        cerr << "[-] DLL 找不到: " << dllPath << endl;
        return 1;
    }

    // 获取DLL的完整路径
    char fullDllPath[MAX_PATH];
    if (!GetFullPathNameA(dllPath.c_str(), MAX_PATH, fullDllPath, NULL)) {
        cerr << "[-] 无法获取完整路径" << endl;
        return 1;
    }

    cout << "[+] DLL 路径: " << fullDllPath << endl << endl;

    // 判断目标是进程名还是PID
    DWORD processId = 0;

	processId = 0;

    while (processId == 0) {
        processId = FindProcessId(target);
        if (dump) {
            Sleep(2000);
        }
    }

    cout << endl << "[*] Starting injection process..." << endl << endl;

    // 执行注入
    if(!dump)
    if (InjectDLL(processId, fullDllPath)) {
        cout << endl << "========================================" << endl;
        cout << "成功!" << endl;
        cout << "========================================" << endl;
		return 0;
    }
    else {
        cerr << endl << "========================================" << endl;
        cerr << "失败!" << endl;
        cerr << "========================================" << endl;
		return 1;
    }
    if(dump)
    if (InjectDLL(processId, fullDllPath)) {
        cout << endl << "========================================" << endl;
        cout << "成功!" << endl;
        cout << "========================================" << endl;
        cout << "等待进程结束" << endl;
        HANDLE hrProcess = OpenProcess(SYNCHRONIZE, FALSE, processId);
        WaitForSingleObject(hrProcess, INFINITE);
        CloseHandle(hrProcess);
        return 0;
    }
    else {
        cerr << endl << "========================================" << endl;
        cerr << "失败!" << endl;
        cerr << "========================================" << endl;
        return 1;
    }
}
