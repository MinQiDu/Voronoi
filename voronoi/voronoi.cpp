#include <Windows.h>
#include <vector>
#include <algorithm>
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
                CW_USEDEFAULT, 800, 600,
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
        620, 50, 120, 30, hwnd, (HMENU)1, hInstance, NULL);
    CreateWindow(L"BUTTON", L"Execute", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
        620, 100, 120, 30, hwnd, (HMENU)2, hInstance, NULL);
    CreateWindow(L"BUTTON", L"Refresh", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
        620, 150, 120, 30, hwnd, (HMENU)3, hInstance, NULL);

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

struct point
{
    double x, y;
};
struct line
{
    double x1, y1, x2, y2;
};
vector<point> voronoi_point; /* 儲存點的座標 */
bool enable_mouse_input = false;
vector<line> voronoi_edge; /* 儲存生成的邊 */
bool enable_edge_create = false;
class VoronoiFunc
{
public:
    point MidPoint(const point& p1, const point& p2)
    {
        return { (p1.x + p2.x) / 2.0, (p1.y + p2.y) / 2.0 };
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

        double t = (by * cx - bx * cy) / det;

        return { mid_12.x + t * nx_12, mid_12.y + t * ny_12 };
    }
    point GetEndPoint(const point& p1, const point& p2, const point& p_other)
    {
        /* 算中點 */
        point mid_12 = MidPoint(p1, p2);

        /* 算{dx, dy}: 方向向量, {-dy, dx}: 法向量{nx, ny} */
        double dx_12 = p2.x - p1.x;
        double dy_12 = p2.y - p1.y;
        double nx_12 = -dy_12;
        double ny_12 = dx_12;

        /* 使用點積判斷方向（中點與第三點的向量 * 法向量） */
        // 幾何意義: v & w 的點積可以看做 w 朝著過原點朝 v 的直線上投影，再將投影長度乘上v的長度
        // 對 '中點與第三點的向量' & '法向量' 算點積 dot = nx_12 * mid_other_x + ny_12 * mid_other_y 
		// 若點積 dot > 0, 代表第三點在當前法向量延伸的反端，則voronoi邊的方向正確，從外心沿著法向量延伸即可
		// 若點積 dot < 0, 代表第三點在當前法向量延伸的那端，則voronoi邊的方向需要反轉，從外心沿著法向量反向延伸
        double mid_other_x = mid_12.x - p_other.x;              //中點與第三點的x向量 mid_other_x
        double mid_other_y = mid_12.y - p_other.y;              //中點與第三點的y向量 mid_other_y
		double dot = nx_12 * mid_other_x + ny_12 * mid_other_y; // 計算點積

        // 如果點積 dot < 0，代表方向指向第三點，要反轉
        if (dot < 0)
        {
            nx_12 = -nx_12;
            ny_12 = -ny_12;
        }

        /* 回傳遠方延伸點（長度 100000）*/
        return { mid_12.x + nx_12 * 100000.0, mid_12.y + ny_12 * 100000.0 };
    }
    double GetCrossProduct(const point& A, const point& B, const point& C)
    {
        /* 使用外積判斷ABC三點順時針/逆時針關係 */
		// 向量AB = [(B.x - A.x), (B.y - A.y)]
		// 向量AC = [(C.x - A.x), (C.y - A.y)]
		// 計算向量AB與AC外積, 正: 逆時針, 負: 順時針, 0: 共線
        return (B.x - A.x) * (C.y - A.y) - (B.y - A.y) * (C.x - A.x);
    }
    void CreateVoronoi(vector<point>& vp, HWND hwnd)
    {
        if (vp.size() < 2)
        {
            return;
        }

        else if (vp.size() == 2)
        {
            /* 畫中垂線 */
            point p1 = vp[0];
            point p2 = vp[1];

            point mid = MidPoint(p1, p2);
            double dx = p2.x - p1.x;
            double dy = p2.y - p1.y;
            double nx = -dy;
            double ny = dx;

            double from_x = mid.x - nx * 100000.0;
            double from_y = mid.y - ny * 100000.0;
            double to_x = mid.x + nx * 100000.0;
            double to_y = mid.y + ny * 100000.0;
            voronoi_edge.push_back({ from_x, from_y, to_x, to_y });

            /* output text */
            wchar_t msg[256];
            swprintf(msg, 256, L"MidPoint: (%.1f, %.1f)", mid.x, mid.y);
            MessageBox(hwnd, msg, L"Result", MB_OK);

            return;
        }

        else if (vp.size() == 3)
        {
            /* 畫外心到各中垂線 */
            point p1 = vp[0];
            point p2 = vp[1];
            point p3 = vp[2];
            point circumcentre = GetCircumCentre(p1, p2, p3);

            /* 線段起點{from_x, from_y} & 終點{to_x, to_y} */
            double from_x = circumcentre.x;
            double from_y = circumcentre.y;

            for (int i = 0; i < vp.size() - 1; ++i)
            {
                for (int j = i + 1; j < vp.size(); ++j)
                {
                    // 找出第三點
                    point p_other;
                    for (const auto& p : vp)
                    {
                        if (!(p.x == vp[i].x && p.y == vp[i].y) &&
                            !(p.x == vp[j].x && p.y == vp[j].y))
                        {
                            p_other = p;
                            break;
                        }
                    }

                    point end_point = GetEndPoint(vp[i], vp[j], p_other);
                    double to_x = end_point.x;
                    double to_y = end_point.y;

                    voronoi_edge.push_back({ from_x, from_y, to_x, to_y });
                }
            }

            /* output text */
            wchar_t msg[256];
            swprintf(msg, 256, L"Circumcentre: (%.1f, %.1f)", circumcentre.x, circumcentre.y);
            MessageBox(hwnd, msg, L"Result", MB_OK);

            //wchar_t msg[256];
            //swprintf(msg, 256, L"Added edge: (%.1f, %.1f) -> (%.1f, %.1f)", from_x, from_y, to_x, to_y);
            //MessageBox(hwnd, msg, L"Debug", MB_OK);

            return;
        }

        else if (vp.size() > 3)
        {
            /* 1. Divide： sort by ascending order */
            sort(vp.begin(), vp.end(), [](const point& a, const point& b)
                {
                    return a.x < b.x;
                });
            /* 2. Divide： 將排序過的點切成左右兩群 */
            int cut = vp.size() / 2;
            vector<point> left(vp.begin(), vp.begin() + cut);
            vector<point> right(vp.begin() + cut, vp.end());

            /* 3. Conquer： Recursion遞迴計算左右兩邊的 Voronoi */
            CreateVoronoi(left, hwnd);
            CreateVoronoi(right, hwnd);

            /* 4. Merge： 將左右 Voronoi 合併 */
            // 用外積公式GetCrossProduct判斷ABC是順時針 or 逆時針, 正: 逆時針, 負: 順時針, 0: 共線
            if (left.size() == 2 && right.size() == 2)
            {
                point A = left[cut - 1]; // 左邊最後一個點
                point B = right[0];              // 右邊第一個點
			}
            else if (left.size() == 2)
            {

            }
            else
            {

            }
        }
    }
};

/* 實際內容 */
/* HWND : Handle to Window 被操作的視窗編號*/
/* UINT : Unsigned Int（訊息 ID）事件類型，例如 WM_PAINT, WM_LBUTTONDOWN*/
/* WPARAM : Word Parameter（16/32位）訊息的額外資訊（例如按下的是哪顆鍵）*/
/* LPARAM : Long Parameter（32/64位）通常包含滑鼠位置等資料*/
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_COMMAND:
    {
        wchar_t msgbuf[128];
        swprintf(msgbuf, 128, L"voronoi_point.size = %d", (int)voronoi_point.size());
        MessageBox(hwnd, msgbuf, L"DEBUG", MB_OK);

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

            VoronoiFunc vf;
            vf.CreateVoronoi(voronoi_point, hwnd);

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
            double x = LOWORD(lParam);
            double y = HIWORD(lParam);
            if (x <= 600)
            {
                voronoi_point.push_back({ x, y }); /* 存入座標 */
                InvalidateRect(hwnd, NULL, TRUE);
            }
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
                MoveToEx(hdc, (int)e.x1, (int)e.y1, NULL);
                LineTo(hdc, (int)e.x2, (int)e.y2);
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