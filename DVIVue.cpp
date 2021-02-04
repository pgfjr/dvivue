// DVIVue.cpp : Defines the entry point for the application.
//

/*

    DVIVue: A viewer for DVI files and frontend for dvipdfmx.
    Copyright (c) 2020 by Peter Frane Jr.

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.



    The author may be contacted via the e-mail address pfranejr@hotmail.com

    Please refer to the following URL for further instructions on how to download
    the packages required by WebView2:

*/


#include "framework.h"
#include "DVIVue.h"
#include <wchar.h>
#include <shlwapi.h>

using namespace std;

#pragma comment(lib, "Shlwapi.lib")

#define MAX_LOADSTRING 100

using namespace Microsoft::WRL;
//using namespace std;

// Global Variables:
HINSTANCE hInst;                                // current instance
WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name

// Pointer to WebViewController
static wil::com_ptr<ICoreWebView2Controller> webviewController;

// Pointer to WebView window
static wil::com_ptr<ICoreWebView2> webviewWindow;
static wil::com_ptr <ICoreWebView2Environment> webEnvironment;
char log_file[MAX_PATH + 1]{ 0 };

// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);
void resize_window(HWND hWnd);
void process_command_line(HWND hWnd);
int on_create(HWND hWnd);
bool create_environment(HWND hWnd);
bool file_exists(const char* filename);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // TODO: Place code here.

    // Initialize global strings
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_DVIVUE, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // Perform application initialization:
    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_DVIVUE));

    MSG msg;

    // Main message loop:
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int) msg.wParam;
}



//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_DVIVUE));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_DVIVUE);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    hInst = hInstance; // Store instance handle in our global variable

    HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

    if (!hWnd)
    {
        return FALSE;
    }

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    if (!create_environment(hWnd))
    {
        DestroyWindow(hWnd);

        return FALSE;
    }
    return TRUE;
}
   

bool create_environment(HWND hWnd)
{
    HRESULT hr;
    wchar_t temp_path[MAX_PATH + 1]{ 0 };

    if (GetTempPath(MAX_PATH, temp_path) == 0)
    {
        MessageBox(hWnd, L"Unable to obtain the TEMP path", L"Internal Error", MB_OK);

        return false;
    }
    
    // To use a fixed version of WebView2, 
    // pass the folder name of the WebView2 runtime as the first parameter of CreateCoreWebView2EnvironmentWithOptions; 
    // otherwise, use nullptr to use an installed version of the runtime.
    // For more info see the following link: https://docs.microsoft.com/en-us/microsoft-edge/webview2/reference/win32/webview2-idl?view=webview2-0.9.622#createcorewebview2environmentwithoptions
    
        hr = CreateCoreWebView2EnvironmentWithOptions(nullptr, temp_path, nullptr,
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [hWnd](HRESULT result, ICoreWebView2Environment* env) -> HRESULT {

                // Create a CoreWebView2Controller and get the associated CoreWebView2 whose parent is the main window hWnd
                env->CreateCoreWebView2Controller(hWnd, Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                    [hWnd](HRESULT result, ICoreWebView2Controller* controller) -> HRESULT {
                        if (controller != nullptr) {
                            webviewController = controller;
                            if (webviewController->get_CoreWebView2(&webviewWindow) == S_OK)
                            {
                                // Resize WebView to fit the bounds of the parent window
                                resize_window(hWnd);

                                process_command_line(hWnd);
                            }
                        }

                        return S_OK;
                    }).Get());
                return S_OK;
            }).Get());

    return (S_OK == hr);
}
// read the messages of dvipdfx

void read_pipe(HWND hWnd, HANDLE child_stdout_read)
{
    HANDLE hlog;

    hlog = CreateFileA(log_file, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

    if (INVALID_HANDLE_VALUE == hlog)
    {
        MessageBox(hWnd, L"Unable to create the log file", L"Internal Error", MB_OK);

        return;
    }
    else
    {
        const size_t BUFSIZE = 4096;

        DWORD dw_read, dw_written;
        char buf[BUFSIZE];
        BOOL success = FALSE;

        while (true)
        {
            success = ReadFile(child_stdout_read, buf, BUFSIZE, &dw_read, NULL);

            if (!success || dw_read == 0)
                break;

            success = WriteFile(hlog, buf, dw_read, &dw_written, NULL);

            if (!success)
                break;
        }
        CloseHandle(hlog);
    }
}

struct handle
{
    HANDLE m_handle{ INVALID_HANDLE_VALUE };
    void close()
    {
        if (m_handle != INVALID_HANDLE_VALUE)
        {
            CloseHandle(m_handle);

            m_handle = INVALID_HANDLE_VALUE;
        }
    }    
    ~handle()
    {
        close();
    }
};

void show_pdf_file(HWND hWnd, const char *filename, const char *title)
{
    wchar_t pdf_file[MAX_PATH + 1]{ 0 };
        
    MultiByteToWideChar(CP_UTF8, 0, filename, lstrlenA(filename), pdf_file, MAX_PATH);

    if (webviewWindow->Navigate(pdf_file) == S_OK)
    {
        SetWindowTextA(hWnd, title);
    }
}

bool file_exists(const char* filename)
{
    WIN32_FILE_ATTRIBUTE_DATA attrib{ 0 };

    GetFileAttributesExA(filename, GetFileExInfoStandard, &attrib);

    if (attrib.nFileSizeLow > 0 || attrib.nFileSizeHigh > 0)
    {
        return true;
    }
    return false;
}

void create_pdf(HWND hWnd, const char* dvi_file)
{
    PROCESS_INFORMATION pi{ 0 };
    STARTUPINFOA si{ 0 };
    char param[MAX_PATH * 2 + 1]{ "dvipdfmx.exe -o " };
    char fname[_MAX_FNAME + 1]{ 0 };
    char temp_pdf_path[MAX_PATH + 1]{ 0 };
    handle child_stdout_read;
    handle child_stdout_write;
    SECURITY_ATTRIBUTES sa{ 0 };

    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    if (!CreatePipe(&child_stdout_read.m_handle, &child_stdout_write.m_handle, &sa, 0))
    {
        MessageBox(hWnd, L"Unable to create a pipe", L"Internal Error", MB_OK);

        return;
    }
    else
    {
        SetHandleInformation(child_stdout_read.m_handle, HANDLE_FLAG_INHERIT, 0);
    }

    if (_splitpath_s(dvi_file, 0, 0, 0, 0, fname, _MAX_FNAME, nullptr, 0) != 0)
    {
        MessageBox(hWnd, L"Unable to split the path", L"Internal Error", MB_OK);

        return;
    }

    if (GetTempPathA(MAX_PATH, temp_pdf_path) == 0)
    {
        MessageBox(hWnd, L"Unable to obtain the TEMP path", L"Internal Error", MB_OK);

        return;
    }
    else if (!PathFileExistsA(temp_pdf_path))
    {
        MessageBox(hWnd, L"The TEMP path folder is not available", L"Internal Error", MB_OK);

        return;
    }

    lstrcatA(log_file, temp_pdf_path);

    lstrcatA(log_file, fname);

    lstrcatA(log_file, ".log");

    lstrcatA(temp_pdf_path, fname);

    lstrcatA(temp_pdf_path, ".pdf");

    // enclose the parameters in quotes

    lstrcatA(param, "\"");

    lstrcatA(param, temp_pdf_path);

    // enclose the parameters in quotes

    lstrcatA(param, "\" \"");

    lstrcatA(param, dvi_file);

    // close the quote

    lstrcatA(param, "\"");

    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    //si.hStdInput = child_stdin_read.m_handle;
    si.hStdOutput = child_stdout_write.m_handle;
    si.hStdError = child_stdout_write.m_handle;
    si.wShowWindow = SW_HIDE;

    if (!CreateProcessA(nullptr, param, nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi))
    {
        MessageBox(hWnd, L"Unable to execute 'dvipdfmx.exe", L"Error", MB_OK);

        return;
    }
    else
    {
        DWORD exit_code = 0;

        WaitForSingleObject(pi.hProcess, INFINITE);

        GetExitCodeProcess(pi.hProcess, &exit_code);

        CloseHandle(pi.hProcess);
        
        CloseHandle(pi.hThread);

        child_stdout_write.close();
        
        read_pipe(hWnd, child_stdout_read.m_handle);
        
        child_stdout_read.close();        

        if (exit_code != 0)
        {
            MessageBox(hWnd, L"'dvipdfmx.exe returned an error.\nPlease see the log file in the File menu", L"Error", MB_OK);

            return;
        }
        else
        {
                if (file_exists(temp_pdf_path))
                {
                    const char* ext = strrchr(dvi_file, '.');

                    // can't fail, but check anyway
                    if (ext)
                    {
                        lstrcatA(fname, ext);
                    }
                    show_pdf_file(hWnd, temp_pdf_path, fname);
                }
        }
    }
    return;
}

void clear_log_file()
{
    log_file[0] = 0;
}

void open_file(HWND hWnd)
{
    OPENFILENAMEA ofn = { 0 };
    char filename[MAX_PATH + 1] = { 0 };

    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hWnd;
    ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    ofn.lpstrFilter = "DVI\0*.dvi\0\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;

    if (GetOpenFileNameA(&ofn) == TRUE)
    {
        create_pdf(hWnd, ofn.lpstrFile);
    }
}

void resize_window(HWND hWnd)
{
    if (webviewController != nullptr) 
    {
        RECT bounds;
        GetClientRect(hWnd, &bounds);
        webviewController->put_Bounds(bounds);
    }
}

void process_command_line(HWND hWnd)
{
    LPWSTR* arg_list;
    int argc;

    arg_list = CommandLineToArgvW(GetCommandLineW(), &argc);

    if (nullptr == arg_list)
    {
        MessageBox(hWnd, L"CommandLineToArgvW failed", L"Error", MB_OK);

        return;
    }
    else if (argc > 1)
    {
        char dvi_file[MAX_PATH + 1]{ 0 };
        int result;

        result = WideCharToMultiByte(CP_UTF8, 0, arg_list[1], -1, dvi_file, (int)MAX_PATH, 0, FALSE);

        LocalFree(arg_list);

        if (result > 0)
        {
            if (!file_exists(dvi_file))
            {
                lstrcatA(dvi_file, ".dvi");

                if (!file_exists(dvi_file))
                {
                    MessageBoxA(hWnd, dvi_file, "File not found", MB_OK);

                    return;
                }
            }
            create_pdf(hWnd, dvi_file);
        }
        else
        {
            MessageBox(hWnd, L"Unable to convert a string from wide char to multibyte", L"Internal Error", MB_OK);

            return;
        }
    }
}

void run_application(HWND hWnd, const wchar_t *app_name, const wchar_t *input_filename)
{
    PROCESS_INFORMATION pi{ 0 };
    STARTUPINFOW si{ 0 };
    wchar_t param[MAX_PATH *2 + 1]{0};

    si.cb = sizeof(si);

    lstrcpyW(param, app_name);
    lstrcatW(param, L" ");

    if (input_filename)
    {
        lstrcatW(param, input_filename);
    }

    if (!CreateProcessW(nullptr, param, nullptr, nullptr, false, 0, nullptr, nullptr, &si, &pi))
    {
        wchar_t msg[MAX_PATH + 1]{ L"Unable to execute " };

        lstrcatW(msg, app_name);

        MessageBoxW(hWnd, msg, L"Error", MB_OK);

        return;
    }

    CloseHandle(pi.hProcess);

    CloseHandle(pi.hThread);
}

void view_log_file(HWND hWnd)
{
    wchar_t input_filename[MAX_PATH + 1]{ 0 };

    MultiByteToWideChar(CP_UTF8, 0, log_file, -1, input_filename, MAX_PATH);

    run_application(hWnd, L"notepad.exe", input_filename);
}


void new_instance(HWND hWnd, const wchar_t* input_filename)
{
    wchar_t app_name[MAX_PATH + 1]{ 0 };

    GetModuleFileNameW(nullptr, app_name, MAX_PATH);

    run_application(hWnd, app_name, input_filename);
}

void close_file(HWND hWnd)
{
    webviewWindow->Navigate(L"about:blank");
    clear_log_file();
    SetWindowText(hWnd, L"DVIVue");
}
//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE: Processes messages for the main window.
//
//  WM_COMMAND  - process the application menu
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            // Parse the menu selections:
            switch (wmId)
            {
            case IDM_ABOUT:
                DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
                break;
            case IDM_EXIT:
                DestroyWindow(hWnd);
                break;
            case IDM_FILE_NEW:
                new_instance(hWnd, nullptr);
                break;
            case IDM_FILE_OPEN:
                open_file(hWnd);
                break;
            case IDM_FILE_CLOSE:
                close_file(hWnd);
                break;
            case IDM_FILE_VIEW_LOG:
                view_log_file(hWnd);
                break;
            default:
                return DefWindowProc(hWnd, message, wParam, lParam);
            }
        }
        break;
    case WM_INITMENU:
        {
            HMENU hmenu = (HMENU)wParam;

            if (log_file[0] != 0)
            {
                EnableMenuItem(hmenu, IDM_FILE_VIEW_LOG, MF_ENABLED);
            }
            else
            {
                EnableMenuItem(hmenu, IDM_FILE_VIEW_LOG, MF_DISABLED | MF_GRAYED);
            }
        }
        break;
    case WM_SIZE:
        resize_window(hWnd);
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}
