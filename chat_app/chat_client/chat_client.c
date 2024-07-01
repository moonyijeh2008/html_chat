#include <windows.h>
#include <winsock2.h>
#include <stdio.h>

#pragma comment(lib, "ws2_32.lib")

#define IDC_MAIN_EDIT 101
#define IDC_MAIN_BUTTON 102
#define IDC_NAME_EDIT 103
#define IDC_CHANNEL_EDIT 104
#define IDC_CONNECT_BUTTON 105
#define IDC_MESSAGE_EDIT 106
#define BUF_SIZE 1024

HINSTANCE hInst;
HWND hEdit, hNameEdit, hChannelEdit, hMessageEdit;
SOCKET server_socket;
char buffer[BUF_SIZE];
char client_name[32];

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow) {
    WSADATA wsa;
    WNDCLASSA wc = {0};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "ChatClientClass";

    if (!RegisterClassA(&wc)) {
        return -1;
    }

    HWND hwnd = CreateWindowExA(
        0,
        wc.lpszClassName,
        "Chat Client",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 500, 400,
        NULL,
        NULL,
        hInstance,
        NULL
    );

    if (hwnd == NULL) {
        return -1;
    }

    ShowWindow(hwnd, nCmdShow);

    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        MessageBoxA(NULL, "WSAStartup failed", "Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    WSACleanup();
    return 0;
}

// 서버에 연결하는 함수
void connectToServer(HWND hwnd) {
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == INVALID_SOCKET) {
        MessageBoxA(hwnd, "Could not create socket", "Error", MB_OK | MB_ICONERROR);
        return;
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr("112.172.248.92");
    server_addr.sin_port = htons(54972); // 포트 번호

    if (connect(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        MessageBoxA(hwnd, "Connect failed", "Error", MB_OK | MB_ICONERROR);
        return;
    }

    MessageBoxA(hwnd, "Connected to server", "Info", MB_OK | MB_ICONINFORMATION);

    GetWindowTextA(hNameEdit, client_name, sizeof(client_name));
    char channel[32];
    GetWindowTextA(hChannelEdit, channel, sizeof(channel));

    // 사용자 이름과 채널 이름을 서버로 전송
    send(server_socket, client_name, strlen(client_name), 0);
    Sleep(100); 
    send(server_socket, channel, strlen(channel), 0);
}

// 서버로부터 메시지를 수신하는 스레드 함수
DWORD WINAPI receive_message(LPVOID arg) {
    while (1) {
        int len = recv(server_socket, buffer, BUF_SIZE, 0);
        if (len > 0) {
            buffer[len] = '\0';

            // 개행 문자로 메시지 구분
            char* token = strtok(buffer, "\n");
            while (token != NULL) {
                // 새로운 메시지를 추가합니다.
                SendMessageA(hEdit, EM_SETSEL, (WPARAM)-1, (LPARAM)-1); // 커서를 텍스트 끝으로 이동
                SendMessageA(hEdit, EM_REPLACESEL, FALSE, (LPARAM)token);
                SendMessageA(hEdit, EM_REPLACESEL, FALSE, (LPARAM)"\r\n"); // 새로운 줄 추가

                token = strtok(NULL, "\n");
            }
        }
    }
    return 0;
}

// 윈도우 프로시저: 윈도우 메시지를 처리하는 함수
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE: {
            // 채팅 로그를 표시하는 에디트 컨트롤 생성
            hEdit = CreateWindowExA(
                0, "EDIT", NULL,
                WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
                10, 10, 460, 200,
                hwnd, (HMENU)IDC_MAIN_EDIT, hInst, NULL
            );

            // 사용자 이름 입력 에디트 컨트롤 생성
            hNameEdit = CreateWindowExA(
                0, "EDIT", "",
                WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                10, 220, 100, 30,
                hwnd, (HMENU)IDC_NAME_EDIT, hInst, NULL
            );

            // 채널 이름 입력 에디트 컨트롤 생성
            hChannelEdit = CreateWindowExA(
                0, "EDIT", "",
                WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                120, 220, 100, 30,
                hwnd, (HMENU)IDC_CHANNEL_EDIT, hInst, NULL
            );

            // 메시지 입력 에디트 컨트롤 생성
            hMessageEdit = CreateWindowExA(
                0, "EDIT", "",
                WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                10, 260, 360, 30,
                hwnd, (HMENU)IDC_MESSAGE_EDIT, hInst, NULL
            );

            // 서버 연결 버튼 생성
            CreateWindowExA(
                0, "BUTTON", "Connect",
                WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
                230, 220, 100, 30,
                hwnd, (HMENU)IDC_CONNECT_BUTTON, hInst, NULL
            );

            // 메시지 전송 버튼 생성
            CreateWindowExA(
                0, "BUTTON", "Send",
                WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
                380, 260, 100, 30,
                hwnd, (HMENU)IDC_MAIN_BUTTON, hInst, NULL
            );
        }
        break;

        case WM_COMMAND: {
            switch (LOWORD(wParam)) {
                case IDC_CONNECT_BUTTON: {
                    connectToServer(hwnd);
                    HANDLE hThread = CreateThread(NULL, 0, receive_message, NULL, 0, NULL);
                    CloseHandle(hThread);
                }
                break;

                case IDC_MAIN_BUTTON: {
                    char message[BUF_SIZE];
                    GetWindowTextA(hMessageEdit, message, BUF_SIZE);
                    char formatted_message[BUF_SIZE + 64];
                    sprintf(formatted_message, "[%s]: %s", client_name, message);
                    send(server_socket, formatted_message, strlen(formatted_message), 0);
                    SetWindowTextA(hMessageEdit, "");

                    // 새로운 메시지를 추가합니다.
                    char display_message[BUF_SIZE + 64];
                    sprintf(display_message, "[%s]: %s\r\n", client_name, message);

                    SendMessageA(hEdit, EM_SETSEL, (WPARAM)-1, (LPARAM)-1); // 커서를 텍스트 끝으로 이동
                    SendMessageA(hEdit, EM_REPLACESEL, FALSE, (LPARAM)display_message);
                }
                break;
            }
        }
        break;

        case WM_DESTROY: {
            closesocket(server_socket);
            PostQuitMessage(0);
        }
        break;

        default:
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
}