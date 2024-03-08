#include <windows.h>
#include "config.h"

HINSTANCE _hInstance;
char *_lpWndMsg;
HWND _hParent;
HWND _hEdit, _hBtnOk, _hBtnCancel;
HWND _hMsgText;
RECT _st_rcDesktop;
HFONT _hWndFont;
char _szBuffer[256];
UINT _nMaxLine = 5;
UINT _nEditStyle = WS_BORDER | WS_CHILD | WS_VISIBLE |
				ES_AUTOHSCROLL | ES_AUTOVSCROLL | ES_NUMBER;

int WINAPI WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	HDC hWndDc;
	WORD uBtnID;
	switch(uMsg) {
		case WM_DESTROY:
			if (_hWndFont) ::DeleteObject(_hWndFont);
			::PostQuitMessage(0);
			break;
		case WM_CREATE:
			_hMsgText = ::CreateWindowExA(0,
										"Static",
										_lpWndMsg,
										WS_CHILD | WS_VISIBLE,
										5,
										5,
										400,
										100,
										hWnd,
										(HMENU)1000,
										_hInstance,
										0);
			_hBtnOk = ::CreateWindowExA(0,
										"Button",
										"OK(&K)",
										WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
										400,
										5,
										150,
										50,
										hWnd,
										(HMENU)IDOK,
										_hInstance,
										0);
			_hBtnCancel = ::CreateWindowExA(0,
										"Button",
										"Cancel(&C)",
										WS_CHILD | WS_VISIBLE,
										400,
										55,
										150,
										50,
										hWnd,
										(HMENU)IDCANCEL,
										_hInstance,
										0);
			_hEdit = ::CreateWindowExA(WS_EX_CLIENTEDGE,
										"Edit",
										"",
										_nEditStyle,
										5,
										60,
										300,
										30,
										hWnd,
										(HMENU)2000,
										_hInstance,
										0);
			::SendMessage(_hEdit, EM_SETLIMITTEXT, _nMaxLine, 0);
			_hWndFont =		::CreateFont(20,
										 12,
										 0,
										 0,
										 12,
										 0,
										 0,
										 0,
										 DEFAULT_CHARSET,
										 OUT_DEFAULT_PRECIS,
										 CLIP_DEFAULT_PRECIS,
										 DEFAULT_QUALITY,
										 DEFAULT_PITCH,
										 NULL);
			hWndDc = ::GetDC(hWnd);
			::SelectObject(hWndDc, _hWndFont);
			::ReleaseDC(hWnd,hWndDc);
			::SendDlgItemMessage(hWnd, 1000, WM_SETFONT, (WPARAM)_hWndFont, 0);
			::SendDlgItemMessage(hWnd, 2000, WM_SETFONT, (WPARAM)_hWndFont, 0);
			::SendDlgItemMessage(hWnd, IDOK, WM_SETFONT, (WPARAM)_hWndFont, 0);
			::SendDlgItemMessage(hWnd, IDCANCEL, WM_SETFONT, (WPARAM)_hWndFont, 0);
			break;
		case WM_KEYDOWN:
			if (wParam == VK_RETURN) ::SendMessage(hWnd, WM_COMMAND, IDOK, 0);
			break;
		case WM_SETFOCUS:
			::SetFocus(_hEdit);
			break;
		case WM_COMMAND:
			uBtnID = LOWORD(wParam);
			switch(uBtnID) {
				case IDOK:
					::GetDlgItemTextA(hWnd, 2000, _szBuffer, 256);
					::DestroyWindow(hWnd);
					break;
				case IDCANCEL:
					::DestroyWindow(hWnd);
					break;
				};
			break;
		default:
			return ::DefWindowProc(hWnd, uMsg, wParam, lParam);
		}
	return(TRUE);
}

HWND _CreateWindow(HINSTANCE hInst) {
	WNDCLASSEXA st_WndClass;
	HWND hWnd;
	::RtlZeroMemory(&st_WndClass, sizeof(st_WndClass));
	st_WndClass.cbSize			= sizeof(st_WndClass);
	st_WndClass.hInstance		= hInst;
	st_WndClass.hbrBackground	= (HBRUSH)COLOR_BTNSHADOW;
	st_WndClass.hCursor			= LoadCursor(0, IDC_ARROW);
	st_WndClass.hIcon			= LoadIcon(0, IDI_APPLICATION);
	st_WndClass.hIconSm			= st_WndClass.hIcon;
	st_WndClass.lpfnWndProc		= (WNDPROC)&WndProc;
	st_WndClass.lpszClassName	= "InputBox_Class";
	st_WndClass.style			= CS_HREDRAW | CS_VREDRAW;
	::RegisterClassExA(&st_WndClass);
	hWnd = ::CreateWindowExA(0,
							"InputBox_Class",
							" ",
							WS_DLGFRAME | WS_SYSMENU | WS_VISIBLE,
							CW_USEDEFAULT,
							CW_USEDEFAULT,
							600,
							200,
							_hParent,
							0,
							hInst,
							0);
	return hWnd;
}

int _Run(HWND hWnd) {
	MSG st_Msg;
	if (!hWnd) return 0;
	::ShowWindow(hWnd, SW_SHOW);
	::UpdateWindow(hWnd);
	while (::GetMessage(&st_Msg, 0, 0, 0)) {
		if (st_Msg.message == WM_KEYDOWN && st_Msg.wParam == VK_RETURN) {
			::SendMessage(hWnd, st_Msg.message, st_Msg.wParam, st_Msg.wParam);
		}
		::TranslateMessage(&st_Msg);
		::DispatchMessage(&st_Msg);
	}
	return st_Msg.wParam;
}

char *_InputBox(char *lpWndMsg) {
	_lpWndMsg = lpWndMsg;
	_Run(_CreateWindow(_hInstance));
	return _szBuffer;
}
