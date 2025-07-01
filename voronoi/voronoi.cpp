#include <Windows.h>
#include <vector>
using namespace std;

struct point
{
    double x, y;
};
struct line
{
    double x1, y1, x2, y2;
};
class VoronoiFunc
{ 
public:
    point MidPoint(const point& p1, const point& p2)
    {
        return {(p1.x + p2.x) / 2.0, (p1.y + p2.y) / 2.0};
    }
    point GetCircumCentre(const point& p1, const point& p2, const point& p3)
    {
        /* 算中點 */
        point mid_12 = MidPoint(p1, p2);
        point mid_23 = MidPoint(p2, p3);

        /* 算{dx, dy}: 方向向量, {-dy, dx}: 法向量{nx, ny} */
        /* p1 & p2 */
        double dx_12 = p2.x - p1.x;
        double dy_12 = p2.y - p1.y;
        double nx_12 = -dy_12;
        double ny_12 = dx_12;

        /* p2 & p3 */
        double dx_23 = p3.x - p2.x;
        double dy_23 = p3.y - p2.y;
        double nx_23 = -dy_23;
        double ny_23 = dx_23;

        // L12: x = mid_12.x + t * nx_12
        //      y = mid_12.y + t * ny_12
        // L23: x = mid_23.x + s * nx_23
        //      y = mid_23.y + s * ny_23
        // 解聯立方程式, 解 t or s
        // mid_12.x + t * nx_12 = mid_23.x + s * nx_23
        // mid_12.y + t * ny_12 = mid_23.y + s * ny_23
        double ax = nx_12;
        double bx = -nx_23;
        double cx = mid_23.x - mid_12.x;

        double ay = ny_12;
        double by = -ny_23;
        double cy = mid_23.y - mid_12.y;

        // t*ax + s*bx = cx
        // t*ay + s*by = cy
        // 用 Cramer's rule 解 det -> t
        double det = ax * by - ay * bx;

        if (fabs(det) < 1e-8) // 若det = 0->表示無內積->兩中垂平行->無外心
        {
            return { -1, -1 };
        }

        double t = (bx * cy - by * cx) / det;

        return {mid_12.x + t * nx_12, mid_12.y + t * ny_12};
    }
    point GetEndPoint(const point& p1, const point& p2)
    {

    }
};

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


vector<point> voronoi_point; /* 儲存點的座標 */

bool enable_mouse_input = false;

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
                if (voronoi_point.size() == 3)
                {
                    VoronoiFunc vf;
                    point p1 = voronoi_point[0];
                    point p2 = voronoi_point[1];
                    point p3 = voronoi_point[2];
                    point circumcentre = vf.GetCircumCentre(p1, p2, p3);
                    
                    /* 線段起點{from_x, from_y} & 終點{to_x, to_y} */
                    double len = 100000.0; // 延伸至無限長
                    double from_x = circumcentre.x;
                    double from_y = circumcentre.y;
                    
                    for (int i = 0; i < voronoi_point.size() - 1; ++i)
                    {
                        for (int j = i + 1; j < voronoi_point.size(); ++j)
                        {
                            point end_point = vf.GetEndPoint(voronoi_point[i], voronoi_point[j]);
                            double to_x = end_point.x;
                            double to_y = end_point.y;
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