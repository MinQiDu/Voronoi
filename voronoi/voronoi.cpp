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
        /* �⤤�I */
        point mid_12 = MidPoint(p1, p2);
        point mid_23 = MidPoint(p2, p3);

        /* ��{dx, dy}: ��V�V�q, {-dy, dx}: �k�V�q{nx, ny} */
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
        // ���p�ߤ�{��, �� t or s
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
        // �� Cramer's rule �� det -> t
        double det = ax * by - ay * bx;

        if (fabs(det) < 1e-8) // �Ydet = 0->��ܵL���n->�⤤������->�L�~��
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

/* �ƫe�ŧi */
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

/* �{���J�f */
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    /* 1. ���U�������O�A���w���� WndProc */
    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc;       /* �i�D Windows �έ��Ө禡�B�z�T�� */
    wc.hInstance = hInstance;
    wc.lpszClassName = L"MyWindow"; /* L"�r�ꤺ�e" -> �ഫ���e�r�� */
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1); // �[�J�I����l
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);      // �[�J��м˦�
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);

    if (!RegisterClass(&wc)) {
        MessageBox(NULL, L"RegisterClass failed!", L"Error", MB_OK);
        return 1;
    }

    /* 2. �إߵ��� */
    HWND hwnd = CreateWindow
                (L"MyWindow",          /* �i�D�t�έn�إߤ@��MyWindow���������� */
                L"Voronoi Visualizer", /* �������D */
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

    /* �إߥ\����� */
    CreateWindow(L"BUTTON", L"Mouse Input", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
        420, 50, 120, 30, hwnd, (HMENU)1, hInstance, NULL);
    CreateWindow(L"BUTTON", L"Execute", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
        420, 100, 120, 30, hwnd, (HMENU)2, hInstance, NULL);
    CreateWindow(L"BUTTON", L"Refresh", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
        420, 150, 120, 30, hwnd, (HMENU)3, hInstance, NULL);

    ShowWindow(hwnd, nCmdShow);        /* �X�{���� */
    UpdateWindow(hwnd);                /* ��ø���� */

    /* 3. �i�J�u�T���j��v�A���ݨϥΪ̨ƥ� */
    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg); /* ��T���e�� WndProc() �h�B�z */
    }

    return 0;
}

/* ��ڤ��e */
/* HWND : Handle to Window �Q�ާ@�������s��*/
/* UINT : Unsigned Int�]�T�� ID�^�ƥ������A�Ҧp WM_PAINT, WM_LBUTTONDOWN*/
/* WPARAM : Word Parameter�]16/32��^�T�����B�~��T�]�Ҧp���U���O������^*/
/* LPARAM : Long Parameter�]32/64��^�q�`�]�t�ƹ���m�����*/


vector<point> voronoi_point; /* �x�s�I���y�� */

bool enable_mouse_input = false;

vector<line> voronoi_edge; /* �x�s�ͦ����� */

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
            voronoi_edge.clear(); /* �M������ */

            if (voronoi_point.size() >= 2)
            {
                if (voronoi_point.size() == 3)
                {
                    VoronoiFunc vf;
                    point p1 = voronoi_point[0];
                    point p2 = voronoi_point[1];
                    point p3 = voronoi_point[2];
                    point circumcentre = vf.GetCircumCentre(p1, p2, p3);
                    
                    /* �u�q�_�I{from_x, from_y} & ���I{to_x, to_y} */
                    double len = 100000.0; // �����ܵL����
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

    case WM_LBUTTONDOWN: /* �ƹ�������U */
    {
        if (enable_mouse_input)
        {
            int x = LOWORD(lParam);
            int y = HIWORD(lParam);
            voronoi_point.push_back({ x, y }); /* �s�J�y�� */
            InvalidateRect(hwnd, NULL, TRUE);
            break;
        }
        else
        {
            break;
        }
        
    }
        
    case WM_PAINT: /* �e��ø�s */
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        /* �e�X�Ҧ��I */
        for (const auto& p : voronoi_point)
        {
            Ellipse(hdc, p.x - 3, p.y - 3, p.x + 3, p.y + 3); /* draw a point */
        }

        if (enable_edge_create)
        {
            /* �e�X�Ҧ��� */
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
        PostQuitMessage(0); /* �o�e WM_QUIT�A���}�{�� */
        break;
    }

    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}