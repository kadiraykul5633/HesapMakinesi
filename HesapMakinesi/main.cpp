#include <windows.h>
#include <string>
#include <sstream>
#include <cmath>

#define ID_BASLANGIC 200
#define ID_ZAMANLAYICI 1

const double PI = 3.14159265358979323846;

// ---------- Renkler ----------
const COLORREF RENK_ZEMIN = RGB(28, 28, 28);
const COLORREF RENK_RAKAM = RGB(58, 58, 58);
const COLORREF RENK_ISLEM = RGB(255, 149, 0);
const COLORREF RENK_OZEL = RGB(165, 165, 165);
const COLORREF RENK_BILIMSEL = RGB(44, 44, 50);

// ---------- Düğme tablosu ----------
// tur: 0 = rakam, 1 = işlem, 2 = özel (C, geri sil, %), 3 = bilimsel
struct Dugme
{
    const wchar_t* etiket;
    wchar_t kod;
    int sutun, satir;
    int tur;
};

const Dugme dugmeler[] = {
    // Bilimsel satırlar
    { L"sin",    L'S', 0, 0, 3 },
    { L"cos",    L'O', 1, 0, 3 },
    { L"tan",    L'T', 2, 0, 3 },
    { L"ln",     L'L', 3, 0, 3 },

    { L"\u221A", L'R', 0, 1, 3 },   // √
    { L"x\u00B2", L'Q', 1, 1, 3 },  // x²
    { L"1/x",    L'I', 2, 1, 3 },
    { L"x^y",    L'^', 3, 1, 3 },

    // Normal hesap makinesi
    { L"C",      L'C', 0, 2, 2 },
    { L"\u2190", L'B', 1, 2, 2 },   // ←
    { L"%",      L'%', 2, 2, 2 },
    { L"\u00F7", L'/', 3, 2, 1 },   // ÷

    { L"7", L'7', 0, 3, 0 },
    { L"8", L'8', 1, 3, 0 },
    { L"9", L'9', 2, 3, 0 },
    { L"\u00D7", L'*', 3, 3, 1 },   // ×

    { L"4", L'4', 0, 4, 0 },
    { L"5", L'5', 1, 4, 0 },
    { L"6", L'6', 2, 4, 0 },
    { L"\u2212", L'-', 3, 4, 1 },   // −

    { L"1", L'1', 0, 5, 0 },
    { L"2", L'2', 1, 5, 0 },
    { L"3", L'3', 2, 5, 0 },
    { L"+", L'+', 3, 5, 1 },

    { L"\u00B1", L'N', 0, 6, 0 },   // ±
    { L"0", L'0', 1, 6, 0 },
    { L".", L'.', 2, 6, 0 },
    { L"=", L'=', 3, 6, 1 },
};
const int DUGME_SAYISI = sizeof(dugmeler) / sizeof(dugmeler[0]);

// ---------- Genel değişkenler ----------
HWND hAna;
HFONT hFontDugme, hFontBilimsel, hFontEkran, hFontEkranKucuk, hFontEkranMini, hFontUst;
RECT rcEkran = { 8, 8, 312, 118 };
int hoverIndex = -1;                   // fare şu an hangi düğmenin üstünde (-1 = hiçbiri)

// Hesap makinesinin hafızası
std::wstring ekranMetni = L"0";
double ilkSayi = 0;
wchar_t islem = 0;
bool yeniSayiBekleniyor = true;        // ikinci sayı henüz girilmedi mi?
bool ekranDegistirilecek = true;       // sıradaki rakam ekranın üstüne mi yazılsın?

// ---------- Yardımcı fonksiyonlar ----------
HFONT FontYap(int yukseklik)
{
    return CreateFont(-yukseklik, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
}

std::wstring SayiyiYaziyaCevir(double d)
{
    if (std::fabs(d) < 1e-12) d = 0;   // sin(180) gibi 1e-16'lık artıkları sıfırla
    std::wostringstream ss;
    ss.precision(12);
    ss << d;
    return ss.str();
}

const wchar_t* IslemSembolu(wchar_t op)
{
    switch (op)
    {
    case L'/': return L"\u00F7";
    case L'*': return L"\u00D7";
    case L'-': return L"\u2212";
    case L'+': return L"+";
    case L'^': return L"^";
    }
    return L"";
}

COLORREF Acik(COLORREF c, int miktar)
{
    auto ac = [miktar](int v) { return v + miktar > 255 ? 255 : v + miktar; };
    return RGB(ac(GetRValue(c)), ac(GetGValue(c)), ac(GetBValue(c)));
}

void EkraniGuncelle()
{
    InvalidateRect(hAna, &rcEkran, FALSE);
}

void Sifirla()
{
    ekranMetni = L"0";
    ilkSayi = 0;
    islem = 0;
    yeniSayiBekleniyor = true;
    ekranDegistirilecek = true;
}

void HataGoster()
{
    ekranMetni = L"Hata";
    islem = 0;
    yeniSayiBekleniyor = true;
    ekranDegistirilecek = true;
}

// Sonuç geçerliyse ekrana yazar, değilse (sonsuz, tanımsız) Hata gösterir
bool SonucuEkranaYaz(double r)
{
    if (!std::isfinite(r))
    {
        HataGoster();
        return false;
    }
    ekranMetni = SayiyiYaziyaCevir(r);
    return true;
}

// İki sayılı işlemler: + - * / ^
bool Hesapla(double a, double b, wchar_t op, double& sonuc)
{
    switch (op)
    {
    case L'+': sonuc = a + b; return true;
    case L'-': sonuc = a - b; return true;
    case L'*': sonuc = a * b; return true;
    case L'/':
        if (b == 0) return false;
        sonuc = a / b;
        return true;
    case L'^': sonuc = std::pow(a, b); return true;
    }
    sonuc = b;
    return true;
}
// Bekleyen işlemi (ilkSayi islem ekrandaki) uygular, sonucu ekrana yazar
bool BekleyenIslemiUygula()
{
    double ikinciSayi = std::stod(ekranMetni);
    double sonuc;
    if (!Hesapla(ilkSayi, ikinciSayi, islem, sonuc))
    {
        HataGoster();
        return false;
    }
    return SonucuEkranaYaz(sonuc);
}

// Tek sayılı işlemler: sin cos tan ln √ x² 1/x
void TekliIslem(wchar_t c)
{
    double x = std::stod(ekranMetni);
    double radyan = x * PI / 180.0;   // trigonometri DERECE ile çalışır
    double r = 0;
    bool tamam = true;

    switch (c)
    {
    case L'S': r = std::sin(radyan); break;
    case L'O': r = std::cos(radyan); break;
    case L'T':
        if (std::fabs(std::cos(radyan)) < 1e-12) tamam = false;   // tan(90) tanımsız
        else r = std::tan(radyan);
        break;
    case L'L':
        if (x <= 0) tamam = false;
        else r = std::log(x);
        break;
    case L'R':
        if (x < 0) tamam = false;
        else r = std::sqrt(x);
        break;
    case L'Q': r = x * x; break;
    case L'I':
        if (x == 0) tamam = false;
        else r = 1.0 / x;
        break;
    }

    if (!tamam) { HataGoster(); return; }
    if (SonucuEkranaYaz(r))
    {
        yeniSayiBekleniyor = false;    // ekrandaki artık ikinci sayı sayılır
        ekranDegistirilecek = true;    // ama sıradaki rakam üstüne yazılsın
    }
}

// ---------- Ekranı çizme ----------
void EkraniCiz(HDC hdc)
{
    HBRUSH zemin = CreateSolidBrush(RENK_ZEMIN);
    FillRect(hdc, &rcEkran, zemin);
    DeleteObject(zemin);
    SetBkMode(hdc, TRANSPARENT);

    if (islem != 0)
    {
        std::wstring ust = SayiyiYaziyaCevir(ilkSayi) + L" " + IslemSembolu(islem);
        RECT r = { 16, 14, 304, 44 };
        SelectObject(hdc, hFontUst);
        SetTextColor(hdc, RGB(150, 150, 150));
        DrawText(hdc, ust.c_str(), -1, &r, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    }

    RECT r = { 16, 44, 304, 114 };
    HFONT font = hFontEkran;
    if (ekranMetni.size() > 14)     font = hFontEkranMini;
    else if (ekranMetni.size() > 9) font = hFontEkranKucuk;
    SelectObject(hdc, font);
    SetTextColor(hdc, RGB(255, 255, 255));
    DrawText(hdc, ekranMetni.c_str(), -1, &r, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
}

// ---------- Asıl mantık ----------
void DugmeyeBasildi(wchar_t c)
{
    if (ekranMetni == L"Hata")
        Sifirla();

    if (c >= L'0' && c <= L'9')
    {
        if (ekranDegistirilecek || ekranMetni == L"0")
            ekranMetni = std::wstring(1, c);
        else if (ekranMetni.size() < 15)
            ekranMetni += c;
        yeniSayiBekleniyor = false;
        ekranDegistirilecek = false;
    }
    else if (c == L'.')
    {
        if (ekranDegistirilecek)
        {
            ekranMetni = L"0.";
            yeniSayiBekleniyor = false;
            ekranDegistirilecek = false;
        }
        else if (ekranMetni.find(L'.') == std::wstring::npos)
            ekranMetni += L'.';
    }
    else if (c == L'B')   // geri sil
    {
        if (!ekranDegistirilecek)
        {
            ekranMetni.pop_back();
            if (ekranMetni.empty() || ekranMetni == L"-")
                ekranMetni = L"0";
        }
    }
    else if (c == L'C')
    {
        Sifirla();
    }
    else if (c == L'N')   // ± işaret değiştir
    {
        if (ekranMetni != L"0")
        {
            if (ekranMetni[0] == L'-') ekranMetni.erase(0, 1);
            else                       ekranMetni.insert(0, 1, L'-');
        }
        yeniSayiBekleniyor = false;
    }
    else if (c == L'%')
    {
        double x = std::stod(ekranMetni);
        double r;
        if (islem == L'+' || islem == L'-')
            r = ilkSayi * x / 100.0;   // 200 + 10% -> 20
        else
            r = x / 100.0;
        if (SonucuEkranaYaz(r))
        {
            yeniSayiBekleniyor = false;
            ekranDegistirilecek = true;
        }
    }
    else if (c == L'S' || c == L'O' || c == L'T' || c == L'L' ||
        c == L'R' || c == L'Q' || c == L'I')
    {
        TekliIslem(c);
    }
    else if (c == L'=')
    {
        if (islem != 0 && !yeniSayiBekleniyor)
        {
            if (BekleyenIslemiUygula())
            {
                islem = 0;
                yeniSayiBekleniyor = true;
                ekranDegistirilecek = true;
            }
        }
    }
    else // + - * / ^
    {
        if (islem != 0 && !yeniSayiBekleniyor)
        {
            if (!BekleyenIslemiUygula())
            {
                EkraniGuncelle();
                return;
            }
        }
        ilkSayi = std::stod(ekranMetni);
        islem = c;
        yeniSayiBekleniyor = true;
        ekranDegistirilecek = true;
    }

    EkraniGuncelle();
}

void KlavyedenGirildi(wchar_t ch)
{
    if ((ch >= L'0' && ch <= L'9') || ch == L'+' || ch == L'-' ||
        ch == L'*' || ch == L'/' || ch == L'%' || ch == L'^')
        DugmeyeBasildi(ch);
    else if (ch == L'=' || ch == L'\r')
        DugmeyeBasildi(L'=');
    else if (ch == L'.' || ch == L',')
        DugmeyeBasildi(L'.');
    else if (ch == L'\b')
        DugmeyeBasildi(L'B');
    else if (ch == 27 || ch == L'c' || ch == L'C')
        DugmeyeBasildi(L'C');
    else if (ch == L'r' || ch == L'R')      // r = karekök
        DugmeyeBasildi(L'R');
    else if (ch == L'n' || ch == L'N')      // n = işaret değiştir
        DugmeyeBasildi(L'N');
}

// ---------- Pencere prosedürü ----------
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
    {
        hAna = hwnd;
        hFontDugme = FontYap(24);
        hFontBilimsel = FontYap(18);
        hFontEkran = FontYap(48);
        hFontEkranKucuk = FontYap(32);
        hFontEkranMini = FontYap(24);
        hFontUst = FontYap(20);

        for (int i = 0; i < DUGME_SAYISI; i++)
        {
            const Dugme& d = dugmeler[i];
            CreateWindow(
                L"BUTTON", d.etiket,
                WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                8 + d.sutun * 78, 130 + d.satir * 58, 70, 52,
                hwnd, (HMENU)(INT_PTR)(ID_BASLANGIC + i),
                nullptr, nullptr);
        }

        SetTimer(hwnd, ID_ZAMANLAYICI, 40, nullptr);   // hover kontrolü için
        return 0;
    }

    case WM_TIMER:
    {
        // Fare hangi düğmenin üstünde?
        POINT p;
        GetCursorPos(&p);
        HWND h = WindowFromPoint(p);
        int yeni = -1;
        if (h && GetParent(h) == hwnd)
        {
            int id = GetDlgCtrlID(h);
            if (id >= ID_BASLANGIC && id < ID_BASLANGIC + DUGME_SAYISI)
                yeni = id - ID_BASLANGIC;
        }
        if (yeni != hoverIndex)
        {
            int eski = hoverIndex;
            hoverIndex = yeni;
            if (eski >= 0) InvalidateRect(GetDlgItem(hwnd, ID_BASLANGIC + eski), nullptr, FALSE);
            if (yeni >= 0) InvalidateRect(GetDlgItem(hwnd, ID_BASLANGIC + yeni), nullptr, FALSE);
        }
        return 0;
    }

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        EkraniCiz(hdc);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_DRAWITEM:
    {
        DRAWITEMSTRUCT* dis = (DRAWITEMSTRUCT*)lParam;
        int index = (int)dis->CtlID - ID_BASLANGIC;
        if (index < 0 || index >= DUGME_SAYISI) break;

        const Dugme& d = dugmeler[index];
        bool basili = (dis->itemState & ODS_SELECTED) != 0;

        COLORREF arka, yazi;
        switch (d.tur)
        {
        case 1:  arka = RENK_ISLEM;    yazi = RGB(255, 255, 255); break;
        case 2:  arka = RENK_OZEL;     yazi = RGB(0, 0, 0);       break;
        case 3:  arka = RENK_BILIMSEL; yazi = RGB(255, 255, 255); break;
        default: arka = RENK_RAKAM;    yazi = RGB(255, 255, 255); break;
        }
        if (basili)                    arka = Acik(arka, 50);
        else if (index == hoverIndex)  arka = Acik(arka, 25);

        HDC hdc = dis->hDC;
        RECT r = dis->rcItem;

        HBRUSH zemin = CreateSolidBrush(RENK_ZEMIN);
        FillRect(hdc, &r, zemin);
        DeleteObject(zemin);

        HBRUSH firca = CreateSolidBrush(arka);
        HGDIOBJ eskiFirca = SelectObject(hdc, firca);
        HGDIOBJ eskiKalem = SelectObject(hdc, GetStockObject(NULL_PEN));
        RoundRect(hdc, r.left, r.top, r.right + 1, r.bottom + 1, 16, 16);
        SelectObject(hdc, eskiKalem);
        SelectObject(hdc, eskiFirca);
        DeleteObject(firca);

        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, yazi);
        HGDIOBJ eskiFont = SelectObject(hdc, d.tur == 3 ? hFontBilimsel : hFontDugme);
        DrawText(hdc, d.etiket, -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(hdc, eskiFont);
        return TRUE;
    }

    case WM_COMMAND:
    {
        int id = LOWORD(wParam);
        if (id >= ID_BASLANGIC && id < ID_BASLANGIC + DUGME_SAYISI)
            DugmeyeBasildi(dugmeler[id - ID_BASLANGIC].kod);

        SetFocus(hwnd);
        return 0;
    }

    case WM_DESTROY:
        KillTimer(hwnd, ID_ZAMANLAYICI);
        DeleteObject(hFontDugme);
        DeleteObject(hFontBilimsel);
        DeleteObject(hFontEkran);
        DeleteObject(hFontEkranKucuk);
        DeleteObject(hFontEkranMini);
        DeleteObject(hFontUst);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance,
    _In_ PWSTR pCmdLine, _In_ int nCmdShow)
{
    const wchar_t CLASS_NAME[] = L"HesapMakinesiPenceresi";

    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(RENK_ZEMIN);

    RegisterClass(&wc);

    DWORD stil = WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX;
    RECT rc = { 0, 0, 320, 540 };
    AdjustWindowRect(&rc, stil, FALSE);

    HWND hwnd = CreateWindowEx(
        0, CLASS_NAME, L"Hesap Makinesi", stil,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rc.right - rc.left, rc.bottom - rc.top,
        nullptr, nullptr, hInstance, nullptr);

    if (!hwnd) return 0;
    ShowWindow(hwnd, nCmdShow);
    SetFocus(hwnd);

    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (msg.message == WM_CHAR)
        {
            KlavyedenGirildi((wchar_t)msg.wParam);
            continue;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}