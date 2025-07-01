#include <Windows.h>
#include <vector>
using namespace std;

/* 事前宣告 */
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

/* 程式入口 */
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    /* 1. 註冊視窗類別，指定它的 WndProc */
    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc;       /* 告訴 Windows 用哪個函式處理訊息 */
    wc.hInstance = hInstance;
    wc.lpszClassName = L"MyWindow"; /* L"字串內容" -> 轉換成寬字元 */
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1); // 加入背景刷子
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);      // 加入游標樣式
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);

    if (!RegisterClass(&wc)) {
        MessageBox(NULL, L"RegisterClass failed!", L"Error", MB_OK);
        return 1;
    }

    /* 2. 建立視窗 */
    HWND hwnd = CreateWindow
                (L"MyWindow",          /* 告訴系統要建立一個MyWindow類型的視窗 */
                L"Voronoi Visualizer", /* 視窗標題 */
                WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, 
                CW_USEDEFAULT, 600, 600,
                NULL, NULL, hInstance, NULL);

    if (hwnd == NULL) {
        DWORD err = GetLastError();
        wchar_t msg[256];
        wsprintf(msg, L"CreateWindow failed! Error Code = %lu", err);
        MessageBox(NULL, msg, L"Error", MB_OK);
        return 1;
    }

    /* 建立功能按鍵 */
    CreateWindow(L"BUTTON", L"Mouse Input", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
        420, 50, 120, 30, hwnd, (HMENU)1, hInstance, NULL);
    CreateWindow(L"BUTTON", L"Execute", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
        420, 100, 120, 30, hwnd, (HMENU)2, hInstance, NULL);
    CreateWindow(L"BUTTON", L"Refresh", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
        420, 150, 120, 30, hwnd, (HMENU)3, hInstance, NULL);

    ShowWindow(hwnd, nCmdShow);        /* 出現視窗 */
    UpdateWindow(hwnd);                /* 重繪視窗 */

    /* 3. 進入「訊息迴圈」，等待使用者事件 */
    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg); /* 把訊息送到 WndProc() 去處理 */
    }

    return 0;
}

/* 實際內容 */
/* HWND : Handle to Window 被操作的視窗編號*/
/* UINT : Unsigned Int（訊息 ID）事件類型，例如 WM_PAINT, WM_LBUTTONDOWN*/
/* WPARAM : Word Parameter（16/32位）訊息的額外資訊（例如按下的是哪顆鍵）*/
/* LPARAM : Long Parameter（32/64位）通常包含滑鼠位置等資料*/

struct point /* 自定義point資料結構: {int x, int y} */
{
    int x;
    int y;
};
vector<point> voronoi_point; /* 儲存點的座標 */

bool enable_mouse_input = false;

struct line
{
    double x1, y1, x2, y2;
};
vector<line> voronoi_edge; /* 儲存生成的邊 */

bool enable_edge_create = false;

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_COMMAND:
    {
        int button_id = LOWORD(wParam);
        switch(button_id)
        {
        case 1: // Mouse Input
        {
            MessageBox(hwnd, L"Mouse Input Enabled", L"Info", MB_OK);
            enable_mouse_input = true;
            break;
        }
        case 2: // Execute
        {
            MessageBox(hwnd, L"Execute Clicked", L"Info", MB_OK);
            enable_mouse_input = false;
            enable_edge_create = true;
            voronoi_edge.clear(); /* 清除舊邊 */

            if (voronoi_point.size() >= 2)
            {
                for (int i = 0; i < voronoi_point.size() - 1; ++i)
                {
                    for (int j = i + 1; j < voronoi_point.size(); ++j)
                    {
                        /* {dx, dy} : 方向向量, {-dy, dx} : 法向量 */
                        double dx = voronoi_point[j].x - voronoi_point[i].x;
                        double dy = voronoi_point[j].y - voronoi_point[i].y;
                        double nx = -dy;
                        double ny = dx;

                        /* {midx, midy} : 兩點中點 */
                        double midx = (voronoi_point[i].x + voronoi_point[j].x) / 2.0;
                        double midy = (voronoi_point[i].y + voronoi_point[j].y) / 2.0;

                        /* 線段起點{from_x, from_y} & 終點{to_x, to_y} */
                        double len = 100000.0; // 延伸至無限長
                        double from_x = midx + nx * len;
                        double from_y = midy + ny * len;
                        double to_x = midx - nx * len;
                        double to_y = midy - ny * len;

                        voronoi_edge.push_back({ from_x, from_y, to_x, to_y });
                    }
                }
            }
            InvalidateRect(hwnd, NULL, TRUE);

            break;
        }
        case 3: // Refresh
        {
            MessageBox(hwnd, L"Refresh Clicked", L"Info", MB_OK);
            voronoi_point.clear();
            enable_mouse_input = false;
            voronoi_edge.clear();
            InvalidateRect(hwnd, NULL, TRUE);
            break;
        }
        }
    }

    case WM_LBUTTONDOWN: /* 滑鼠左鍵按下 */
    {
        if (enable_mouse_input)
        {
            int x = LOWORD(lParam);
            int y = HIWORD(lParam);
            voronoi_point.push_back({ x, y }); /* 存入座標 */
            InvalidateRect(hwnd, NULL, TRUE);
            break;
        }
        else
        {
            break;
        }
        
    }
        
    case WM_PAINT: /* 畫面繪製 */
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        /* 畫出所有點 */
        for (const auto& p : voronoi_point)
        {
            Ellipse(hdc, p.x - 3, p.y - 3, p.x + 3, p.y + 3); /* draw a point */
        }

        if (enable_edge_create)
        {
            /* 畫出所有邊 */
            HPEN hPen = CreatePen(PS_SOLID, 1, RGB(0, 100, 150));
            HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
            for (const auto& e : voronoi_edge)
            {
                MoveToEx(hdc, e.x1, e.y1, NULL);
                LineTo(hdc, e.x2, e.y2);
            }
            SelectObject(hdc, hOldPen);
            DeleteObject(hPen);

            enable_edge_create = false;
        }
        
        EndPaint(hwnd, &ps);
        break;
    }

    case WM_DESTROY:
    {
        PostQuitMessage(0); /* 發送 WM_QUIT，離開程式 */
        break;
    }

    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}