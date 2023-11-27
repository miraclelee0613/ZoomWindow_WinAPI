#include <Windows.h>
#ifndef GET_X_LPARAM
#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#endif

#ifndef GET_Y_LPARAM
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))
#endif
#include <magnification.h>

#pragma comment (lib, "Magnification.lib")

// 전역 변수
HINSTANCE hInst;
HWND hwndMag;
HWND hwndHost;
RECT magWindowRect = { 0, 0, 300, 300 }; // 확대 창의 크기와 위치

float zoomFactor = 1.0f; // 확대/축소 배율

bool isMiddleButtonDown = false;
POINT lastCursorPos;  // 마지막 커서 위치 저장

// 타이머 설정
#define MAG_TIMER_ID 1
#define MAG_TIMER_ELAPSE 30 // 30ms 간격으로 업데이트
boolean cursorHidden = false;
// 함수 선언
BOOL InitializeMagnification(HINSTANCE hInstance, int nCmdShow);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // 윈도우 클래스 등록
    WNDCLASSEX wcex = {};
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.hCursor = LoadCursor(nullptr, IDC_CROSS);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszClassName = L"SampleWindowClass";
    RegisterClassEx(&wcex);

    // 돋보기 초기화
    if (!InitializeMagnification(hInstance, nCmdShow)) {
        return FALSE;
    }

    // WinMain 함수 내에서 타이머 설정
    SetTimer(hwndHost, 1, 15, NULL);

    // 메시지 루프
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}

// 돋보기 기능 초기화
BOOL InitializeMagnification(HINSTANCE hInstance, int nCmdShow) {
    // 돋보기 컨트롤 초기화
    MagInitialize();

    // 메인 윈도우 생성
    hwndHost = CreateWindowEx(
        WS_EX_TOPMOST | WS_EX_TRANSPARENT,  // 확장 스타일에 WS_EX_TRANSPARENT 추가  | WS_EX_LAYERED
        L"SampleWindowClass",               // 윈도우 클래스 이름
        L"Screen Magnifier",                // 윈도우 타이틀
        WS_OVERLAPPEDWINDOW,                // 윈도우 스타일
        magWindowRect.left,                 // 시작 위치 X
        magWindowRect.top,                  // 시작 위치 Y
        magWindowRect.right - magWindowRect.left, // 너비
        magWindowRect.bottom - magWindowRect.top, // 높이
        nullptr,                            // 부모 윈도우 핸들
        nullptr,                            // 메뉴 핸들
        hInstance,                          // 인스턴스 핸들
        nullptr);                           // 추가 파라미터

    if (!hwndHost) {
        return FALSE;
    }

    // 돋보기 윈도우 생성
    hwndMag = CreateWindow(WC_MAGNIFIER, L"MagnifierWindow",
        WS_CHILD | MS_SHOWMAGNIFIEDCURSOR | WS_VISIBLE,
        0, 0, magWindowRect.right, magWindowRect.bottom,
        hwndHost, nullptr, hInstance, nullptr);
    if (!hwndMag) {
        return FALSE;
    }

    ShowWindow(hwndHost, nCmdShow);
    UpdateWindow(hwndHost);

    // 돋보기 설정 (여기서는 2배 확대로 설정)
    MAGTRANSFORM matrix;
    memset(&matrix, 0, sizeof(matrix));
    matrix.v[0][0] = 2.0f;
    matrix.v[1][1] = 2.0f;
    matrix.v[2][2] = 1.0f;

    MagSetWindowTransform(hwndMag, &matrix);

    return TRUE;
}

// 돋보기 설정 업데이트 함수
void UpdateMagnification(HWND hwndMag, float zoomFactor) {
    MAGTRANSFORM matrix;
    memset(&matrix, 0, sizeof(matrix));

    // 확대/축소 비율 적용
    matrix.v[0][0] = zoomFactor;
    matrix.v[1][1] = zoomFactor;
    matrix.v[2][2] = 1.0f; // Z축은 항상 1.0으로 고정

    MagSetWindowTransform(hwndMag, &matrix);
}

// 윈도우 프로시저
// WndProc 함수 내에서 WM_TIMER 메시지 처리
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    RECT windowRect;
    RECT clinetRect;
    RECT magRect;
    int width, height;
    //int left, bottom;
    int centerX, centerY;
    static bool isDragging = false;
    static POINT dragStartPoint;
    LONG_PTR style = GetWindowLongPtr(hWnd, GWL_EXSTYLE); // style 변수 선언을 switch 문 밖으로 이동

    switch (message) {
    //case WM_SIZE: {
    //    // 창의 새로운 크기를 얻음
    //    int newWidth = LOWORD(lParam);
    //    int newHeight = HIWORD(lParam);

    //    // hwndHost와 hwndMag의 크기를 업데이트
    //    /*SetWindowPos(hwndHost, NULL, 0, 0, newWidth, newHeight, SWP_NOZORDER | SWP_NOMOVE);*/
    //    SetWindowPos(hwndMag, NULL, 0, 0, newWidth, newHeight, SWP_NOZORDER | SWP_NOMOVE);

    //    // magWindowRect 업데이트
    //    magWindowRect.right = magWindowRect.left + newWidth;
    //    magWindowRect.top = magWindowRect.bottom - newHeight;

    //    // 확대 영역 업데이트
    //    MagSetWindowSource(hwndMag, magWindowRect);
    //    InvalidateRect(hwndMag, NULL, TRUE);
    //    break;
    //}
    case WM_MOUSEWHEEL: {
        if (GET_KEYSTATE_WPARAM(wParam) == MK_CONTROL) {
            // 컨트롤 키가 눌린 상태에서 마우스 휠 이벤트 처리
            int zDelta = GET_WHEEL_DELTA_WPARAM(wParam);
            if (zDelta > 0) {
                // 휠을 위로 스크롤: 확대
                zoomFactor += 0.1f;
            }
            else if (zDelta < 0) {
                // 휠을 아래로 스크롤: 축소
                zoomFactor -= 0.1f;
            }
            // 돋보기 설정 업데이트
            UpdateMagnification(hwndMag, zoomFactor);
        }
        break;
    }
    case WM_KILLFOCUS: {
        // 창이 포커스를 얻었을 때
        LONG_PTR style = GetWindowLongPtr(hWnd, GWL_EXSTYLE);
        // 원하는 스타일 변경을 수행
        SetWindowLongPtr(hWnd, GWL_EXSTYLE, style | WS_EX_LAYERED);
        SetLayeredWindowAttributes(hWnd, 0, 255, LWA_ALPHA); // 예: 투명도 설정
        break;
    }
    case WM_SETFOCUS: {
        // 창이 포커스를 잃었을 때
        LONG_PTR style = GetWindowLongPtr(hWnd, GWL_EXSTYLE);
        // 원하는 스타일 변경을 수행
        SetWindowLongPtr(hWnd, GWL_EXSTYLE, style & ~WS_EX_LAYERED);
        break;
    }
    case WM_TIMER:
        //if (wParam == MAG_TIMER_ID) {
        //    // 현재 창의 위치 및 크기를 얻음
        //    RECT windowRect;
        //    GetWindowRect(hwndHost, &windowRect);

        //    // 창이 위치한 모니터의 핸들을 얻음
        //    HMONITOR hMonitor = MonitorFromWindow(hwndHost, MONITOR_DEFAULTTONEAREST);

        //    MONITORINFO monitorInfo = { sizeof(MONITORINFO) };
        //    if (GetMonitorInfo(hMonitor, &monitorInfo)) {
        //        // 모니터 정보를 바탕으로 확대 영역을 계산
        //        int monitorWidth = monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left;
        //        int monitorHeight = monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top;

        //        // 확대 창의 크기를 고려하여 확대 영역을 설정
        //        int width = windowRect.right - windowRect.left;
        //        int height = windowRect.bottom - windowRect.top;
        //        int left = max(windowRect.left, monitorInfo.rcMonitor.left);
        //        int top = max(windowRect.top, monitorInfo.rcMonitor.top);

        //        // 모니터 경계를 넘지 않도록 조정
        //        left = min(left, monitorInfo.rcMonitor.right - width);
        //        top = min(top, monitorInfo.rcMonitor.bottom - height);

        //        magRect.left = left;
        //        magRect.top = top;
        //        magRect.right = left + width;
        //        magRect.bottom = top + height;

        //        MagSetWindowSource(hwndMag, magRect); // 확대 영역 설정
        //        InvalidateRect(hwndMag, NULL, TRUE);
        //    }
        //}
        //break;
        /*if (wParam == MAG_TIMER_ID) {
            
        }
        */
        break;
    case WM_DESTROY:
        // 타이머 해제
        KillTimer(hWnd, MAG_TIMER_ID);
        MagUninitialize();
        PostQuitMessage(0);
        break;
    case WM_MBUTTONDOWN: {
        // 마우스 휠 클릭시 클릭 좌표를 중심으로 확대
        POINT clickPoint;
        clickPoint.x = GET_X_LPARAM(lParam);
        clickPoint.y = GET_Y_LPARAM(lParam);

        // 클라이언트 좌표를 스크린 좌표로 변환
        ClientToScreen(hWnd, &clickPoint);
        int newWidth = LOWORD(lParam);
        int newHeight = HIWORD(lParam);
        SetWindowPos(hwndHost, NULL, clickPoint.x-(newWidth/2), clickPoint.y-(newHeight/2), newWidth, newHeight, SWP_NOSIZE);
        
        // 확대 영역 설정
        RECT newMagRect;
        newMagRect.left = clickPoint.x - (magWindowRect.right - magWindowRect.left) / 2;
        newMagRect.top = clickPoint.y - (magWindowRect.bottom - magWindowRect.top) / 2;
        newMagRect.right = clickPoint.x + (magWindowRect.right - magWindowRect.left) / 2;
        newMagRect.bottom = clickPoint.y + (magWindowRect.bottom - magWindowRect.top) / 2;

        MagSetWindowSource(hwndMag, newMagRect);
        InvalidateRect(hwndMag, NULL, TRUE);

        // 마우스 휠 클릭시 2배 확대로 복구
        UpdateMagnification(hwndMag, 2.0f);
        return 0;
        /*break;*/
    }
    /*case WM_MOUSEMOVE:
        if (!cursorHidden) {
            ShowCursor(FALSE);
            cursorHidden = true;

            TRACKMOUSEEVENT tme;
            tme.cbSize = sizeof(TRACKMOUSEEVENT);
            tme.dwFlags = TME_LEAVE;
            tme.hwndTrack = hWnd;
            TrackMouseEvent(&tme);
        }
        break;*/
    /*case WM_MOUSELEAVE:
        ShowCursor(TRUE);
        cursorHidden = false;
        break;*/
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    // 공통 코드: 창의 크기나 위치가 변경될 때마다 실행
    /*GetWindowRect(hwndHost, &windowRect);
    GetClientRect(hwndHost, &hostRect);
    centerX = ((windowRect.left + windowRect.right) / 2);
    centerY = ((windowRect.top + windowRect.bottom) / 2);
    width = windowRect.right - windowRect.left;
    height = windowRect.bottom - windowRect.top;

    magRect.left = centerX - (width / 4);
    magRect.right = centerX + (width / 4);
    magRect.top = centerY - (height / 4);
    magRect.bottom = centerY + (height / 4);*/
    // 화면 업데이트 로직
    
    GetWindowRect(hwndHost, &windowRect);
    GetClientRect(hwndHost, &clinetRect);

    // 확대 창의 크기를 고려하여 확대 영역을 설정
    width = clinetRect.right - clinetRect.left;
    height = clinetRect.bottom - clinetRect.top;
    /*int left = windowRect.left;
    int top = windowRect.top;*/

    centerX = windowRect.left + (width / 2);
    centerY = windowRect.bottom - (height / 2);
    //left = windowRect.left;
    //bottom = windowRect.bottom;
    //left = (windowRect.left - monitorInfo.rcMonitor.left);
    //top = (windowRect.top - monitorInfo.rcMonitor.top);
    /*left = min(left, monitorInfo.rcMonitor.right - width);
    top = min(top, monitorInfo.rcMonitor.bottom - height);*/
    magRect.top = centerY - (height / 4);
    magRect.left = centerX - (width / 4);
    magRect.bottom = centerY + (height / 4);
    magRect.right = centerX + (width / 4);

    MagSetWindowSource(hwndMag, magRect); // 확대 영역 설정
    InvalidateRect(hwndMag, NULL, TRUE);

    // 창이 위치한 모니터의 핸들을 얻음
    /*HMONITOR hMonitor = MonitorFromWindow(hwndHost, MONITOR_DEFAULTTONEAREST);

    MONITORINFO monitorInfo = { sizeof(MONITORINFO) };
    GetMonitorInfo(hMonitor, &monitorInfo);*/
    /*if (GetMonitorInfo(hMonitor, &monitorInfo)) {*/
        // 모니터 정보를 바탕으로 확대 영역을 계산
        /*int monitorWidth = monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left;
        int monitorHeight = monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top;*/

        
    /*}*/
    return 0;
}


