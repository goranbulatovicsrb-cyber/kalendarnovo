// ============================================================================
//  RASPORED SMJENA - Kalendar Aplikacija v2.1
//  Opis:  Aplikacija za evidenciju radnih smjena sa kalendarskim prikazom.
//         ISPRAVKA: Copy i Clear dugmad sada rade ispravno.
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
//  ENUMS & IDS
// ============================================================================

enum ShiftType { SHIFT_NONE = 0, SHIFT_DAY = 1, SHIFT_NIGHT = 2, SHIFT_FREE = 3 };

#define IDM_DAY_SHIFT   1001
#define IDM_NIGHT_SHIFT 1002
#define IDM_FREE_DAY    1003
#define IDM_CLEAR       1004

#define IDC_CHK_BASE      2001
#define IDC_YEAR_EDIT     2020
#define IDC_YEAR_UP       2021
#define IDC_YEAR_DOWN     2022
#define IDC_BTN_OK        2030
#define IDC_BTN_CANCEL    2031
#define IDC_BTN_ALL       2032
#define IDC_BTN_NONE      2033
#define IDC_CHK_OVERWRITE 2040

static const int HEADER_H    = 75;
static const int DAYNAMES_H  = 38;
static const int LEGEND_H    = 65;
static const int STATS_H     = 38;
static const int CELL_PAD    = 3;
static const int MIN_W       = 850;
static const int MIN_H       = 680;

// ============================================================================
//  COLORS
// ============================================================================

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

// ============================================================================
//  STRINGS
// ============================================================================

static const wchar_t* MONTH_NAMES[] = {
    L"Januar", L"Februar", L"Mart", L"April",
    L"Maj", L"Juni", L"Juli", L"August",
    L"Septembar", L"Oktobar", L"Novembar", L"Decembar"
};
static const wchar_t* MONTH_NAMES_SHORT[] = {
    L"Jan", L"Feb", L"Mar", L"Apr", L"Maj", L"Jun",
    L"Jul", L"Aug", L"Sep", L"Okt", L"Nov", L"Dec"
};
static const wchar_t* DAY_NAMES[] = {
    L"PON", L"UTO", L"SRI", L"CET", L"PET", L"SUB", L"NED"
};

// ============================================================================
//  GLOBALS
// ============================================================================

static HWND                           g_hWnd = NULL;
static ULONG_PTR                      g_gdipToken = 0;
static int                            g_viewMonth = 0;
static int                            g_viewYear  = 0;
static int                            g_todayDay = 0, g_todayMonth = 0, g_todayYear = 0;
static int                            g_hoverDay = -1;
static int                            g_hoverBtn = -1;
static std::map<std::wstring, int>    g_shifts;
static std::wstring                   g_dataPath;

// Layout rects
static RECT g_prevBtn = {0}, g_nextBtn = {0}, g_todayBtn = {0};
static RECT g_copyBtn = {0}, g_clearMonthBtn = {0};
static int  g_gridTop = 0, g_gridLeft = 0;
static int  g_cellW = 0, g_cellH = 0;
static int  g_gridStartDow = 0;
static int  g_daysInView = 0;

// Copy dialog state
static HWND g_hCopyDlg = NULL;
static int  g_copyTargetYear = 0;
static bool g_dlgResult = false;

// ============================================================================
//  UTILITY
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
    t.tm_year = y - 1900; t.tm_mon = m - 1; t.tm_mday = d;
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
    return (it != g_shifts.end()) ? (ShiftType)it->second : SHIFT_NONE;
}

static void SetShift(int d, int m, int y, ShiftType st) {
    if (st == SHIFT_NONE) g_shifts.erase(DateKey(d, m, y));
    else g_shifts[DateKey(d, m, y)] = (int)st;
}

static int CountShiftsInMonth(int m, int y) {
    int count = 0;
    for (int d = 1; d <= DaysInMonth(m, y); d++)
        if (GetShift(d, m, y) != SHIFT_NONE) count++;
    return count;
}

// ============================================================================
//  PERSISTENCE
// ============================================================================

static void GetDataPath() {
    wchar_t path[MAX_PATH] = {0};
    GetModuleFileNameW(NULL, path, MAX_PATH);
    wchar_t* s = wcsrchr(path, L'\\');
    if (s) *(s + 1) = 0;
    wcscat(path, L"smjene_data.txt");
    g_dataPath = path;
}

static void LoadData() {
    g_shifts.clear();
    FILE* f = _wfopen(g_dataPath.c_str(), L"r");
    if (!f) return;
    char buf[64];
    while (fgets(buf, sizeof(buf), f)) {
        if (strlen(buf) < 12) continue;
        // Parse "YYYY-MM-DD V"
        buf[10] = 0;
        int val = atoi(buf + 11);
        if (val >= 1 && val <= 3) {
            wchar_t wkey[16];
            MultiByteToWideChar(CP_ACP, 0, buf, 10, wkey, 16);
            wkey[10] = 0;
            g_shifts[wkey] = val;
        }
    }
    fclose(f);
}

static void SaveData() {
    FILE* f = _wfopen(g_dataPath.c_str(), L"w");
    if (!f) return;
    for (auto& p : g_shifts) {
        char key[16];
        WideCharToMultiByte(CP_ACP, 0, p.first.c_str(), -1, key, 16, NULL, NULL);
        fprintf(f, "%s %d\n", key, p.second);
    }
    fclose(f);
}

// ============================================================================
//  COPY LOGIC
// ============================================================================

static void CopyMonthPattern(int srcM, int srcY, int dstM, int dstY, bool overwrite) {
    int srcDays = DaysInMonth(srcM, srcY);
    int dstDays = DaysInMonth(dstM, dstY);
    int n = (srcDays < dstDays) ? srcDays : dstDays;
    for (int d = 1; d <= n; d++) {
        ShiftType ss = GetShift(d, srcM, srcY);
        if (ss == SHIFT_NONE) continue;
        if (!overwrite && GetShift(d, dstM, dstY) != SHIFT_NONE) continue;
        SetShift(d, dstM, dstY, ss);
    }
}

// ============================================================================
//  COPY DIALOG - proper WndProc based
// ============================================================================

static void UpdateCopyDlgMonthLabels(HWND hDlg, int yr) {
    for (int i = 0; i < 12; i++) {
        wchar_t lb[40];
        int ex = CountShiftsInMonth(i + 1, yr);
        if (ex > 0) wsprintfW(lb, L"%s (%d)", MONTH_NAMES_SHORT[i], ex);
        else wsprintfW(lb, L"%s", MONTH_NAMES_SHORT[i]);
        SetDlgItemTextW(hDlg, IDC_CHK_BASE + i, lb);
        bool isSrc = (i == g_viewMonth - 1 && yr == g_viewYear);
        EnableWindow(GetDlgItem(hDlg, IDC_CHK_BASE + i), !isSrc);
        if (isSrc) CheckDlgButton(hDlg, IDC_CHK_BASE + i, BST_UNCHECKED);
    }
}

static LRESULT CALLBACK CopyDlgWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_COMMAND: {
        int id = LOWORD(wParam);
        int notif = HIWORD(wParam);

        if (id == IDC_YEAR_UP || id == IDC_YEAR_DOWN) {
            if (notif == BN_CLICKED) {
                wchar_t ys[10];
                GetDlgItemTextW(hWnd, IDC_YEAR_EDIT, ys, 10);
                int yr = _wtoi(ys);
                if (id == IDC_YEAR_UP) yr++; else yr--;
                if (yr < 2020) yr = 2020;
                if (yr > 2040) yr = 2040;
                g_copyTargetYear = yr;
                wsprintfW(ys, L"%d", yr);
                SetDlgItemTextW(hWnd, IDC_YEAR_EDIT, ys);
                UpdateCopyDlgMonthLabels(hWnd, yr);
            }
            return 0;
        }

        if (id == IDC_BTN_ALL && notif == BN_CLICKED) {
            for (int i = 0; i < 12; i++) {
                bool canCheck = !(g_copyTargetYear == g_viewYear && i == g_viewMonth - 1);
                if (canCheck) CheckDlgButton(hWnd, IDC_CHK_BASE + i, BST_CHECKED);
            }
            return 0;
        }

        if (id == IDC_BTN_NONE && notif == BN_CLICKED) {
            for (int i = 0; i < 12; i++)
                CheckDlgButton(hWnd, IDC_CHK_BASE + i, BST_UNCHECKED);
            return 0;
        }

        if (id == IDC_BTN_OK && notif == BN_CLICKED) {
            wchar_t ys[10];
            GetDlgItemTextW(hWnd, IDC_YEAR_EDIT, ys, 10);
            g_copyTargetYear = _wtoi(ys);

            int checkedCount = 0;
            bool months[12] = {};
            for (int i = 0; i < 12; i++) {
                months[i] = (IsDlgButtonChecked(hWnd, IDC_CHK_BASE + i) == BST_CHECKED);
                if (months[i]) checkedCount++;
            }

            if (checkedCount == 0) {
                MessageBoxW(hWnd, L"Niste odabrali nijedan mjesec!", L"Greska", MB_OK | MB_ICONWARNING);
                return 0;
            }

            bool overwrite = (IsDlgButtonChecked(hWnd, IDC_CHK_OVERWRITE) == BST_CHECKED);

            int srcCount = CountShiftsInMonth(g_viewMonth, g_viewYear);
            wchar_t confirmMsg[512];
            wsprintfW(confirmMsg,
                L"Kopiram raspored iz %s %d (%d smjena)\nna %d odabranih mjeseci u %d. godini.\n\n%s\n\nNastaviti?",
                MONTH_NAMES[g_viewMonth - 1], g_viewYear, srcCount,
                checkedCount, g_copyTargetYear,
                overwrite ? L"Postojece smjene CE biti prepisane!" :
                            L"Postojece smjene NECE biti prepisane.");

            if (MessageBoxW(hWnd, confirmMsg, L"Potvrda kopiranja", MB_YESNO | MB_ICONQUESTION) == IDYES) {
                for (int i = 0; i < 12; i++) {
                    if (months[i])
                        CopyMonthPattern(g_viewMonth, g_viewYear, i + 1, g_copyTargetYear, overwrite);
                }
                SaveData();
                wchar_t doneMsg[100];
                wsprintfW(doneMsg, L"Raspored uspjesno kopiran na %d mjeseci!", checkedCount);
                MessageBoxW(hWnd, doneMsg, L"Gotovo", MB_OK | MB_ICONINFORMATION);
                g_dlgResult = true;
                EnableWindow(g_hWnd, TRUE);
                DestroyWindow(hWnd);
            }
            return 0;
        }

        if ((id == IDC_BTN_CANCEL && notif == BN_CLICKED) || id == IDCANCEL) {
            g_dlgResult = false;
            EnableWindow(g_hWnd, TRUE);
            DestroyWindow(hWnd);
            return 0;
        }
        break;
    }

    case WM_CLOSE:
        g_dlgResult = false;
        EnableWindow(g_hWnd, TRUE);
        DestroyWindow(hWnd);
        return 0;

    case WM_DESTROY:
        g_hCopyDlg = NULL;
        SetForegroundWindow(g_hWnd);
        InvalidateRect(g_hWnd, NULL, FALSE);
        return 0;
    }

    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

static void RegisterCopyDlgClass(HINSTANCE hInst) {
    static bool registered = false;
    if (registered) return;
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = CopyDlgWndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
    wc.lpszClassName = L"SmjeneCopyDlgClass";
    RegisterClassExW(&wc);
    registered = true;
}

static HFONT MakeFont(int size, bool bold = false) {
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

    if (g_hCopyDlg && IsWindow(g_hCopyDlg)) {
        SetForegroundWindow(g_hCopyDlg);
        return;
    }

    RegisterCopyDlgClass(GetModuleHandle(NULL));

    g_copyTargetYear = g_viewYear;
    g_dlgResult = false;

    // Calculate dialog size
    int dlgClientW = 330, dlgClientH = 340;
    RECT rcDlg = {0, 0, dlgClientW, dlgClientH};
    AdjustWindowRectEx(&rcDlg, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, FALSE, WS_EX_DLGMODALFRAME);
    int dlgW = rcDlg.right - rcDlg.left;
    int dlgH = rcDlg.bottom - rcDlg.top;

    // Center on parent
    RECT rcP; GetWindowRect(g_hWnd, &rcP);
    int px = rcP.left + (rcP.right - rcP.left - dlgW) / 2;
    int py = rcP.top + (rcP.bottom - rcP.top - dlgH) / 2;

    g_hCopyDlg = CreateWindowExW(
        WS_EX_DLGMODALFRAME,
        L"SmjeneCopyDlgClass",
        L"Kopiraj Raspored Na Druge Mjesece",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        px, py, dlgW, dlgH,
        g_hWnd, NULL, GetModuleHandle(NULL), NULL);

    if (!g_hCopyDlg) return;

    HFONT hFont     = MakeFont(15);
    HFONT hFontBold = MakeFont(15, true);
    HFONT hFontSm   = MakeFont(13);

    int y = 10, x = 15;
    HWND h;

    // Source label
    wchar_t srcLbl[100];
    wsprintfW(srcLbl, L"Izvor: %s %d (%d smjena)", MONTH_NAMES[g_viewMonth - 1], g_viewYear, srcCount);
    h = CreateWindowW(L"STATIC", srcLbl, WS_CHILD | WS_VISIBLE, x, y, 300, 20, g_hCopyDlg, NULL, NULL, NULL);
    SendMessage(h, WM_SETFONT, (WPARAM)hFontBold, TRUE);
    y += 30;

    // Year row
    h = CreateWindowW(L"STATIC", L"Ciljna godina:", WS_CHILD | WS_VISIBLE, x, y+3, 100, 20, g_hCopyDlg, NULL, NULL, NULL);
    SendMessage(h, WM_SETFONT, (WPARAM)hFont, TRUE);

    wchar_t ys[10]; wsprintfW(ys, L"%d", g_viewYear);
    h = CreateWindowW(L"EDIT", ys, WS_CHILD | WS_VISIBLE | WS_BORDER | ES_CENTER | ES_READONLY,
        x+105, y, 65, 24, g_hCopyDlg, (HMENU)IDC_YEAR_EDIT, NULL, NULL);
    SendMessage(h, WM_SETFONT, (WPARAM)hFontBold, TRUE);

    h = CreateWindowW(L"BUTTON", L"<", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x+175, y, 30, 24, g_hCopyDlg, (HMENU)IDC_YEAR_DOWN, NULL, NULL);
    SendMessage(h, WM_SETFONT, (WPARAM)hFont, TRUE);

    h = CreateWindowW(L"BUTTON", L">", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x+210, y, 30, 24, g_hCopyDlg, (HMENU)IDC_YEAR_UP, NULL, NULL);
    SendMessage(h, WM_SETFONT, (WPARAM)hFont, TRUE);
    y += 32;

    // Month label + Sve/Nista
    h = CreateWindowW(L"STATIC", L"Odaberite mjesece:", WS_CHILD | WS_VISIBLE, x, y, 180, 20, g_hCopyDlg, NULL, NULL, NULL);
    SendMessage(h, WM_SETFONT, (WPARAM)hFont, TRUE);

    h = CreateWindowW(L"BUTTON", L"Sve", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x+195, y-2, 50, 22, g_hCopyDlg, (HMENU)IDC_BTN_ALL, NULL, NULL);
    SendMessage(h, WM_SETFONT, (WPARAM)hFontSm, TRUE);

    h = CreateWindowW(L"BUTTON", L"Nista", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x+250, y-2, 55, 22, g_hCopyDlg, (HMENU)IDC_BTN_NONE, NULL, NULL);
    SendMessage(h, WM_SETFONT, (WPARAM)hFontSm, TRUE);
    y += 25;

    // Month checkboxes 3x4
    int colW = 100;
    for (int i = 0; i < 12; i++) {
        int col = i % 3, row = i / 3;
        wchar_t lb[40];
        int ex = CountShiftsInMonth(i + 1, g_viewYear);
        if (ex > 0) wsprintfW(lb, L"%s (%d)", MONTH_NAMES_SHORT[i], ex);
        else wsprintfW(lb, L"%s", MONTH_NAMES_SHORT[i]);

        h = CreateWindowW(L"BUTTON", lb, WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            x + col * colW, y + row * 22, colW, 20,
            g_hCopyDlg, (HMENU)(UINT_PTR)(IDC_CHK_BASE + i), NULL, NULL);
        SendMessage(h, WM_SETFONT, (WPARAM)hFontSm, TRUE);
        if (i == g_viewMonth - 1) EnableWindow(h, FALSE);
    }
    y += 4 * 22 + 10;

    // Overwrite
    h = CreateWindowW(L"BUTTON", L"Prepisi postojece smjene",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        x, y, 280, 20, g_hCopyDlg, (HMENU)IDC_CHK_OVERWRITE, NULL, NULL);
    SendMessage(h, WM_SETFONT, (WPARAM)hFont, TRUE);
    y += 28;

    // Note
    h = CreateWindowW(L"STATIC", L"Raspored se kopira dan-po-dan (1.=1., 2.=2. itd.)",
        WS_CHILD | WS_VISIBLE, x, y, 300, 18, g_hCopyDlg, NULL, NULL, NULL);
    SendMessage(h, WM_SETFONT, (WPARAM)hFontSm, TRUE);
    y += 30;

    // OK / Cancel
    h = CreateWindowW(L"BUTTON", L"KOPIRAJ", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_DEFPUSHBUTTON,
        x+80, y, 90, 32, g_hCopyDlg, (HMENU)IDC_BTN_OK, NULL, NULL);
    SendMessage(h, WM_SETFONT, (WPARAM)hFontBold, TRUE);

    h = CreateWindowW(L"BUTTON", L"Odustani", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x+180, y, 90, 32, g_hCopyDlg, (HMENU)IDC_BTN_CANCEL, NULL, NULL);
    SendMessage(h, WM_SETFONT, (WPARAM)hFont, TRUE);

    // Disable main window (modal behavior)
    EnableWindow(g_hWnd, FALSE);
    ShowWindow(g_hCopyDlg, SW_SHOW);
    UpdateWindow(g_hCopyDlg);
}

// ============================================================================
//  CLEAR MONTH
// ============================================================================

static void ClearMonth() {
    int count = CountShiftsInMonth(g_viewMonth, g_viewYear);
    if (count == 0) {
        MessageBoxW(g_hWnd, L"Ovaj mjesec nema unesenih smjena.", L"Info", MB_OK | MB_ICONINFORMATION);
        return;
    }

    wchar_t msg[256];
    wsprintfW(msg, L"Obrisati sve smjene iz %s %d?\n\n(%d smjena ce biti obrisano)\n\nOva akcija se ne moze ponistiti!",
        MONTH_NAMES[g_viewMonth - 1], g_viewYear, count);

    if (MessageBoxW(g_hWnd, msg, L"Brisanje mjeseca", MB_YESNO | MB_ICONWARNING) == IDYES) {
        int days = DaysInMonth(g_viewMonth, g_viewYear);
        for (int d = 1; d <= days; d++)
            SetShift(d, g_viewMonth, g_viewYear, SHIFT_NONE);
        SaveData();
        InvalidateRect(g_hWnd, NULL, FALSE);
    }
}

// ============================================================================
//  GDI+ HELPERS
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

static void FillRR(Graphics& g, Brush* b, float x, float y, float w, float h, float r) {
    GraphicsPath* p = RoundRect(x, y, w, h, r); g.FillPath(b, p); delete p;
}
static void DrawRR(Graphics& g, Pen* pen, float x, float y, float w, float h, float r) {
    GraphicsPath* p = RoundRect(x, y, w, h, r); g.DrawPath(pen, p); delete p;
}

// ============================================================================
//  MAIN DRAWING
// ============================================================================

static void DrawCalendar(HDC hdc, int W, int H) {
    Graphics g(hdc);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);

    // BG
    LinearGradientBrush bgBr(Point(0,0), Point(0,H), CLR_BG_TOP, CLR_BG_BOT);
    g.FillRectangle(&bgBr, 0, 0, W, H);

    // Header
    SolidBrush hdrBr(CLR_HEADER);
    g.FillRectangle(&hdrBr, 0, 0, W, HEADER_H);
    Pen sepPen(CLR_SEPARATOR, 1.0f);
    g.DrawLine(&sepPen, 0, HEADER_H, W, HEADER_H);

    FontFamily ff(L"Segoe UI");
    Font fTitle(&ff, 22, FontStyleBold, UnitPixel);
    Font fBtn(&ff, 18, FontStyleBold, UnitPixel);
    Font fBtnSm(&ff, 11, FontStyleBold, UnitPixel);
    Font fDay(&ff, 12, FontStyleBold, UnitPixel);
    Font fCellN(&ff, 14, FontStyleBold, UnitPixel);
    Font fCellS(&ff, 11, FontStyleRegular, UnitPixel);
    Font fLeg(&ff, 12, FontStyleRegular, UnitPixel);
    Font fStat(&ff, 13, FontStyleRegular, UnitPixel);

    SolidBrush txBr(CLR_TEXT), dimBr(CLR_TEXT_DIM);

    int bY = 16, bH = 42, bR = 8;

    // Prev
    int pX = 15, pW = 42;
    g_prevBtn = {pX, bY, pX+pW, bY+bH};
    { SolidBrush b(g_hoverBtn==0?CLR_BTN_HOVER:CLR_BTN); FillRR(g,&b,(float)pX,(float)bY,(float)pW,(float)bH,(float)bR);
      StringFormat sf; sf.SetAlignment(StringAlignmentCenter); sf.SetLineAlignment(StringAlignmentCenter);
      g.DrawString(L"\x25C0",1,&fBtn,RectF((float)pX,(float)bY,(float)pW,(float)bH),&sf,&txBr); }

    // Next
    int nX = pX+pW+6, nW = 42;
    g_nextBtn = {nX, bY, nX+nW, bY+bH};
    { SolidBrush b(g_hoverBtn==1?CLR_BTN_HOVER:CLR_BTN); FillRR(g,&b,(float)nX,(float)bY,(float)nW,(float)bH,(float)bR);
      StringFormat sf; sf.SetAlignment(StringAlignmentCenter); sf.SetLineAlignment(StringAlignmentCenter);
      g.DrawString(L"\x25B6",1,&fBtn,RectF((float)nX,(float)bY,(float)nW,(float)bH),&sf,&txBr); }

    // Clear (rightmost)
    int clW = 65, clX = W-clW-15;
    g_clearMonthBtn = {clX, bY, clX+clW, bY+bH};
    { SolidBrush b(g_hoverBtn==4?CLR_BTN_CLEARMONTH_HOVER:CLR_BTN_CLEARMONTH);
      FillRR(g,&b,(float)clX,(float)bY,(float)clW,(float)bH,(float)bR);
      StringFormat sf; sf.SetAlignment(StringAlignmentCenter); sf.SetLineAlignment(StringAlignmentCenter);
      g.DrawString(L"Brisi",5,&fBtnSm,RectF((float)clX,(float)bY,(float)clW,(float)bH),&sf,&txBr); }

    // Copy
    int cpW = 80, cpX = clX-cpW-8;
    g_copyBtn = {cpX, bY, cpX+cpW, bY+bH};
    { SolidBrush b(g_hoverBtn==3?CLR_BTN_COPY_HOVER:CLR_BTN_COPY);
      FillRR(g,&b,(float)cpX,(float)bY,(float)cpW,(float)bH,(float)bR);
      StringFormat sf; sf.SetAlignment(StringAlignmentCenter); sf.SetLineAlignment(StringAlignmentCenter);
      g.DrawString(L"Kopiraj",7,&fBtnSm,RectF((float)cpX,(float)bY,(float)cpW,(float)bH),&sf,&txBr); }

    // Danas
    int dW = 72, dX = cpX-dW-8;
    g_todayBtn = {dX, bY, dX+dW, bY+bH};
    { SolidBrush b(g_hoverBtn==2?CLR_BTN_HOVER:CLR_BTN);
      FillRR(g,&b,(float)dX,(float)bY,(float)dW,(float)bH,(float)bR);
      StringFormat sf; sf.SetAlignment(StringAlignmentCenter); sf.SetLineAlignment(StringAlignmentCenter);
      g.DrawString(L"DANAS",5,&fBtnSm,RectF((float)dX,(float)bY,(float)dW,(float)bH),&sf,&txBr); }

    // Title
    { wchar_t t[100]; wsprintfW(t, L"%s %d", MONTH_NAMES[g_viewMonth-1], g_viewYear);
      StringFormat sf; sf.SetAlignment(StringAlignmentCenter); sf.SetLineAlignment(StringAlignmentCenter);
      g.DrawString(t,(int)wcslen(t),&fTitle,RectF((float)(nX+nW+10),(float)bY,(float)(dX-10-nX-nW-10),(float)bH),&sf,&txBr); }

    // Day names
    int dnTop = HEADER_H + 5;
    g_cellW = (W - 20) / 7;
    g_gridLeft = (W - g_cellW * 7) / 2;
    for (int i = 0; i < 7; i++) {
        StringFormat sf; sf.SetAlignment(StringAlignmentCenter); sf.SetLineAlignment(StringAlignmentCenter);
        SolidBrush* br = (i>=5) ? new SolidBrush(CLR_TEXT_WEEKEND) : new SolidBrush(CLR_TEXT_DIM);
        g.DrawString(DAY_NAMES[i],(int)wcslen(DAY_NAMES[i]),&fDay,
            RectF((float)(g_gridLeft+i*g_cellW),(float)dnTop,(float)g_cellW,(float)DAYNAMES_H),&sf,br);
        delete br;
    }

    int sY = dnTop + DAYNAMES_H;
    Pen gPen(CLR_GRID_LINE, 1.0f);
    g.DrawLine(&gPen, g_gridLeft, sY, g_gridLeft + g_cellW*7, sY);

    // Grid
    g_gridTop = sY + 4;
    int gBot = H - LEGEND_H - STATS_H;
    g_cellH = (gBot - g_gridTop) / 6;
    g_gridStartDow = DayOfWeek(1, g_viewMonth, g_viewYear);
    g_daysInView = DaysInMonth(g_viewMonth, g_viewYear);

    for (int day = 1; day <= g_daysInView; day++) {
        int idx = (day-1) + g_gridStartDow;
        int row = idx/7, col = idx%7;
        float cx = (float)(g_gridLeft + col*g_cellW + CELL_PAD);
        float cy = (float)(g_gridTop + row*g_cellH + CELL_PAD);
        float cw = (float)(g_cellW - CELL_PAD*2);
        float ch = (float)(g_cellH - CELL_PAD*2);

        ShiftType st = GetShift(day, g_viewMonth, g_viewYear);
        bool isToday = (day==g_todayDay && g_viewMonth==g_todayMonth && g_viewYear==g_todayYear);
        bool isHover = (day==g_hoverDay);
        bool isWE = (col>=5);

        Color cellBg = isHover ? CLR_CELL_HOVER : CLR_CELL_BG;
        if (st==SHIFT_DAY) cellBg=CLR_DAY_SHIFT_BG;
        if (st==SHIFT_NIGHT) cellBg=CLR_NIGHT_SHIFT_BG;
        if (st==SHIFT_FREE) cellBg=CLR_FREE_DAY_BG;

        SolidBrush cBr(cellBg);
        FillRR(g, &cBr, cx, cy, cw, ch, 8.0f);

        if (isToday) { Pen tp(CLR_CELL_TODAY_BORDER, 2.5f); DrawRR(g, &tp, cx, cy, cw, ch, 8.0f); }

        // Accent bar
        if (st != SHIFT_NONE) {
            Color ac = CLR_DAY_SHIFT;
            if (st==SHIFT_NIGHT) ac=CLR_NIGHT_SHIFT;
            if (st==SHIFT_FREE) ac=CLR_FREE_DAY;
            GraphicsPath* bp = new GraphicsPath();
            float r2=8.0f;
            bp->AddArc(cx, cy, r2*2, r2*2, 180, 90);
            bp->AddArc(cx+cw-r2*2, cy, r2*2, r2*2, 270, 90);
            bp->AddLine(cx+cw, cy+r2, cx+cw, cy+5);
            bp->AddLine(cx+cw, cy+5, cx, cy+5);
            bp->AddLine(cx, cy+5, cx, cy+r2);
            bp->CloseFigure();
            SolidBrush ab(ac); g.FillPath(&ab, bp); delete bp;
        }

        // Day num
        wchar_t ds[4]; wsprintfW(ds, L"%d", day);
        StringFormat sfN; sfN.SetAlignment(StringAlignmentNear); sfN.SetLineAlignment(StringAlignmentNear);
        Color nc = isWE ? CLR_TEXT_WEEKEND : CLR_TEXT;
        if (isToday) nc = CLR_CELL_TODAY_BORDER;
        SolidBrush nb(nc);
        g.DrawString(ds,(int)wcslen(ds),&fCellN,RectF(cx+8,cy+8,cw-16,20),&sfN,&nb);

        if (isToday) {
            StringFormat sfT; sfT.SetAlignment(StringAlignmentFar); sfT.SetLineAlignment(StringAlignmentNear);
            Font ft(&ff, 9, FontStyleBold, UnitPixel); SolidBrush tb(CLR_CELL_TODAY_BORDER);
            g.DrawString(L"DANAS",5,&ft,RectF(cx+8,cy+8,cw-16,18),&sfT,&tb);
        }

        if (st != SHIFT_NONE) {
            const wchar_t* lb = L"";
            Color lc = CLR_TEXT;
            if (st==SHIFT_DAY)   { lb=L"\x2600 DNEVNA";  lc=CLR_DAY_SHIFT; }
            if (st==SHIFT_NIGHT) { lb=L"\x263E NOCNA";    lc=CLR_NIGHT_SHIFT; }
            if (st==SHIFT_FREE)  { lb=L"\x2714 SLOBODAN"; lc=CLR_FREE_DAY; }
            StringFormat sfS; sfS.SetAlignment(StringAlignmentCenter); sfS.SetLineAlignment(StringAlignmentCenter);
            SolidBrush sb(lc);
            g.DrawString(lb,(int)wcslen(lb),&fCellS,RectF(cx,cy+ch*0.35f,cw,ch*0.55f),&sfS,&sb);
        }
    }

    // Stats
    int stTop = gBot + 2;
    int dc=0, nc2=0, fc=0;
    for (int d=1; d<=g_daysInView; d++) {
        ShiftType s=GetShift(d,g_viewMonth,g_viewYear);
        if(s==SHIFT_DAY)dc++; if(s==SHIFT_NIGHT)nc2++; if(s==SHIFT_FREE)fc++;
    }
    { wchar_t st[200];
      wsprintfW(st, L"Ovaj mjesec:   Dnevnih: %d   |   Nocnih: %d   |   Slobodnih: %d   |   Ukupno radnih: %d",
          dc, nc2, fc, dc+nc2);
      StringFormat sf; sf.SetAlignment(StringAlignmentCenter); sf.SetLineAlignment(StringAlignmentCenter);
      g.DrawString(st,(int)wcslen(st),&fStat,RectF((float)g_gridLeft,(float)stTop,(float)(g_cellW*7),(float)STATS_H),&sf,&dimBr);
    }
    g.DrawLine(&gPen, g_gridLeft, stTop+STATS_H, g_gridLeft+g_cellW*7, stTop+STATS_H);

    // Legend
    int lTop = stTop + STATS_H + 5;
    int lCY = lTop + (LEGEND_H-10)/2;
    int dotSz = 16, sp = 20;
    struct LI { Color c; const wchar_t* t; };
    LI items[] = {{CLR_DAY_SHIFT,L"Dnevna"},{CLR_NIGHT_SHIFT,L"Nocna"},{CLR_FREE_DAY,L"Slobodan"}};
    int tw = 0;
    for (int i=0;i<3;i++) { RectF b; g.MeasureString(items[i].t,(int)wcslen(items[i].t),&fLeg,PointF(0,0),&b); tw+=dotSz+8+(int)b.Width+sp; }
    tw -= sp;
    int lx = (W-tw)/2;
    for (int i=0;i<3;i++) {
        SolidBrush db(items[i].c); FillRR(g,&db,(float)lx,(float)(lCY-dotSz/2),(float)dotSz,(float)dotSz,4.0f);
        lx+=dotSz+8;
        RectF b; g.MeasureString(items[i].t,(int)wcslen(items[i].t),&fLeg,PointF(0,0),&b);
        g.DrawString(items[i].t,(int)wcslen(items[i].t),&fLeg,RectF((float)lx,(float)(lCY-b.Height/2),b.Width,b.Height),nullptr,&dimBr);
        lx+=(int)b.Width+sp;
    }
    { Font fi(&ff,10,FontStyleItalic,UnitPixel); SolidBrush ib(Color(255,70,72,100));
      StringFormat sf; sf.SetAlignment(StringAlignmentCenter);
      g.DrawString(L"Lijevi klik = postavi  |  Desni klik = obrisi  |  Scroll = mjesec",65,&fi,
          RectF(0,(float)(lCY+dotSz/2+6),(float)W,20),&sf,&ib); }
}

// ============================================================================
//  HIT TESTING
// ============================================================================

static int HitTestDay(int mx, int my) {
    if (my < g_gridTop || g_cellW==0 || g_cellH==0) return -1;
    int col=(mx-g_gridLeft)/g_cellW, row=(my-g_gridTop)/g_cellH;
    if (col<0||col>6||row<0||row>5) return -1;
    int day = row*7+col-g_gridStartDow+1;
    return (day>=1 && day<=g_daysInView) ? day : -1;
}

static int HitTestButton(int mx, int my) {
    POINT pt={mx,my};
    if (PtInRect(&g_prevBtn,pt)) return 0;
    if (PtInRect(&g_nextBtn,pt)) return 1;
    if (PtInRect(&g_todayBtn,pt)) return 2;
    if (PtInRect(&g_copyBtn,pt)) return 3;
    if (PtInRect(&g_clearMonthBtn,pt)) return 4;
    return -1;
}

// ============================================================================
//  NAVIGATION
// ============================================================================

static void GoToPrevMonth() { g_viewMonth--; if(g_viewMonth<1){g_viewMonth=12;g_viewYear--;} InvalidateRect(g_hWnd,NULL,FALSE); }
static void GoToNextMonth() { g_viewMonth++; if(g_viewMonth>12){g_viewMonth=1;g_viewYear++;} InvalidateRect(g_hWnd,NULL,FALSE); }
static void GoToToday() { g_viewMonth=g_todayMonth; g_viewYear=g_todayYear; InvalidateRect(g_hWnd,NULL,FALSE); }

// ============================================================================
//  CONTEXT MENU
// ============================================================================

static void ShowShiftMenu(int day, int x, int y) {
    HMENU hM = CreatePopupMenu();
    ShiftType cur = GetShift(day, g_viewMonth, g_viewYear);
    AppendMenuW(hM, MF_STRING|(cur==SHIFT_DAY?MF_CHECKED:0),   IDM_DAY_SHIFT,  L"  \x2600  Dnevna Smjena");
    AppendMenuW(hM, MF_STRING|(cur==SHIFT_NIGHT?MF_CHECKED:0), IDM_NIGHT_SHIFT, L"  \x263E  Nocna Smjena");
    AppendMenuW(hM, MF_STRING|(cur==SHIFT_FREE?MF_CHECKED:0),  IDM_FREE_DAY,    L"  \x2714  Slobodan Dan");
    AppendMenuW(hM, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hM, MF_STRING, IDM_CLEAR, L"  \x2716  Obrisi");
    POINT pt={x,y}; ClientToScreen(g_hWnd,&pt);
    int cmd = TrackPopupMenu(hM, TPM_RETURNCMD|TPM_RIGHTBUTTON, pt.x, pt.y, 0, g_hWnd, NULL);
    if (cmd==IDM_DAY_SHIFT)   SetShift(day,g_viewMonth,g_viewYear,SHIFT_DAY);
    if (cmd==IDM_NIGHT_SHIFT) SetShift(day,g_viewMonth,g_viewYear,SHIFT_NIGHT);
    if (cmd==IDM_FREE_DAY)    SetShift(day,g_viewMonth,g_viewYear,SHIFT_FREE);
    if (cmd==IDM_CLEAR)       SetShift(day,g_viewMonth,g_viewYear,SHIFT_NONE);
    if (cmd) { SaveData(); InvalidateRect(g_hWnd,NULL,FALSE); }
    DestroyMenu(hM);
}

// ============================================================================
//  MAIN WNDPROC
// ============================================================================

static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: g_hWnd=hWnd; return 0;

    case WM_PAINT: {
        PAINTSTRUCT ps; HDC hdc=BeginPaint(hWnd,&ps);
        RECT rc; GetClientRect(hWnd,&rc);
        HDC mem=CreateCompatibleDC(hdc);
        HBITMAP bm=CreateCompatibleBitmap(hdc,rc.right,rc.bottom);
        HBITMAP old=(HBITMAP)SelectObject(mem,bm);
        DrawCalendar(mem,rc.right,rc.bottom);
        BitBlt(hdc,0,0,rc.right,rc.bottom,mem,0,0,SRCCOPY);
        SelectObject(mem,old); DeleteObject(bm); DeleteDC(mem);
        EndPaint(hWnd,&ps); return 0;
    }

    case WM_ERASEBKGND: return 1;
    case WM_SIZE: InvalidateRect(hWnd,NULL,FALSE); return 0;

    case WM_GETMINMAXINFO: {
        MINMAXINFO* m=(MINMAXINFO*)lParam;
        m->ptMinTrackSize.x=MIN_W; m->ptMinTrackSize.y=MIN_H; return 0;
    }

    case WM_MOUSEMOVE: {
        int mx=(int)(short)LOWORD(lParam), my=(int)(short)HIWORD(lParam);
        int nh=HitTestDay(mx,my), nb=HitTestButton(mx,my);
        if (nh!=g_hoverDay||nb!=g_hoverBtn) {
            g_hoverDay=nh; g_hoverBtn=nb;
            InvalidateRect(hWnd,NULL,FALSE);
            SetCursor(LoadCursor(NULL,(nh>0||nb>=0)?IDC_HAND:IDC_ARROW));
        }
        TRACKMOUSEEVENT tme={sizeof(tme),TME_LEAVE,hWnd,0}; TrackMouseEvent(&tme);
        return 0;
    }

    case WM_MOUSELEAVE:
        g_hoverDay=-1; g_hoverBtn=-1; InvalidateRect(hWnd,NULL,FALSE); return 0;

    case WM_LBUTTONDOWN: {
        int mx=(int)(short)LOWORD(lParam), my=(int)(short)HIWORD(lParam);
        int btn=HitTestButton(mx,my);
        if (btn==0) { GoToPrevMonth(); return 0; }
        if (btn==1) { GoToNextMonth(); return 0; }
        if (btn==2) { GoToToday(); return 0; }
        if (btn==3) { ShowCopyDialog(); return 0; }
        if (btn==4) { ClearMonth(); return 0; }
        int day=HitTestDay(mx,my);
        if (day>0) ShowShiftMenu(day,mx,my);
        return 0;
    }

    case WM_RBUTTONDOWN: {
        int mx=(int)(short)LOWORD(lParam), my=(int)(short)HIWORD(lParam);
        int day=HitTestDay(mx,my);
        if (day>0 && GetShift(day,g_viewMonth,g_viewYear)!=SHIFT_NONE) {
            SetShift(day,g_viewMonth,g_viewYear,SHIFT_NONE);
            SaveData(); InvalidateRect(hWnd,NULL,FALSE);
        }
        return 0;
    }

    case WM_MOUSEWHEEL: {
        if (GET_WHEEL_DELTA_WPARAM(wParam)>0) GoToPrevMonth(); else GoToNextMonth();
        return 0;
    }

    case WM_KEYDOWN:
        if (wParam==VK_LEFT) GoToPrevMonth();
        else if (wParam==VK_RIGHT) GoToNextMonth();
        else if (wParam==VK_HOME) GoToToday();
        return 0;

    case WM_DESTROY: SaveData(); PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(hWnd,msg,wParam,lParam);
}

// ============================================================================
//  ENTRY POINT
// ============================================================================

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, int nShow) {
    GdiplusStartupInput si; GdiplusStartup(&g_gdipToken,&si,NULL);
    INITCOMMONCONTROLSEX ic={sizeof(ic),ICC_WIN95_CLASSES}; InitCommonControlsEx(&ic);

    time_t now=time(NULL); struct tm* t=localtime(&now);
    g_todayDay=t->tm_mday; g_todayMonth=t->tm_mon+1; g_todayYear=t->tm_year+1900;
    g_viewMonth=g_todayMonth; g_viewYear=g_todayYear;

    GetDataPath(); LoadData();

    WNDCLASSEXW wc={}; wc.cbSize=sizeof(wc); wc.style=CS_HREDRAW|CS_VREDRAW;
    wc.lpfnWndProc=WndProc; wc.hInstance=hInst; wc.hCursor=LoadCursor(NULL,IDC_ARROW);
    wc.lpszClassName=L"SmjeneKalendarClass";
    wc.hIcon=LoadIcon(NULL,IDI_APPLICATION); wc.hIconSm=wc.hIcon;
    RegisterClassExW(&wc);

    int sw=GetSystemMetrics(SM_CXSCREEN), sh=GetSystemMetrics(SM_CYSCREEN);
    int ww=950, wh=740;
    HWND hw=CreateWindowExW(0,L"SmjeneKalendarClass",L"Raspored Smjena - Kalendar v2.1",
        WS_OVERLAPPEDWINDOW,(sw-ww)/2,(sh-wh)/2,ww,wh,NULL,NULL,hInst,NULL);
    if (!hw) return 1;
    ShowWindow(hw,nShow); UpdateWindow(hw);

    MSG msg;
    while (GetMessageW(&msg,NULL,0,0)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    GdiplusShutdown(g_gdipToken);
    return (int)msg.wParam;
}

#ifndef _MSC_VER
int main() { return wWinMain(GetModuleHandle(NULL),NULL,GetCommandLineW(),SW_SHOWDEFAULT); }
#endif
