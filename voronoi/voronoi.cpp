﻿#include <Windows.h>
#include <vector>
#include <algorithm>
#include <set>
#include <fstream>
#include <iostream>
#include <string>       // for std::string 和 std::stoi
#include <sstream>      // for std::stringstream（簡稱 ss）
#include <commdlg.h>    // GetOpenFileName 要用
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
    CreateWindow(L"BUTTON", L"Load File", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
        620, 150, 120, 30, hwnd, (HMENU)3, hInstance, NULL);
    CreateWindow(L"BUTTON", L"Previous Case", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
        620, 200, 120, 30, hwnd, (HMENU)4, hInstance, NULL);
    CreateWindow(L"BUTTON", L"Next Case", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
        620, 250, 120, 30, hwnd, (HMENU)5, hInstance, NULL);
    CreateWindow(L"BUTTON", L"Ouput Result", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
        620, 300, 120, 30, hwnd, (HMENU)6, hInstance, NULL);
    CreateWindow(L"BUTTON", L"Refresh", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
        620, 350, 120, 30, hwnd, (HMENU)7, hInstance, NULL);

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

const int canvas_w = 600;                 // 畫布長寬
const int canvas_h = 600;

// 資料結構
struct point                              
{
    double x, y;

    // 制定point的'小於'規則，使得 set 在插入元素時能夠排序並排除重複
    // 如果 x 不相等（差距超過誤差 1e-8），就以 x 小的為小。
    // 否則以 y 小的為小。
    // 這也表示只有 x 和 y 都差在 1e-8 內時，才視為同一個點。
    bool operator<(const point& other) const {
        if (fabs(x - other.x) > 1e-8) return x < other.x;
        return y < other.y;
    }
};                    
struct line
{
    double x1, y1, x2, y2;
};

vector<point> voronoi_point;              // 儲存點的座標
bool enable_mouse_input = false;          // on / off 滑鼠輸入
vector<line> voronoi_edge;                // 儲存要畫出&輸出的的邊
bool enable_edge_create = false;          // on / off 畫出邊線
vector<line> voronoi_edge_source;         // 儲存生成邊的兩點座標{x1, y1, x2, y2}為了計算 GetIntersec()
vector<line> hyperplane;                  // 儲存要畫出的hyperplane線段
vector<vector<point>> test_cases;         // 儲存每一筆輸入的測資點集
int current_case = 0;                     // 當前測資點集編號
const point INVALID_POINT = {-1e9, -1e9}; // 設定無效點，供後面判斷有無外心、有無交點

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

        /* 算: 方向向量{dx, dy}, 法向量{nx, ny} = 中垂線的方向向量 */
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

        // 中垂線公式 
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
        // 用 Cramer's Rule 解 det -> t
        double det = ax * by - ay * bx;

        if (fabs(det) < 1e-8) // 若det = 0->表示無內積->兩中垂平行->無外心
        {
            return INVALID_POINT;
        }

        double t = (by * cx - bx * cy) / det; // 算出t (也可以算出s = ay * cx - ax * cy)

        return { mid_12.x + t * nx_12, mid_12.y + t * ny_12 }; // 回傳t對應的L12公式 (若計算s回傳L23公式即可)
    }
    line GetEndNxNyMid(const point& p1, const point& p2, const point& p_other)
    {
        /* 算中點 */
        point mid_12 = MidPoint(p1, p2);

        /* 算{dx, dy}: 方向向量, {-dy, dx}: 法向量{nx, ny} */
        double dx_12 = p2.x - p1.x;
        double dy_12 = p2.y - p1.y;
        double nx_12 = -dy_12;
        double ny_12 = dx_12;

        /* 使用點積判斷方向（第三點->中點的向量 * p1&p2法向量） */
        // 幾何意義: v & w 的點積可以看做 w 朝著過原點朝 v 的直線上投影，再將投影長度乘上v的長度
        // 對 '第三點->中點的向量' & 'p1&p2法向量' 算點積 dot = nx_12 * mid_other_x + ny_12 * mid_other_y 
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

        /* 回傳end方向向量 */
        return { nx_12, ny_12, mid_12.x, mid_12.y };
        /* 回傳end延伸點（長度 100000）*/
        //return { mid_12.x + nx_12 * 100000.0, mid_12.y + ny_12 * 100000.0 };
    }
    double GetCrossProduct(const point& A, const point& B, const point& C)
    {
        /* 使用外積判斷ABC三點順時針/逆時針關係 */
		// 向量AB = [(B.x - A.x), (B.y - A.y)]
		// 向量AC = [(C.x - A.x), (C.y - A.y)]
		// 計算向量AB與AC外積, 正: 逆時針, 負: 順時針, 0: 共線
        return (B.x - A.x) * (C.y - A.y) - (B.y - A.y) * (C.x - A.x);
    }
    point GetIntersec(const line& upper_bisector, const line& voronoi_edge, const line& voronoi_edge_source) /* 回傳交點xy座標，高於過去y值就更新 */
    {
        /* 上中垂線公式 */
        // 算中點
        point up_p1 = { upper_bisector.x1, upper_bisector.y1 };
        point up_p2 = { upper_bisector.x2, upper_bisector.y2 };
        point up_point = MidPoint(up_p1, up_p2);
        // 算中垂線方向向量 up_dx, up_dy
        double up_dx = upper_bisector.x2 - upper_bisector.x1;
        double up_dy = upper_bisector.y2 - upper_bisector.y1;
        // 中垂線公式: 因為這邊用中垂線求出公式，所以直接使用自己的方向向量，法向量反而不是我們要的
        // x = up_point.x + up_dx * 變數t
        // y = up_point.y + up_dy * 變數t

        /* voronoi_edge公式 */
        // 取點
        point v_point = { voronoi_edge.x1, voronoi_edge.y1 };
        // 算方向向量
        double v_dx = voronoi_edge.x2 - voronoi_edge.x1;
        double v_dy = voronoi_edge.y2 - voronoi_edge.y1;
        /* voronoi_edge公式 */
        // x = v_point.x + v_dx * 變數s
        // y = v_point.y + v_dy * 變數s

        /* 解聯立方程式求t */
        // up_dx * 變數t - v_dx * 變數s = v_point.x - up_point.x
        // up_dy * 變數t - v_dy * 變數s = v_point.y - up_point.y
        double ax = up_dx; // 轉換表達式，以便計算
        double bx = -v_dx;
        double cx = v_point.x - up_point.x;
        double ay = up_dy;
        double by = -v_dy; 
        double cy = v_point.y - up_point.y;

        // ax * t + bx * s = cx
        // ay * t + by * s = cy
        // Cramer's Rule
        double det = ax * by - ay * bx;

        if (fabs(det) < 1e-8) // if det = 0->表示無內積->兩中垂平行->無交點
        {
            return INVALID_POINT; // 回傳無效點
        }

        double t = (cx * by - cy * bx) / det; // 算出t

        double intersec_x = up_point.x + up_dx * t; // 交點座標(需判斷是真or假交點)
        double intersec_y = up_point.y + up_dy * t;

        double x_min = min(voronoi_edge.x1, voronoi_edge.x2); // 設定真實voronoi_edge的x有效範圍
        double x_max = max(voronoi_edge.x1, voronoi_edge.x2);
        if (intersec_x >= x_min && intersec_x <= x_max) // 若交點確實落在voronoi_edge而不是虛擬交點
        {
            return { up_point.x + up_dx * t, up_point.y + up_dy * t }; // 回傳交點座標
        }
        else
        {
            return INVALID_POINT; // 若是虛擬延長線上的假交點，則回傳無效點
        }
    }
    void DecideNewUpperLine(point& upper_l, point& upper_r, const line& highest_source) /* 更新新一輪的上切線 */
    {
        if (highest_source.x1 == upper_l.x && highest_source.y1 == upper_l.y) {
            upper_l.x = highest_source.x2;
            upper_l.y = highest_source.y2;
            
        }
        else if (highest_source.x1 == upper_r.x && highest_source.y1 == upper_r.y) {
            upper_r.x = highest_source.x2;
            upper_r.y = highest_source.y2;
        }
        else if (highest_source.x2 == upper_l.x && highest_source.y2 == upper_l.y) {
            upper_l.x = highest_source.x1;
            upper_l.y = highest_source.y1;
        }
        else { // highest_source.x2 == upper_r.x && highest_source.y2 == upper_r.y
            upper_r.x = highest_source.x1;
            upper_r.y = highest_source.y1;
        }
    }
    bool IsInCanvas(const point& p) // 判斷點是否在畫布內
    {
        return (p.y <= canvas_h);   // (暫時)只用y座標判斷
    }
    void CreateVoronoiEdge(vector<point>& vp, HWND hwnd)
    {
        // 先依照x座標排序
        sort(vp.begin(), vp.end(), [](const point& a, const point& b)
            {
                return a.x < b.x;
            });

        // 篩掉重複的點, set不允許重複元素出現，因為使用紅黑樹資料結構
        set<point> up(vp.begin(), vp.end()); // 須設定point的比大小規則於資料結構宣告中
        vector<point> fp(up.begin(), up.end());
        vp = fp;


        if (vp.size() < 2)
        {
            return;
        }

        if (vp.size() == 2)
        {
            /* 畫中垂線 */
            point p1 = vp[0];
            point p2 = vp[1];

            point mid = MidPoint(p1, p2);
            double dx = p2.x - p1.x;
            double dy = p2.y - p1.y;
            double nx = -dy;
            double ny = dx;

            double from_x = mid.x + nx * 100000.0;
            double from_y = mid.y + ny * 100000.0;
            double to_x = mid.x - nx * 100000.0;
            double to_y = mid.y - ny * 100000.0;
            voronoi_edge.push_back({ from_x, from_y, to_x, to_y }); // 由畫面上到下
            voronoi_edge_source.push_back({p1.x, p1.y, p2.x, p2.y});

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

            // if無外心(共線)，要特別處理 -> 畫出中垂線
            if (circumcentre.x == INVALID_POINT.x && circumcentre.y == INVALID_POINT.y)
            {
                vector<point> p12 = { p1, p2 };
                vector<point> p23 = { p2, p3 };
                CreateVoronoiEdge(p12, hwnd);
                CreateVoronoiEdge(p23, hwnd);
            }
            else // 有外心
            {
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
                            if (!(p.x == vp[i].x && p.y == vp[i].y) && // 若此點不等於當前ij兩點，則此點就為第三點
                                !(p.x == vp[j].x && p.y == vp[j].y))
                            {
                                p_other = p;
                                break;
                            }
                        }

                        line end_point = GetEndNxNyMid(vp[i], vp[j], p_other); // 得到 voronoi_edge 要延伸的方向(遠離第三點)
                        double to_x = end_point.x2 + end_point.x1 * 100000.0;  // mid_x + nx * 100000
                        double to_y = end_point.y2 + end_point.y1 * 100000.0;  // mid_y + ny * 100000

                        voronoi_edge.push_back({ from_x, from_y, to_x, to_y });
                        voronoi_edge_source.push_back({ vp[i].x, vp[i].y, vp[j].x, vp[j].y });
                    }
                }

                /* output text */
                wchar_t msg[256];
                swprintf(msg, 256, L"Circumcentre: (%.1f, %.1f)", circumcentre.x, circumcentre.y);
                MessageBox(hwnd, msg, L"Result", MB_OK);

                //wchar_t msg[256];
                //swprintf(msg, 256, L"Added edge: (%.1f, %.1f) -> (%.1f, %.1f)", from_x, from_y, to_x, to_y);
                //MessageBox(hwnd, msg, L"Debug", MB_OK);
            }
            return;
        }

        else if (vp.size() > 3)
        {
            vector<line> this_rcs_hyperl;     // 暫存此次recursion產生的hyperplane線段(上切中垂線)
            vector<line> this_rcs_hyperl_src; // 其對應的生成點

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
            CreateVoronoiEdge(left, hwnd);
            CreateVoronoiEdge(right, hwnd);

            /* 4. Merge： 將左右 Voronoi 合併 */
            // 用外積公式GetCrossProduct判斷ABC是順時針 or 逆時針, 正: 逆時針, 負: 順時針, 0: 共線

            /* 找上切線 */
            point L = left.back();   // 左邊最後一個點
			point R = right.front(); // 右邊第一個點
            point upper_l = L;
            point upper_r = R;
            line last_upline = { INVALID_POINT.x, INVALID_POINT.y, INVALID_POINT.x, INVALID_POINT.y }; // 用來比對上切線是否已得到最終結果(卡死)

            while (upper_l.x != last_upline.x1 || upper_l.y != last_upline.y1
                || upper_r.x != last_upline.x2 || upper_r.y != last_upline.y2)
            {
                last_upline = { upper_l.x, upper_l.y, upper_r.x, upper_r.y }; // 重要! 紀錄上次的上切線

                for (int i = 0; i < left.size() - 1; ++i) // 找左邊最上點
                {
                    //if (i != left.size() - 1) // 略過最後一個點
                    {
                        if (GetCrossProduct(left[i], upper_l, upper_r) < 0) // 若順時針排列
                        {
                            upper_l = left[i]; // update左邊最上點 = upper_l
                        }
                    }
                }

                for (int j = 1; j < right.size(); ++j) // 找右邊最上點
                {
                    //if (j != 0) // 略過第一個點
                    {
                        if (GetCrossProduct(upper_l, upper_r, right[j]) < 0) // 若順時針排列
                        {
                            upper_r = right[j]; // update右邊最上點 = upper_r
                        }
                    }
                }
                // 得到上切線 upper_line = upper_l -> upper_r = { upper_l.x, upper_l.y, upper_r.x, upper_r.y }
            }

            /* 畫出hyperplane */
            point highest_intersec;              // 最高交點: 初始化為中垂線上最下方點
            line highest_source;                 // 最高交點對應的原voronoi_points {x1, y1, x2, y2}
            int highest_edge_num = -2;           // 擁有最高交點的voronoi_edge編號，紀錄以供裁切，初始值為-1
            point last_intersec = INVALID_POINT; // 上一輪的交點，作為新一輪中垂線的起點
            point intersec;                      // 比較交點上下用
            vector<int> used_edge;               // 紀錄被選中的voronoi_edge編號，避免再次被選中產生衝突
            vector<point> used_inter;            // 紀錄使用過的交點座標

            // 當初次執行 or 上一輪交點在畫布內時，重複執行
            //for (int i = 0; i < 4; ++i)
            while(highest_edge_num != -1)
            //while (IsInCanvas(last_intersec) || 
            //    (last_intersec.x == INVALID_POINT.x && last_intersec.y == INVALID_POINT.y))
            {
                // (1) 畫出上切線的中垂線 upper_bisector
                point mid = MidPoint(upper_l, upper_r);
                double dx = upper_r.x - upper_l.x;
                double dy = upper_r.y - upper_l.y;
                double nx = -dy; // 法向量
                double ny = dx;

                double from_x;
                double from_y;

                if (last_intersec.x == INVALID_POINT.x && last_intersec.y == INVALID_POINT.y) // 第一次執行時
                {
                    from_x = mid.x - nx * 100000.0;
                    from_y = mid.y - ny * 100000.0;
                }
                else // 若不是初次執行，就從上一個交點作為起頭
                {
                    from_x = last_intersec.x;
                    from_y = last_intersec.y;
                }

                double to_x = mid.x + nx * 100000.0;
                double to_y = mid.y + ny * 100000.0;
                line upper_bisector = { from_x, from_y, to_x, to_y }; // 當前上切線的中垂線
                line upper_line = { upper_l.x, upper_l.y, upper_r.x, upper_r.y }; // 當前上切線
                
                // debug
                //hyperplane.push_back(upper_line);
                //hyperplane.push_back(upper_bisector);

                // (2) 找出此中垂線與當前voronoi_edge最上面的交點, 因為原點在左上方，因此y值越小越上方
                // initialization
                highest_intersec = { to_x, to_y };            // 最高交點: 初始化為中垂線上最下方點
                highest_source = {0, 0, 0, 0};                // 最高交點對應的原voronoi_points {x1, y1, x2, y2}
                highest_edge_num = -1;                        // 擁有最高交點的voronoi_edge編號，紀錄以供裁切，初始值為-1
                
                for (int i = 0; i < voronoi_edge.size(); ++i) // 遍歷當前所有voronoi_edges
                {
                    intersec = GetIntersec(upper_bisector, voronoi_edge[i], voronoi_edge_source[i]);

                    if (intersec.x == INVALID_POINT.x && intersec.y == INVALID_POINT.y)
                    {
                        continue;                             // 若無交點，跳過
                    }

                    // 檢查此交點是否使用過了
                    auto e_exist = find(used_edge.begin(), used_edge.end(), i);   // layer1: 邊是否已使用過; layer2: 此邊對應的交點是否已使用過
                    int idx = distance(used_edge.begin(), e_exist);               // 邊ID對應的index
                    if (e_exist == used_edge.end() || (e_exist != used_edge.end() && used_inter[idx].x != intersec.x && used_inter[idx].y != intersec.y))
                    {
                        // 若這條(邊->交點)沒重複，進入下一步比較是否為最高交點
                        if (intersec.y < highest_intersec.y)      // 若此交點高過當前最高交點y值, 更新最高交點
                        {
                            highest_intersec = intersec;          // 更新最高交點
                            highest_source = voronoi_edge_source[i];
                            highest_edge_num = i;
                        }
                        else if (intersec.y = highest_intersec.y)
                        {
                            // 如果遇到交點是多線段重疊該如何處理?
                            // 保留這個交點為last_intersec，使接續的hyperplane順利接上
                            // 不要存這個原地畫線的hyperplane line
                        }
                    }
                    else // 若這條(邊->交點)已被選過
                    {
                        continue; // 跳過此條edge，檢查下一條
                    }
                }

                if (highest_edge_num == -1)       // 無交點，表示為最後一條hyperplane，離開畫布
                {
                    upper_bisector = { from_x, from_y, to_x, to_y }; // 畫出這條收尾的中垂線，從last_intersec -> 原本to_x & to_y
                    hyperplane.push_back(upper_bisector);
                    this_rcs_hyperl.push_back(upper_bisector);
                    this_rcs_hyperl_src.push_back({ upper_l.x, upper_l.y, upper_r.x, upper_r.y });
                    break;                                           // 再跳出while
                }
                used_edge.push_back(highest_edge_num);
                used_inter.push_back(highest_intersec); // 記錄這一輪被選中的交點座標，避免後續重複使用

                // 更新last_ontersec給下一輪中垂線使用
                last_intersec = highest_intersec;

                // (3) 切割
                // 中垂線 upper_bisector 要從交點被切割，留下 from -> highest_intersec 這一段
                upper_bisector = { from_x, from_y, highest_intersec.x, highest_intersec.y };
                hyperplane.push_back(upper_bisector); // 存入hyperplane線段集

                // voronoi_edge也從交點被切割，留下從外心到交點的線段，直接從voronoi_edge中修改不須另外儲存
                double org_from_x = voronoi_edge[highest_edge_num].x1; // 保存original座標
                double org_from_y = voronoi_edge[highest_edge_num].y1;
                double org_to_x = voronoi_edge[highest_edge_num].x2;
                double org_to_y = voronoi_edge[highest_edge_num].y2;

                // 方向向量from -> to (2點情況: 畫面上 -> 下) (3點情況: 外心 -> 延伸)
                double org_nx = org_to_x - org_from_x;
                double org_ny = org_to_y - org_from_y;

                double checkp1_x = highest_intersec.x + org_nx * 1; //(2: 較下面的點) (3: 遠離外心的點)
                double checkp1_y = highest_intersec.y + org_ny * 1;
                double checkp2_x = highest_intersec.x - org_nx * 1; //(2: 較上面的點) (3: 靠向外心的點)
                double checkp2_y = highest_intersec.y - org_ny * 1;

                // (2點情況: 下方點離source點的距離) (3點情況: 遠離外心的點離source點的距離)
                double dis1 = sqrt(pow(checkp1_x - voronoi_edge_source[highest_edge_num].x1, 2) + pow(checkp1_y - voronoi_edge_source[highest_edge_num].y1, 2));
                // (2點情況: 上方點離source點的距離) (3點情況: 靠向外心的點離source點的距離)
                double dis2 = sqrt(pow(checkp2_x - voronoi_edge_source[highest_edge_num].x1, 2) + pow(checkp2_y - voronoi_edge_source[highest_edge_num].y1, 2));

                if (dis1 > dis2) // (2: 表示下方點比較遠->留住上方線段，保留from_x, from_y) (3: 表示遠離外心的點比較遠->留住靠向外心的線段，保留from_x, from_y)

                {
                    voronoi_edge[highest_edge_num] = { org_from_x, org_from_y, highest_intersec.x, highest_intersec.y };
                }
                else if (dis2 > dis1) // (2: 表示上方點比較遠->留住下方線段，保留to_x, to_y) (3: 表示靠向外心的點比較遠->留住遠離外心的線段，保留to_x, to_y)
                {
                    voronoi_edge[highest_edge_num] = { highest_intersec.x, highest_intersec.y, org_to_x, org_to_y };
                }
                else // dis1 = dis2 表示交點在兩點中點上，要特殊處理 用getendpoint
                {
                    point A = { org_from_x, org_from_y };     // voronoi兩點
                    point B = { org_to_x, org_to_y };
                    point Other = { from_x, from_y };         // 中垂線起始點
                    line end = GetEndNxNyMid(A, B, Other);
                    if (end.x2 != org_nx || end.y2 != org_ny) // 若得出的延伸方向與原方向向量不同，表示要反轉延伸方向，把原本的from改到to的位置
                    {
                        voronoi_edge[highest_edge_num] = { highest_intersec.x, highest_intersec.y, org_from_x, org_from_y };
                    }
                    else                                      // 若得出的延伸方向與原方向相同
                    {
                        voronoi_edge[highest_edge_num] = { highest_intersec.x, highest_intersec.y, org_to_x, org_to_y };
                    }
                }
                
                // 不論此次有無找到交點，裁好的中垂線在下一次recursion要變成voronoi_edge來處理
                this_rcs_hyperl.push_back(upper_bisector); // 暫存
                this_rcs_hyperl_src.push_back({ upper_l.x, upper_l.y, upper_r.x, upper_r.y });
                // if (intersec.y = highest_intersec.y) // 如果此次交點與上一次一樣，表示中垂線根本在原地打轉
                // if (intersec.y != highest_intersec.y)   // 交點不同時才需要存入此recursion的中垂線紀錄
                
                // (4) 更新上切線, 假設此voronoi_edge由左邊的點 upper_l & B 產生，則上切線左邊端點改成 B, upper_l = B
                DecideNewUpperLine(upper_l, upper_r, highest_source);

                // hyperplane離開畫布前重複執行以上步驟直到hyperplane離開畫布
            }

            // (5) 最後將暫存的hyperplane存入 voronoi_edge 供下一輪recursion使用
            for (int i = 0; i < this_rcs_hyperl.size(); ++i)
            {
                voronoi_edge.push_back(this_rcs_hyperl[i]);
                voronoi_edge_source.push_back(this_rcs_hyperl_src[i]);
            }
            
        }
    }
};

/* 實際內容 */
/* HWND : Handle to Window 被操作的視窗編號 */
/* UINT : Unsigned Int（訊息 ID）事件類型，例如 WM_PAINT, WM_LBUTTONDOWN */
/* WPARAM : Word Parameter（16/32位）訊息的額外資訊（例如按下的是哪顆鍵）*/
/* LPARAM : Long Parameter（32/64位）通常包含滑鼠位置等資料 */
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_COMMAND:
    {
        int button_id = LOWORD(wParam); // 獲取滑鼠點選的按鍵ID

        switch(button_id)
        {
        case 1: // Mouse Input
        {
            enable_mouse_input = true;
            voronoi_edge.clear();
            voronoi_edge_source.clear();
            hyperplane.clear();
            break;
        }
        case 2: // Execute
        {
            enable_mouse_input = false;
            enable_edge_create = true;
            voronoi_edge.clear(); /* 清除舊邊 */
            voronoi_edge_source.clear();
            hyperplane.clear();

            VoronoiFunc vf;
            vf.CreateVoronoiEdge(voronoi_point, hwnd);

            InvalidateRect(hwnd, NULL, TRUE);

            break;
        }
        case 3: // Load file
        {
            voronoi_point.clear();
            voronoi_edge.clear();
            voronoi_edge_source.clear();
            hyperplane.clear();
            enable_edge_create = true;

            OPENFILENAME ofn;          // 共用對話框結構
            TCHAR szFile[260] = { 0 }; // 儲存檔名

            ZeroMemory(&ofn, sizeof(ofn));
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = hwnd;
            ofn.lpstrFile = szFile;
            ofn.nMaxFile = sizeof(szFile);
            ofn.lpstrFilter = TEXT("Text Files\0*.txt\0All Files\0*.*\0");
            ofn.nFilterIndex = 1;
            ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

            if (GetOpenFileName(&ofn) == TRUE)
            {
                // 使用者選好了檔案，開始讀檔
                ifstream fin(ofn.lpstrFile);
                string line;
                vector<point> each_case;
                test_cases.clear();
                current_case = 0;

                while (getline(fin, line)) {
                    double x, y, a, b;
                    string z;

                    if (line.empty() || line[0] == '#') continue; // 當此行開頭為# or 空白時，跳過

                    if (line[0] == 'P')
                    {
                        stringstream s(line);        // 把 line 裡的內容（例如一行是 "300 400"）包裝進 stringstream，然後用像 cin 的方式讀出來
                        s >> z >> x >> y;            // 會得到 x = 300, y = 400
                        voronoi_point.push_back({ x, y });
                    }

                    else if (line[0] == 'E')
                    {
                        stringstream ss(line);       // 把 line 裡的內容（例如一行是 "300 400"）包裝進 stringstream，然後用像 cin 的方式讀出來
                        ss >> z >> x >> y >> a >> b; // 會得到 x = 300, y = 400
                        voronoi_edge.push_back({ x, y, a, b });
                    }

                    else // 開頭不是'P'or'E'的字串
                    {
                        int n = stoi(line);         // 讀入第一行，n = 此筆測資點數量
                        if (n == 0)                 // 停止條件
                        {               
                            break;
                        }

                        each_case.clear();
                        for (int i = 0; i < n; ++i) {      // 重複n次，把此筆測資的點都讀入
                            while (getline(fin, line)) {
                                if (line.empty() || line[0] == '#') continue;
                                stringstream sss(line);    // 把 line 裡的內容（例如一行是 "300 400"）包裝進 stringstream，然後用像 cin 的方式讀出來
                                sss >> x >> y;             // 會得到 x = 300, y = 400
                                each_case.push_back({ x, y });
                                break;
                            }
                        }

                        test_cases.push_back(each_case);
                    }
                    
                }

                if (!test_cases.empty()) {
                    voronoi_point = test_cases[0];

                    // 顯示第一筆測資點集數量及座標供檢查
                    wstring info = L"點數: " + to_wstring(test_cases[0].size()) + L"\n";
                    for (const auto& p : test_cases[0])
                    {
                        info += L"(" + to_wstring(p.x) + L", " + to_wstring(p.y) + L")\n";
                    }
                    MessageBox(hwnd, info.c_str(), L"Check Points Info", MB_OK); // .c_str(): 將wstring轉成C可讀取的w_char_t
                }
                InvalidateRect(hwnd, NULL, TRUE);
            }

            break;
        }
        case 4: // Previous Case
        {
            if (current_case > 0)
            {
                current_case--;
                voronoi_point = test_cases[current_case];

                wstring info = L"點數: " + to_wstring(test_cases[current_case].size()) + L"\n";
                for (const auto& p : test_cases[current_case])
                {
                    info += L"(" + to_wstring(p.x) + L", " + to_wstring(p.y) + L")\n";
                }
                MessageBox(hwnd, info.c_str(), L"Check Points Info", MB_OK); // .c_str(): 將wstring轉成C可讀取的w_char_t
            
                enable_mouse_input = false;
                voronoi_edge.clear();
                voronoi_edge_source.clear();
                hyperplane.clear();
                InvalidateRect(hwnd, NULL, TRUE);
            }
            break;
        }
        case 5: // Next Case
        {
            if (current_case < test_cases.size() - 1)
            {
                current_case++;
                voronoi_point = test_cases[current_case];

                wstring info = L"點數: " + to_wstring(test_cases[current_case].size()) + L"\n";
                for (const auto& p : test_cases[current_case])
                {
                    info += L"(" + to_wstring(p.x) + L", " + to_wstring(p.y) + L")\n";
                }
                MessageBox(hwnd, info.c_str(), L"Check Points Info", MB_OK); // .c_str(): 將wstring轉成C可讀取的w_char_t

                enable_mouse_input = false;
                voronoi_edge.clear();
                voronoi_edge_source.clear();
                hyperplane.clear();
                InvalidateRect(hwnd, NULL, TRUE);
            }
            break;
        }

        case 6: // Export Results
        {
            // 1. 開啟儲存檔案對話框
            OPENFILENAME ofn;
            TCHAR szFile[260] = TEXT("result.txt");
            ZeroMemory(&ofn, sizeof(ofn));
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = hwnd;
            ofn.lpstrFile = szFile;
            ofn.nMaxFile = sizeof(szFile);
            ofn.lpstrFilter = TEXT("Text Files\0*.txt\0All Files\0*.*\0");
            ofn.nFilterIndex = 1;
            ofn.lpstrTitle = TEXT("Export Voronoi Result");
            ofn.Flags = OFN_OVERWRITEPROMPT;

            if (GetSaveFileName(&ofn) == TRUE)
            {
                // 2. 開始輸出
                ofstream fout(ofn.lpstrFile);
                if (!fout.is_open())
                {
                    MessageBox(hwnd, TEXT("無法打開檔案"), TEXT("錯誤"), MB_OK | MB_ICONERROR);
                    break;
                }

                // 3. 輸出點
                for (const auto& p : voronoi_point)
                {
                    fout << "P " << p.x << " " << p.y << endl;
                }

                // 4. 輸出邊
                for (const auto& e : voronoi_edge)
                {
                    fout << "E " << e.x1 << " " << e.y1 << " " << e.x2 << " " << e.y2 << endl;
                }

                // 5. 結束條件
                fout << "0" << endl;

                fout.close();
                MessageBox(hwnd, TEXT("Output result to result.txt"), TEXT("Output successful"), MB_OK);
            }
            voronoi_point.clear();
            voronoi_edge.clear();
            voronoi_edge_source.clear();
            hyperplane.clear();
            break;
        }

        case 7: // Refresh
        {
            enable_mouse_input = false;
            voronoi_point.clear();
            voronoi_edge.clear();
            voronoi_edge_source.clear();
            hyperplane.clear();
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
            if (x <= canvas_w) // 限制繪點範圍，避免與點擊按鍵產生衝突，誤產生voronoi point
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
        HDC hdc = BeginPaint(hwnd, &ps); // 請作業系統傳給我合法、可繪製的 hdc（繪圖上下文），準備好重繪區域

        /* 畫出所有點 */
        for (const auto& p : voronoi_point)
        {
            Ellipse(hdc, p.x - 3, p.y - 3, p.x + 3, p.y + 3); /* draw a point */
        }

        if (enable_edge_create)
        {
            /* 畫出所有 voronoi 邊 */
            HPEN hPen = CreatePen(PS_SOLID, 1, RGB(0, 100, 150)); // HPEN = Handle Pen
            HPEN hOldPen = (HPEN)SelectObject(hdc, hPen); // SelectObject() 的回傳值是「原本」使用的 GDI 對象，在這裡是原本的 HPEN，此行目的: 儲存舊的 pen（畫筆）設定，等繪圖完畢後能恢復
            for (const auto& e : voronoi_edge)
            {
                MoveToEx(hdc, (int)e.x1, (int)e.y1, NULL);
                LineTo(hdc, (int)e.x2, (int)e.y2);
            }
            SelectObject(hdc, hOldPen); // 恢復原本的 pen
            DeleteObject(hPen);         // 釋放自己建立的 hPen，避免資源洩漏

            /* 畫出所有 hyperplane 邊 */
            HPEN hHyperPen = CreatePen(PS_SOLID, 2, RGB(200, 140, 0));
            HPEN hHyperOldPen = (HPEN)SelectObject(hdc, hHyperPen);
            for (const auto& h : hyperplane)
            {
                MoveToEx(hdc, (int)h.x1, (int)h.y1, NULL);
                LineTo(hdc, (int)h.x2, (int)h.y2);
            }
            SelectObject(hdc, hHyperOldPen);
            DeleteObject(hHyperPen);

            enable_edge_create = false;
        }
        
        EndPaint(hwnd, &ps); // 跟作業系統說畫完了，釋放hdc，關閉WM_PAINT 專屬繪圖流程
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