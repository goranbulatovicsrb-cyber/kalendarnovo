// ============================================================================
//  RASPORED SMJENA - Kalendar Aplikacija v2.0
//  Opis:  Aplikacija za evidenciju radnih smjena sa kalendarskim prikazom.
//         Podrzava dnevnu smjenu, nocnu smjenu i slobodne dane.
//         Podaci se cuvaju u fajlu pored exe-a.
//         NOVO: Kopiranje rasporeda na ostale mjesece/godine.
//  Build: Windows + GDI+ (nema eksternih zavisnosti)
// ============================================================================

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <gdiplus.h>
#include <commctrl.h>
#include <string>
#include <map>
#include <fstream>
#include <sstream>
#include <ctime>
#include <vector>
#include <algorithm>

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "comctl32.lib")

using namespace Gdiplus;

// ============================================================================
//  CONSTANTS
// ============================================================================

enum ShiftType { SHIFT_NONE = 0, SHIFT_DAY = 1, SHIFT_NIGHT = 2, SHIFT_FREE = 3 };

#define IDM_DAY_SHIFT   1001
#define IDM_NIGHT_SHIFT 1002
#define IDM_FREE_DAY    1003
#define IDM_CLEAR       1004

// Copy dialog control IDs
#define IDC_CHK_JAN     2001
#define IDC_CHK_FEB     2002
#define IDC_CHK_MAR     2003
#define IDC_CHK_APR     2004
#define IDC_CHK_MAY     2005
#define IDC_CHK_JUN     2006
#define IDC_CHK_JUL     2007
#define IDC_CHK_AUG     2008
#define IDC_CHK_SEP     2009
#define IDC_CHK_OCT     2010
#define IDC_CHK_NOV     2011
#define IDC_CHK_DEC     2012
#define IDC_YEAR_EDIT   2020
#define IDC_YEAR_UP     2021
#define IDC_YEAR_DOWN   2022
#define IDC_BTN_OK      2030
#define IDC_BTN_CANCEL  2031
#define IDC_BTN_ALL     2032
#define IDC_BTN_NONE    2033
#define IDC_CHK_OVERWRITE 2040

static const int HEADER_H    = 75;
static const int DAYNAMES_H  = 38;
static const int LEGEND_H    = 65;
static const int STATS_H     = 38;
static const int CELL_PAD    = 3;
static const int MIN_W       = 850;
static const int MIN_H       = 680;

// Colors - Modern dark theme
static const Color CLR_BG_TOP(255, 12, 12, 28);
static const Color CLR_BG_BOT(255, 22, 22, 48);
static const Color CLR_HEADER(255, 18, 18, 40);
static const Color CLR_CELL_BG(255, 28, 30, 58);
static const Color CLR_CELL_HOVER(255, 40, 42, 75);
static const Color CLR_CELL_TODAY_BORDER(255, 100, 130, 255);
static const Color CLR_TEXT(255, 210, 215, 235);
static const Color CLR_TEXT_DIM(255, 130, 135, 160);
static const Color CLR_TEXT_WEEKEND(255, 170, 140, 160);
static const Color CLR_DAY_SHIFT(255, 255, 160, 40);
static const Color CLR_DAY_SHIFT_BG(255, 60, 45, 15);
static const Color CLR_NIGHT_SHIFT(255, 80, 130, 255);
static const Color CLR_NIGHT_SHIFT_BG(255, 20, 28, 65);
static const Color CLR_FREE_DAY(255, 80, 200, 100);
static const Color CLR_FREE_DAY_BG(255, 18, 50, 25);
static const Color CLR_BTN(255, 45, 48, 85);
static const Color CLR_BTN_HOVER(255, 65, 68, 110);
static const Color CLR_BTN_COPY(255, 55, 40, 90);
static const Color CLR_BTN_COPY_HOVER(255, 80, 55, 125);
static const Color CLR_BTN_CLEARMONTH(255, 90, 30, 30);
static const Color CLR_BTN_CLEARMONTH_HOVER(255, 120, 40, 40);
static const Color CLR_SEPARATOR(255, 50, 52, 80);
static const Color CLR_GRID_LINE(255, 35, 37, 65);

// Month names
static const wchar_t* MONTH_NAMES[] = {
    L"Januar", L"Februar", L"Mart", L"April",
    L"Maj", L"Juni", L"Juli", L"August",
    L"Septembar", L"Oktobar", L"Novembar", L"Decembar"
};

static const wchar_t* MONTH_NAMES_SHORT[] = {
    L"Jan", L"Feb", L"Mar", L"Apr",
    L"Maj", L"Jun", L"Jul", L"Aug",
    L"Sep", L"Okt", L"Nov", L"Dec"
};

static const wchar_t* DAY_NAMES[] = {
    L"PON", L"UTO", L"SRI", L"\x010CET", L"PET", L"SUB", L"NED"
};

// ============================================================================
//  GLOBAL STATE
// ============================================================================

static HWND                           g_hWnd = NULL;
static ULONG_PTR                      g_gdipToken = 0;
static int                            g_viewMonth = 0;
static int                            g_viewYear  = 0;
static int                            g_todayDay = 0, g_todayMonth = 0, g_todayYear = 0;
static int                            g_hoverDay = -1;
static int                            g_hoverBtn = -1; // 0=prev,1=next,2=danas,3=copy,4=clearmonth
static std::map<std::wstring, int>    g_shifts;
static std::wstring                   g_dataPath;

// Copy dialog state
static int  g_copyTargetYear = 0;
static bool g_copyMonths[12] = {};
static bool g_copyOverwrite = false;

// Layout rects
static RECT g_prevBtn = {0}, g_nextBtn = {0}, g_todayBtn = {0};
static RECT g_copyBtn = {0}, g_clearMonthBtn = {0};
static int  g_gridTop = 0, g_gridLeft = 0;
static int  g_cellW = 0, g_cellH = 0;
static int  g_gridStartDow = 0;
static int  g_daysInView = 0;

// ============================================================================
//  UTILITY FUNCTIONS
// ============================================================================

static int DaysInMonth(int m, int y) {
    static const int d[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    if (m < 1 || m > 12) return 30;
    int days = d[m - 1];
    if (m == 2 && ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0)) days = 29;
    return days;
}

static int DayOfWeek(int d, int m, int y) {
    struct tm t = {};
    t.tm_year = y - 1900;
    t.tm_mon = m - 1;
    t.tm_mday = d;
    mktime(&t);
    return (t.tm_wday + 6) % 7;
}

static std::wstring DateKey(int d, int m, int y) {
    wchar_t buf[20];
    wsprintfW(buf, L"%04d-%02d-%02d", y, m, d);
    return buf;
}

static ShiftType GetShift(int d, int m, int y) {
    auto it = g_shifts.find(DateKey(d, m, y));
    if (it != g_shifts.end()) return (ShiftType)it->second;
    return SHIFT_NONE;
}

static void SetShift(int d, int m, int y, ShiftType st) {
    std::wstring key = DateKey(d, m, y);
    if (st == SHIFT_NONE)
        g_shifts.erase(key);
    else
        g_shifts[key] = (int)st;
}

// ============================================================================
//  DATA PERSISTENCE
// ============================================================================

static void GetDataPath() {
    wchar_t path[MAX_PATH] = {0};
    GetModuleFileNameW(NULL, path, MAX_PATH);
    wchar_t* slash = wcsrchr(path, L'\\');
    if (slash) *(slash + 1) = 0;
    wcscat(path, L"smjene_data.txt");
    g_dataPath = path;
}

static void LoadData() {
    g_shifts.clear();
    std::wifstream f(g_dataPath);
    if (!f.is_open()) return;
    std::wstring line;
    while (std::getline(f, line)) {
        if (line.size() < 12) continue;
        std::wstring key = line.substr(0, 10);
        int val = _wtoi(line.substr(11).c_str());
        if (val >= 1 && val <= 3)
            g_shifts[key] = val;
    }
    f.close();
}

static void SaveData() {
    std::wofstream f(g_dataPath);
    if (!f.is_open()) return;
    for (auto& p : g_shifts) {
        f << p.first << L" " << p.second << L"\n";
    }
    f.close();
}

// ============================================================================
//  COPY / CLEAR MONTH LOGIC
// ============================================================================

static int CountShiftsInMonth(int m, int y) {
    int count = 0;
    int days = DaysInMonth(m, y);
    for (int d = 1; d <= days; d++) {
        if (GetShift(d, m, y) != SHIFT_NONE) count++;
    }
    return count;
}

static void CopyMonthPattern(int srcMonth, int srcYear, int dstMonth, int dstYear, bool overwrite) {
    int srcDays = DaysInMonth(srcMonth, srcYear);
    int dstDays = DaysInMonth(dstMonth, dstYear);
    int copyDays = (srcDays < dstDays) ? srcDays : dstDays;

    for (int d = 1; d <= copyDays; d++) {
        ShiftType srcShift = GetShift(d, srcMonth, srcYear);
        if (srcShift == SHIFT_NONE) continue;

        ShiftType dstShift = GetShift(d, dstMonth, dstYear);
        if (dstShift != SHIFT_NONE && !overwrite) continue;

        SetShift(d, dstMonth, dstYear, srcShift);
    }
}

static void ClearMonth() {
    int count = CountShiftsInMonth(g_viewMonth, g_viewYear);
    if (count == 0) {
        MessageBoxW(g_hWnd, L"Ovaj mjesec nema unesenih smjena.", L"Info", MB_OK | MB_ICONINFORMATION);
        return;
    }

    wchar_t msg[200];
    wsprintfW(msg, L"Obrisati sve smjene iz %s %d?\n\n(%d smjena ce biti obrisano)\n\nOva akcija se ne moze ponistiti!",
        MONTH_NAMES[g_viewMonth - 1], g_viewYear, count);

    if (MessageBoxW(g_hWnd, msg, L"Brisanje mjeseca", MB_YESNO | MB_ICONWARNING) == IDYES) {
        int days = DaysInMonth(g_viewMonth, g_viewYear);
        for (int d = 1; d <= days; d++) {
            SetShift(d, g_viewMonth, g_viewYear, SHIFT_NONE);
        }
        SaveData();
        InvalidateRect(g_hWnd, NULL, FALSE);
    }
}

// ============================================================================
//  COPY DIALOG (manual window creation - no .rc file needed)
// ============================================================================

static HFONT CreateDlgFont(int size, bool bold = false) {
    return CreateFontW(size, 0, 0, 0, bold ? FW_BOLD : FW_NORMAL,
        FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
}

static void ShowCopyDialog() {
    int srcCount = CountShiftsInMonth(g_viewMonth, g_viewYear);
    if (srcCount == 0) {
        MessageBoxW(g_hWnd,
            L"Trenutni mjesec nema unesenih smjena!\n\nNajprije unesite raspored za ovaj mjesec,\npa ga onda kopirajte na ostale.",
            L"Nema podataka", MB_OK | MB_ICONWARNING);
        return;
    }

    int dlgW = 340, dlgH = 380;

    HWND hDlg = CreateWindowExW(
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        L"#32770",
        L"Kopiraj Raspored Na Druge Mjesece",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_MODALFRAME,
        0, 0, dlgW, dlgH,
        g_hWnd, NULL, GetModuleHandle(NULL), NULL);

    if (!hDlg) return;

    HFONT hFont = CreateDlgFont(15);
    HFONT hFontBold = CreateDlgFont(15, true);
    HFONT hFontSmall = CreateDlgFont(13);

    int y = 10, x = 15;

    // Source info label
    wchar_t srcLabel[100];
    wsprintfW(srcLabel, L"Izvor: %s %d (%d smjena)", MONTH_NAMES[g_viewMonth - 1], g_viewYear, srcCount);
    HWND hSrcLabel = CreateWindowW(L"STATIC", srcLabel,
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        x, y, 300, 20, hDlg, NULL, NULL, NULL);
    SendMessage(hSrcLabel, WM_SETFONT, (WPARAM)hFontBold, TRUE);
    y += 28;

    // Separator
    CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_ETCHEDHORZ,
        x, y, 300, 2, hDlg, NULL, NULL, NULL);
    y += 10;

    // Target year row
    HWND hYearLabel = CreateWindowW(L"STATIC", L"Ciljna godina:",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        x, y + 3, 100, 20, hDlg, NULL, NULL, NULL);
    SendMessage(hYearLabel, WM_SETFONT, (WPARAM)hFont, TRUE);

    wchar_t yearStr[10];
    wsprintfW(yearStr, L"%d", g_viewYear);
    HWND hYearEdit = CreateWindowW(L"EDIT", yearStr,
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_CENTER | ES_READONLY,
        x + 105, y, 65, 24, hDlg, (HMENU)IDC_YEAR_EDIT, NULL, NULL);
    SendMessage(hYearEdit, WM_SETFONT, (WPARAM)hFontBold, TRUE);

    HWND hYearDown = CreateWindowW(L"BUTTON", L"\x25C0",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x + 175, y, 30, 24, hDlg, (HMENU)IDC_YEAR_DOWN, NULL, NULL);
    SendMessage(hYearDown, WM_SETFONT, (WPARAM)hFont, TRUE);

    HWND hYearUp = CreateWindowW(L"BUTTON", L"\x25B6",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x + 210, y, 30, 24, hDlg, (HMENU)IDC_YEAR_UP, NULL, NULL);
    SendMessage(hYearUp, WM_SETFONT, (WPARAM)hFont, TRUE);

    y += 32;

    // Month checkboxes label
    HWND hMonthLabel = CreateWindowW(L"STATIC", L"Odaberite mjesece:",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        x, y, 200, 20, hDlg, NULL, NULL, NULL);
    SendMessage(hMonthLabel, WM_SETFONT, (WPARAM)hFont, TRUE);

    // Select All / None buttons
    HWND hBtnAll = CreateWindowW(L"BUTTON", L"Sve",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x + 200, y - 2, 45, 22, hDlg, (HMENU)IDC_BTN_ALL, NULL, NULL);
    SendMessage(hBtnAll, WM_SETFONT, (WPARAM)hFontSmall, TRUE);

    HWND hBtnNone = CreateWindowW(L"BUTTON", L"Nista",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x + 250, y - 2, 55, 22, hDlg, (HMENU)IDC_BTN_NONE, NULL, NULL);
    SendMessage(hBtnNone, WM_SETFONT, (WPARAM)hFontSmall, TRUE);

    y += 25;

    // Month checkboxes - 3 columns x 4 rows
    int colW = 100;
    int targetYr = g_viewYear;
    for (int i = 0; i < 12; i++) {
        int col = i % 3;
        int row = i / 3;

        wchar_t label[30];
        int existing = CountShiftsInMonth(i + 1, targetYr);
        if (existing > 0)
            wsprintfW(label, L"%s (%d)", MONTH_NAMES_SHORT[i], existing);
        else
            wsprintfW(label, L"%s", MONTH_NAMES_SHORT[i]);

        HWND hChk = CreateWindowW(L"BUTTON", label,
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            x + col * colW, y + row * 22, colW, 20,
            hDlg, (HMENU)(UINT_PTR)(IDC_CHK_JAN + i), NULL, NULL);
        SendMessage(hChk, WM_SETFONT, (WPARAM)hFontSmall, TRUE);

        // Disable source month in same year
        if (i == g_viewMonth - 1) {
            EnableWindow(hChk, FALSE);
        }
    }
    y += 4 * 22 + 10;

    // Overwrite checkbox
    HWND hOverwrite = CreateWindowW(L"BUTTON",
        L"Prepisi postojece smjene",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        x, y, 250, 20, hDlg, (HMENU)IDC_CHK_OVERWRITE, NULL, NULL);
    SendMessage(hOverwrite, WM_SETFONT, (WPARAM)hFont, TRUE);
    y += 28;

    // Note
    HWND hNote = CreateWindowW(L"STATIC",
        L"Raspored se kopira dan-po-dan (1.=1., 2.=2. itd.)",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        x, y, 300, 18, hDlg, NULL, NULL, NULL);
    SendMessage(hNote, WM_SETFONT, (WPARAM)hFontSmall, TRUE);
    y += 25;

    // Separator
    CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_ETCHEDHORZ,
        x, y, 300, 2, hDlg, NULL, NULL, NULL);
    y += 10;

    // Buttons
    HWND hBtnOk = CreateWindowW(L"BUTTON", L"  Kopiraj  ",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_DEFPUSHBUTTON,
        x + 100, y, 90, 30, hDlg, (HMENU)IDC_BTN_OK, NULL, NULL);
    SendMessage(hBtnOk, WM_SETFONT, (WPARAM)hFontBold, TRUE);

    HWND hBtnCancel = CreateWindowW(L"BUTTON", L"Odustani",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x + 200, y, 90, 30, hDlg, (HMENU)IDC_BTN_CANCEL, NULL, NULL);
    SendMessage(hBtnCancel, WM_SETFONT, (WPARAM)hFont, TRUE);

    // Resize dialog to fit content
    RECT rcClient = {0, 0, dlgW, y + 45};
    AdjustWindowRectEx(&rcClient, WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_MODALFRAME,
        FALSE, WS_EX_DLGMODALFRAME);
    int finalW = rcClient.right - rcClient.left;
    int finalH = rcClient.bottom - rcClient.top;

    // Center on parent
    RECT rcParent;
    GetWindowRect(g_hWnd, &rcParent);
    int px = rcParent.left + (rcParent.right - rcParent.left - finalW) / 2;
    int py = rcParent.top + (rcParent.bottom - rcParent.top - finalH) / 2;
    SetWindowPos(hDlg, HWND_TOP, px, py, finalW, finalH, SWP_NOZORDER);

    ShowWindow(hDlg, SW_SHOW);
    EnableWindow(g_hWnd, FALSE);

    g_copyTargetYear = g_viewYear;

    // Modal message loop
    MSG msg;
    bool running = true;
    while (running && GetMessageW(&msg, NULL, 0, 0)) {
        if (msg.hwnd == hDlg || IsChild(hDlg, msg.hwnd)) {
            if (msg.message == WM_COMMAND) {
                int id = LOWORD(msg.wParam);

                if (id == IDC_YEAR_UP || id == IDC_YEAR_DOWN) {
                    wchar_t ys[10];
                    GetDlgItemTextW(hDlg, IDC_YEAR_EDIT, ys, 10);
                    int yr = _wtoi(ys);
                    if (id == IDC_YEAR_UP) yr++; else yr--;
                    if (yr < 2020) yr = 2020;
                    if (yr > 2040) yr = 2040;
                    g_copyTargetYear = yr;
                    wsprintfW(ys, L"%d", yr);
                    SetDlgItemTextW(hDlg, IDC_YEAR_EDIT, ys);

                    // Update month labels with shift counts for new year
                    for (int i = 0; i < 12; i++) {
                        wchar_t lb[30];
                        int ex = CountShiftsInMonth(i + 1, yr);
                        if (ex > 0)
                            wsprintfW(lb, L"%s (%d)", MONTH_NAMES_SHORT[i], ex);
                        else
                            wsprintfW(lb, L"%s", MONTH_NAMES_SHORT[i]);
                        SetDlgItemTextW(hDlg, IDC_CHK_JAN + i, lb);

                        bool isSrc = (i == g_viewMonth - 1 && yr == g_viewYear);
                        EnableWindow(GetDlgItem(hDlg, IDC_CHK_JAN + i), !isSrc);
                        if (isSrc)
                            CheckDlgButton(hDlg, IDC_CHK_JAN + i, BST_UNCHECKED);
                    }
                }

                if (id == IDC_BTN_ALL) {
                    for (int i = 0; i < 12; i++) {
                        bool canCheck = !(g_copyTargetYear == g_viewYear && i == g_viewMonth - 1);
                        if (canCheck) CheckDlgButton(hDlg, IDC_CHK_JAN + i, BST_CHECKED);
                    }
                }
                if (id == IDC_BTN_NONE) {
                    for (int i = 0; i < 12; i++)
                        CheckDlgButton(hDlg, IDC_CHK_JAN + i, BST_UNCHECKED);
                }

                if (id == IDC_BTN_OK) {
                    wchar_t ys[10];
                    GetDlgItemTextW(hDlg, IDC_YEAR_EDIT, ys, 10);
                    g_copyTargetYear = _wtoi(ys);

                    int checkedCount = 0;
                    for (int i = 0; i < 12; i++) {
                        g_copyMonths[i] = (IsDlgButtonChecked(hDlg, IDC_CHK_JAN + i) == BST_CHECKED);
                        if (g_copyMonths[i]) checkedCount++;
                    }

                    if (checkedCount == 0) {
                        MessageBoxW(hDlg, L"Niste odabrali nijedan mjesec!", L"Greska", MB_OK | MB_ICONWARNING);
                        goto skip_cmd;
                    }

                    g_copyOverwrite = (IsDlgButtonChecked(hDlg, IDC_CHK_OVERWRITE) == BST_CHECKED);

                    wchar_t confirmMsg[512];
                    wsprintfW(confirmMsg,
                        L"Kopiram raspored iz %s %d (%d smjena)\nna %d odabranih mjeseci u %d. godini.\n\n%s\n\nNastaviti?",
                        MONTH_NAMES[g_viewMonth - 1], g_viewYear, srcCount,
                        checkedCount, g_copyTargetYear,
                        g_copyOverwrite ? L"Postojece smjene CE biti prepisane!" :
                                          L"Postojece smjene NECE biti prepisane.");

                    if (MessageBoxW(hDlg, confirmMsg, L"Potvrda kopiranja", MB_YESNO | MB_ICONQUESTION) == IDYES) {
                        for (int i = 0; i < 12; i++) {
                            if (g_copyMonths[i]) {
                                CopyMonthPattern(g_viewMonth, g_viewYear,
                                    i + 1, g_copyTargetYear, g_copyOverwrite);
                            }
                        }
                        SaveData();

                        wchar_t doneMsg[100];
                        wsprintfW(doneMsg, L"Raspored uspjesno kopiran na %d mjeseci!", checkedCount);
                        MessageBoxW(hDlg, doneMsg, L"Gotovo", MB_OK | MB_ICONINFORMATION);
                        running = false;
                    }
                }

                if (id == IDC_BTN_CANCEL || id == IDCANCEL) {
                    running = false;
                }
            }
        }

        skip_cmd:
        if (!IsDialogMessageW(hDlg, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        if (!IsWindow(hDlg)) running = false;
    }

    EnableWindow(g_hWnd, TRUE);
    SetForegroundWindow(g_hWnd);
    if (IsWindow(hDlg)) DestroyWindow(hDlg);
    DeleteObject(hFont);
    DeleteObject(hFontBold);
    DeleteObject(hFontSmall);
    InvalidateRect(g_hWnd, NULL, FALSE);
}

// ============================================================================
//  GDI+ DRAWING HELPERS
// ============================================================================

static GraphicsPath* RoundRect(float x, float y, float w, float h, float r) {
    GraphicsPath* p = new GraphicsPath();
    if (r < 1) { p->AddRectangle(RectF(x, y, w, h)); return p; }
    float d = r * 2;
    p->AddArc(x, y, d, d, 180, 90);
    p->AddArc(x + w - d, y, d, d, 270, 90);
    p->AddArc(x + w - d, y + h - d, d, d, 0, 90);
    p->AddArc(x, y + h - d, d, d, 90, 90);
    p->CloseFigure();
    return p;
}

static void FillRoundRect(Graphics& g, Brush* brush, float x, float y, float w, float h, float r) {
    GraphicsPath* p = RoundRect(x, y, w, h, r);
    g.FillPath(brush, p);
    delete p;
}

static void DrawRoundRect(Graphics& g, Pen* pen, float x, float y, float w, float h, float r) {
    GraphicsPath* p = RoundRect(x, y, w, h, r);
    g.DrawPath(pen, p);
    delete p;
}

// ============================================================================
//  MAIN DRAWING
// ============================================================================

static void DrawCalendar(HDC hdc, int W, int H) {
    Graphics g(hdc);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);

    // Background gradient
    LinearGradientBrush bgBrush(Point(0, 0), Point(0, H), CLR_BG_TOP, CLR_BG_BOT);
    g.FillRectangle(&bgBrush, 0, 0, W, H);

    // Header
    SolidBrush headerBr(CLR_HEADER);
    g.FillRectangle(&headerBr, 0, 0, W, HEADER_H);
    Pen sepPen(CLR_SEPARATOR, 1.0f);
    g.DrawLine(&sepPen, 0, HEADER_H, W, HEADER_H);

    // Fonts
    FontFamily ff(L"Segoe UI");
    Font fontTitle(&ff, 22, FontStyleBold, UnitPixel);
    Font fontBtn(&ff, 18, FontStyleBold, UnitPixel);
    Font fontBtnSmall(&ff, 11, FontStyleBold, UnitPixel);
    Font fontDay(&ff, 12, FontStyleBold, UnitPixel);
    Font fontCellNum(&ff, 14, FontStyleBold, UnitPixel);
    Font fontCellShift(&ff, 11, FontStyleRegular, UnitPixel);
    Font fontLegend(&ff, 12, FontStyleRegular, UnitPixel);
    Font fontStats(&ff, 13, FontStyleRegular, UnitPixel);

    SolidBrush textBr(CLR_TEXT);
    SolidBrush dimBr(CLR_TEXT_DIM);

    int btnY = 16, btnH = 42, btnR = 8;

    // === Prev button ===
    int prevX = 15, prevW = 42;
    g_prevBtn = {prevX, btnY, prevX + prevW, btnY + btnH};
    {
        SolidBrush br(g_hoverBtn == 0 ? CLR_BTN_HOVER : CLR_BTN);
        FillRoundRect(g, &br, (float)prevX, (float)btnY, (float)prevW, (float)btnH, (float)btnR);
        StringFormat sf; sf.SetAlignment(StringAlignmentCenter); sf.SetLineAlignment(StringAlignmentCenter);
        RectF r((float)prevX, (float)btnY, (float)prevW, (float)btnH);
        g.DrawString(L"\x25C0", 1, &fontBtn, r, &sf, &textBr);
    }

    // === Next button ===
    int nextX = prevX + prevW + 6, nextW = 42;
    g_nextBtn = {nextX, btnY, nextX + nextW, btnY + btnH};
    {
        SolidBrush br(g_hoverBtn == 1 ? CLR_BTN_HOVER : CLR_BTN);
        FillRoundRect(g, &br, (float)nextX, (float)btnY, (float)nextW, (float)btnH, (float)btnR);
        StringFormat sf; sf.SetAlignment(StringAlignmentCenter); sf.SetLineAlignment(StringAlignmentCenter);
        RectF r((float)nextX, (float)btnY, (float)nextW, (float)btnH);
        g.DrawString(L"\x25B6", 1, &fontBtn, r, &sf, &textBr);
    }

    // === Right side buttons ===
    // Clear month button
    int clrW = 65, clrX = W - clrW - 15;
    g_clearMonthBtn = {clrX, btnY, clrX + clrW, btnY + btnH};
    {
        SolidBrush br(g_hoverBtn == 4 ? CLR_BTN_CLEARMONTH_HOVER : CLR_BTN_CLEARMONTH);
        FillRoundRect(g, &br, (float)clrX, (float)btnY, (float)clrW, (float)btnH, (float)btnR);
        StringFormat sf; sf.SetAlignment(StringAlignmentCenter); sf.SetLineAlignment(StringAlignmentCenter);
        RectF r((float)clrX, (float)btnY, (float)clrW, (float)btnH);
        g.DrawString(L"\x2716 Brisi", 7, &fontBtnSmall, r, &sf, &textBr);
    }

    // Copy button
    int copyW = 80, copyX = clrX - copyW - 8;
    g_copyBtn = {copyX, btnY, copyX + copyW, btnY + btnH};
    {
        SolidBrush br(g_hoverBtn == 3 ? CLR_BTN_COPY_HOVER : CLR_BTN_COPY);
        FillRoundRect(g, &br, (float)copyX, (float)btnY, (float)copyW, (float)btnH, (float)btnR);
        StringFormat sf; sf.SetAlignment(StringAlignmentCenter); sf.SetLineAlignment(StringAlignmentCenter);
        RectF r((float)copyX, (float)btnY, (float)copyW, (float)btnH);
        g.DrawString(L"\x2398 Kopiraj", 8, &fontBtnSmall, r, &sf, &textBr);
    }

    // Danas button
    int danasW = 72, danasX = copyX - danasW - 8;
    g_todayBtn = {danasX, btnY, danasX + danasW, btnY + btnH};
    {
        SolidBrush br(g_hoverBtn == 2 ? CLR_BTN_HOVER : CLR_BTN);
        FillRoundRect(g, &br, (float)danasX, (float)btnY, (float)danasW, (float)btnH, (float)btnR);
        StringFormat sf; sf.SetAlignment(StringAlignmentCenter); sf.SetLineAlignment(StringAlignmentCenter);
        RectF r((float)danasX, (float)btnY, (float)danasW, (float)btnH);
        g.DrawString(L"DANAS", 5, &fontBtnSmall, r, &sf, &textBr);
    }

    // === Title: Month Year ===
    {
        wchar_t title[100];
        wsprintfW(title, L"%s %d", MONTH_NAMES[g_viewMonth - 1], g_viewYear);
        StringFormat sf; sf.SetAlignment(StringAlignmentCenter); sf.SetLineAlignment(StringAlignmentCenter);
        float titleLeft = (float)(nextX + nextW + 10);
        float titleRight = (float)(danasX - 10);
        RectF r(titleLeft, (float)btnY, titleRight - titleLeft, (float)btnH);
        g.DrawString(title, (int)wcslen(title), &fontTitle, r, &sf, &textBr);
    }

    // === Day Names ===
    int dayNamesTop = HEADER_H + 5;
    g_cellW = (W - 20) / 7;
    g_gridLeft = (W - g_cellW * 7) / 2;

    for (int i = 0; i < 7; i++) {
        float fx = (float)(g_gridLeft + i * g_cellW);
        StringFormat sf; sf.SetAlignment(StringAlignmentCenter); sf.SetLineAlignment(StringAlignmentCenter);
        RectF r(fx, (float)dayNamesTop, (float)g_cellW, (float)DAYNAMES_H);
        SolidBrush* br = (i >= 5) ? new SolidBrush(CLR_TEXT_WEEKEND) : new SolidBrush(CLR_TEXT_DIM);
        g.DrawString(DAY_NAMES[i], (int)wcslen(DAY_NAMES[i]), &fontDay, r, &sf, br);
        delete br;
    }

    int sepY2 = dayNamesTop + DAYNAMES_H;
    Pen sepPen2(CLR_GRID_LINE, 1.0f);
    g.DrawLine(&sepPen2, g_gridLeft, sepY2, g_gridLeft + g_cellW * 7, sepY2);

    // === Calendar Grid ===
    g_gridTop = sepY2 + 4;
    int gridBottom = H - LEGEND_H - STATS_H;
    g_cellH = (gridBottom - g_gridTop) / 6;

    g_gridStartDow = DayOfWeek(1, g_viewMonth, g_viewYear);
    g_daysInView = DaysInMonth(g_viewMonth, g_viewYear);

    for (int day = 1; day <= g_daysInView; day++) {
        int idx = (day - 1) + g_gridStartDow;
        int row = idx / 7;
        int col = idx % 7;

        float cx = (float)(g_gridLeft + col * g_cellW + CELL_PAD);
        float cy = (float)(g_gridTop + row * g_cellH + CELL_PAD);
        float cw = (float)(g_cellW - CELL_PAD * 2);
        float ch = (float)(g_cellH - CELL_PAD * 2);

        ShiftType st = GetShift(day, g_viewMonth, g_viewYear);
        bool isToday = (day == g_todayDay && g_viewMonth == g_todayMonth && g_viewYear == g_todayYear);
        bool isHover = (day == g_hoverDay);
        bool isWeekend = (col >= 5);

        Color cellBg = isHover ? CLR_CELL_HOVER : CLR_CELL_BG;
        if (st == SHIFT_DAY)   cellBg = CLR_DAY_SHIFT_BG;
        if (st == SHIFT_NIGHT) cellBg = CLR_NIGHT_SHIFT_BG;
        if (st == SHIFT_FREE)  cellBg = CLR_FREE_DAY_BG;

        SolidBrush cellBr(cellBg);
        FillRoundRect(g, &cellBr, cx, cy, cw, ch, 8.0f);

        if (isToday) {
            Pen todayPen(CLR_CELL_TODAY_BORDER, 2.5f);
            DrawRoundRect(g, &todayPen, cx, cy, cw, ch, 8.0f);
        }

        // Accent bar at top of cell
        if (st != SHIFT_NONE) {
            Color accentClr = CLR_DAY_SHIFT;
            if (st == SHIFT_NIGHT) accentClr = CLR_NIGHT_SHIFT;
            if (st == SHIFT_FREE)  accentClr = CLR_FREE_DAY;

            GraphicsPath* barPath = new GraphicsPath();
            float br2 = 8.0f;
            barPath->AddArc(cx, cy, br2 * 2, br2 * 2, 180, 90);
            barPath->AddArc(cx + cw - br2 * 2, cy, br2 * 2, br2 * 2, 270, 90);
            barPath->AddLine(cx + cw, cy + br2, cx + cw, cy + 5);
            barPath->AddLine(cx + cw, cy + 5, cx, cy + 5);
            barPath->AddLine(cx, cy + 5, cx, cy + br2);
            barPath->CloseFigure();
            SolidBrush accentBr(accentClr);
            g.FillPath(&accentBr, barPath);
            delete barPath;
        }

        // Day number
        wchar_t dayStr[4];
        wsprintfW(dayStr, L"%d", day);
        StringFormat sfNum; sfNum.SetAlignment(StringAlignmentNear); sfNum.SetLineAlignment(StringAlignmentNear);
        RectF numRect(cx + 8, cy + 8, cw - 16, 20);
        Color numClr = isWeekend ? CLR_TEXT_WEEKEND : CLR_TEXT;
        if (isToday) numClr = CLR_CELL_TODAY_BORDER;
        SolidBrush numBr(numClr);
        g.DrawString(dayStr, (int)wcslen(dayStr), &fontCellNum, numRect, &sfNum, &numBr);

        // "DANAS" label
        if (isToday) {
            StringFormat sfT; sfT.SetAlignment(StringAlignmentFar); sfT.SetLineAlignment(StringAlignmentNear);
            RectF tRect(cx + 8, cy + 8, cw - 16, 18);
            Font fontTiny(&ff, 9, FontStyleBold, UnitPixel);
            SolidBrush tBr(CLR_CELL_TODAY_BORDER);
            g.DrawString(L"DANAS", 5, &fontTiny, tRect, &sfT, &tBr);
        }

        // Shift label
        if (st != SHIFT_NONE) {
            const wchar_t* label = L"";
            Color labelClr = CLR_TEXT;
            if (st == SHIFT_DAY)   { label = L"\x2600 DNEVNA";  labelClr = CLR_DAY_SHIFT; }
            if (st == SHIFT_NIGHT) { label = L"\x263E NOCNA";    labelClr = CLR_NIGHT_SHIFT; }
            if (st == SHIFT_FREE)  { label = L"\x2714 SLOBODAN"; labelClr = CLR_FREE_DAY; }

            StringFormat sfS; sfS.SetAlignment(StringAlignmentCenter); sfS.SetLineAlignment(StringAlignmentCenter);
            RectF sRect(cx, cy + ch * 0.35f, cw, ch * 0.55f);
            SolidBrush sBr(labelClr);
            g.DrawString(label, (int)wcslen(label), &fontCellShift, sRect, &sfS, &sBr);
        }
    }

    // === Stats ===
    int statsTop = gridBottom + 2;
    int dayCount = 0, nightCount = 0, freeCount = 0;
    for (int d = 1; d <= g_daysInView; d++) {
        ShiftType s = GetShift(d, g_viewMonth, g_viewYear);
        if (s == SHIFT_DAY) dayCount++;
        if (s == SHIFT_NIGHT) nightCount++;
        if (s == SHIFT_FREE) freeCount++;
    }
    {
        wchar_t stats[200];
        wsprintfW(stats, L"Ovaj mjesec:   \x2600 Dnevnih: %d   |   \x263E Nocnih: %d   |   \x2714 Slobodnih: %d   |   Ukupno radnih: %d",
                  dayCount, nightCount, freeCount, dayCount + nightCount);
        StringFormat sf; sf.SetAlignment(StringAlignmentCenter); sf.SetLineAlignment(StringAlignmentCenter);
        RectF r((float)g_gridLeft, (float)statsTop, (float)(g_cellW * 7), (float)STATS_H);
        g.DrawString(stats, (int)wcslen(stats), &fontStats, r, &sf, &dimBr);
    }
    g.DrawLine(&sepPen2, g_gridLeft, statsTop + STATS_H, g_gridLeft + g_cellW * 7, statsTop + STATS_H);

    // === Legend ===
    int legTop = statsTop + STATS_H + 5;
    int legCenterY = legTop + (LEGEND_H - 10) / 2;
    int dotSize = 16;
    int spacing = 20;

    struct LI { Color clr; const wchar_t* text; };
    LI items[] = {
        { CLR_DAY_SHIFT,   L"Dnevna smjena" },
        { CLR_NIGHT_SHIFT, L"Nocna smjena" },
        { CLR_FREE_DAY,    L"Slobodan dan" },
    };

    int totalLegW = 0;
    for (int i = 0; i < 3; i++) {
        RectF bounds;
        g.MeasureString(items[i].text, (int)wcslen(items[i].text), &fontLegend, PointF(0, 0), &bounds);
        totalLegW += dotSize + 8 + (int)bounds.Width + spacing;
    }
    totalLegW -= spacing;

    int lx = (W - totalLegW) / 2;
    for (int i = 0; i < 3; i++) {
        SolidBrush dotBr(items[i].clr);
        FillRoundRect(g, &dotBr, (float)lx, (float)(legCenterY - dotSize / 2), (float)dotSize, (float)dotSize, 4.0f);
        lx += dotSize + 8;
        RectF bounds;
        g.MeasureString(items[i].text, (int)wcslen(items[i].text), &fontLegend, PointF(0, 0), &bounds);
        RectF textR((float)lx, (float)(legCenterY - bounds.Height / 2), bounds.Width, bounds.Height);
        g.DrawString(items[i].text, (int)wcslen(items[i].text), &fontLegend, textR, nullptr, &dimBr);
        lx += (int)bounds.Width + spacing;
    }

    {
        const wchar_t* instr = L"Lijevi klik = postavi smjenu   |   Desni klik = obrisi   |   Scroll = promijeni mjesec";
        StringFormat sf; sf.SetAlignment(StringAlignmentCenter);
        RectF r(0, (float)(legCenterY + dotSize / 2 + 6), (float)W, 20);
        Font fontInstr(&ff, 10, FontStyleItalic, UnitPixel);
        SolidBrush instrBr(Color(255, 70, 72, 100));
        g.DrawString(instr, (int)wcslen(instr), &fontInstr, r, &sf, &instrBr);
    }
}

// ============================================================================
//  HIT TESTING
// ============================================================================

static int HitTestDay(int mx, int my) {
    if (my < g_gridTop || g_cellW == 0 || g_cellH == 0) return -1;
    int col = (mx - g_gridLeft) / g_cellW;
    int row = (my - g_gridTop) / g_cellH;
    if (col < 0 || col > 6 || row < 0 || row > 5) return -1;
    int day = row * 7 + col - g_gridStartDow + 1;
    if (day < 1 || day > g_daysInView) return -1;
    return day;
}

static int HitTestButton(int mx, int my) {
    POINT pt = {mx, my};
    if (PtInRect(&g_prevBtn, pt))       return 0;
    if (PtInRect(&g_nextBtn, pt))       return 1;
    if (PtInRect(&g_todayBtn, pt))      return 2;
    if (PtInRect(&g_copyBtn, pt))       return 3;
    if (PtInRect(&g_clearMonthBtn, pt)) return 4;
    return -1;
}

// ============================================================================
//  NAVIGATION
// ============================================================================

static void GoToPrevMonth() {
    g_viewMonth--;
    if (g_viewMonth < 1) { g_viewMonth = 12; g_viewYear--; }
    InvalidateRect(g_hWnd, NULL, FALSE);
}
static void GoToNextMonth() {
    g_viewMonth++;
    if (g_viewMonth > 12) { g_viewMonth = 1; g_viewYear++; }
    InvalidateRect(g_hWnd, NULL, FALSE);
}
static void GoToToday() {
    g_viewMonth = g_todayMonth;
    g_viewYear = g_todayYear;
    InvalidateRect(g_hWnd, NULL, FALSE);
}

// ============================================================================
//  CONTEXT MENU (shift selection)
// ============================================================================

static void ShowShiftMenu(int day, int x, int y) {
    HMENU hMenu = CreatePopupMenu();
    ShiftType current = GetShift(day, g_viewMonth, g_viewYear);

    UINT fD = MF_STRING | (current == SHIFT_DAY   ? MF_CHECKED : 0);
    UINT fN = MF_STRING | (current == SHIFT_NIGHT ? MF_CHECKED : 0);
    UINT fF = MF_STRING | (current == SHIFT_FREE  ? MF_CHECKED : 0);

    AppendMenuW(hMenu, fD,           IDM_DAY_SHIFT,   L"  \x2600  Dnevna Smjena");
    AppendMenuW(hMenu, fN,           IDM_NIGHT_SHIFT,  L"  \x263E  Nocna Smjena");
    AppendMenuW(hMenu, fF,           IDM_FREE_DAY,     L"  \x2714  Slobodan Dan");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING,    IDM_CLEAR,        L"  \x2716  Obrisi");

    POINT pt = {x, y};
    ClientToScreen(g_hWnd, &pt);
    int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, 0, g_hWnd, NULL);

    if (cmd == IDM_DAY_SHIFT)   SetShift(day, g_viewMonth, g_viewYear, SHIFT_DAY);
    if (cmd == IDM_NIGHT_SHIFT) SetShift(day, g_viewMonth, g_viewYear, SHIFT_NIGHT);
    if (cmd == IDM_FREE_DAY)    SetShift(day, g_viewMonth, g_viewYear, SHIFT_FREE);
    if (cmd == IDM_CLEAR)       SetShift(day, g_viewMonth, g_viewYear, SHIFT_NONE);

    if (cmd) { SaveData(); InvalidateRect(g_hWnd, NULL, FALSE); }
    DestroyMenu(hMenu);
}

// ============================================================================
//  WINDOW PROCEDURE
// ============================================================================

static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        g_hWnd = hWnd;
        return 0;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        RECT rc; GetClientRect(hWnd, &rc);
        int W = rc.right, H = rc.bottom;

        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP memBM = CreateCompatibleBitmap(hdc, W, H);
        HBITMAP oldBM = (HBITMAP)SelectObject(memDC, memBM);
        DrawCalendar(memDC, W, H);
        BitBlt(hdc, 0, 0, W, H, memDC, 0, 0, SRCCOPY);
        SelectObject(memDC, oldBM);
        DeleteObject(memBM);
        DeleteDC(memDC);
        EndPaint(hWnd, &ps);
        return 0;
    }

    case WM_ERASEBKGND: return 1;
    case WM_SIZE: InvalidateRect(hWnd, NULL, FALSE); return 0;

    case WM_GETMINMAXINFO: {
        MINMAXINFO* mmi = (MINMAXINFO*)lParam;
        mmi->ptMinTrackSize.x = MIN_W;
        mmi->ptMinTrackSize.y = MIN_H;
        return 0;
    }

    case WM_MOUSEMOVE: {
        int mx = (int)(short)LOWORD(lParam);
        int my = (int)(short)HIWORD(lParam);
        int newHover = HitTestDay(mx, my);
        int newBtn = HitTestButton(mx, my);
        if (newHover != g_hoverDay || newBtn != g_hoverBtn) {
            g_hoverDay = newHover;
            g_hoverBtn = newBtn;
            InvalidateRect(hWnd, NULL, FALSE);
            SetCursor(LoadCursor(NULL, (newHover > 0 || newBtn >= 0) ? IDC_HAND : IDC_ARROW));
        }
        TRACKMOUSEEVENT tme = {sizeof(tme), TME_LEAVE, hWnd, 0};
        TrackMouseEvent(&tme);
        return 0;
    }

    case WM_MOUSELEAVE:
        g_hoverDay = -1; g_hoverBtn = -1;
        InvalidateRect(hWnd, NULL, FALSE);
        return 0;

    case WM_LBUTTONDOWN: {
        int mx = (int)(short)LOWORD(lParam);
        int my = (int)(short)HIWORD(lParam);
        int btn = HitTestButton(mx, my);
        if (btn == 0) { GoToPrevMonth(); return 0; }
        if (btn == 1) { GoToNextMonth(); return 0; }
        if (btn == 2) { GoToToday();     return 0; }
        if (btn == 3) { ShowCopyDialog(); return 0; }
        if (btn == 4) { ClearMonth();     return 0; }
        int day = HitTestDay(mx, my);
        if (day > 0) ShowShiftMenu(day, mx, my);
        return 0;
    }

    case WM_RBUTTONDOWN: {
        int mx = (int)(short)LOWORD(lParam);
        int my = (int)(short)HIWORD(lParam);
        int day = HitTestDay(mx, my);
        if (day > 0) {
            if (GetShift(day, g_viewMonth, g_viewYear) != SHIFT_NONE) {
                SetShift(day, g_viewMonth, g_viewYear, SHIFT_NONE);
                SaveData(); InvalidateRect(hWnd, NULL, FALSE);
            }
        }
        return 0;
    }

    case WM_MOUSEWHEEL: {
        int delta = GET_WHEEL_DELTA_WPARAM(wParam);
        if (delta > 0) GoToPrevMonth(); else GoToNextMonth();
        return 0;
    }

    case WM_KEYDOWN:
        if (wParam == VK_LEFT)  { GoToPrevMonth(); return 0; }
        if (wParam == VK_RIGHT) { GoToNextMonth(); return 0; }
        if (wParam == VK_HOME)  { GoToToday();     return 0; }
        return 0;

    case WM_DESTROY:
        SaveData();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

// ============================================================================
//  ENTRY POINT
// ============================================================================

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, int nShow) {
    GdiplusStartupInput gdipSI;
    GdiplusStartup(&g_gdipToken, &gdipSI, NULL);

    INITCOMMONCONTROLSEX icex = {sizeof(icex), ICC_WIN95_CLASSES};
    InitCommonControlsEx(&icex);

    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    g_todayDay   = t->tm_mday;
    g_todayMonth = t->tm_mon + 1;
    g_todayYear  = t->tm_year + 1900;
    g_viewMonth  = g_todayMonth;
    g_viewYear   = g_todayYear;

    GetDataPath();
    LoadData();

    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance      = hInst;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;
    wc.lpszClassName = L"SmjeneKalendarClass";
    wc.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
    wc.hIconSm       = LoadIcon(NULL, IDI_APPLICATION);
    RegisterClassExW(&wc);

    int scrW = GetSystemMetrics(SM_CXSCREEN);
    int scrH = GetSystemMetrics(SM_CYSCREEN);
    int winW = 950, winH = 740;

    HWND hWnd = CreateWindowExW(0, L"SmjeneKalendarClass",
        L"Raspored Smjena \x2014 Kalendar v2.0",
        WS_OVERLAPPEDWINDOW,
        (scrW - winW) / 2, (scrH - winH) / 2, winW, winH,
        NULL, NULL, hInst, NULL);

    if (!hWnd) return 1;
    ShowWindow(hWnd, nShow);
    UpdateWindow(hWnd);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    GdiplusShutdown(g_gdipToken);
    return (int)msg.wParam;
}

#ifndef _MSC_VER
int main() {
    return wWinMain(GetModuleHandle(NULL), NULL, GetCommandLineW(), SW_SHOWDEFAULT);
}
#endif
