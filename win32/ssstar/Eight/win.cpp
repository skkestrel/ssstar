#include <windows.h>
#include  <scrnsave.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include "../../../common/eight.h"
#include "resource.h"
#pragma warning(disable: 4305 4244) 

#pragma comment(lib, "ScrnSavw.lib")

#define TIMER 1

void InitGL(HWND hWnd, HDC & hDC, HGLRC & hRC);
void CloseGL(HWND hWnd, HDC hDC, HGLRC hRC);
void SetupAnimation(int Width, int Height);
void OnTimer(HDC hDC);

int Width, Height; //globals for size of screen

EIGHT_State state;

LRESULT WINAPI ScreenSaverProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
	static HDC hDC;
	static HGLRC hRC;
	static RECT rect;

	switch (message) {

	case WM_CREATE:
		// get window dimensions
		GetClientRect(hWnd, &rect);
		Width = rect.right;
		Height = rect.bottom;
		state.width = Width;
		state.height = Height;

		InitGL(hWnd, hDC, hRC);
		state.hdc = hDC;
		EIGHT_Init(&state);

		SetTimer(hWnd, TIMER, 10, NULL);
		return 0;

	case WM_DESTROY:
		KillTimer(hWnd, TIMER);
		CloseGL(hWnd, hDC, hRC);
		return 0;
	case WM_TIMER:
		EIGHT_Draw(&state);
		SwapBuffers(hDC);
		return 0;
	}

	return DefScreenSaverProc(
		hWnd, message, wParam, lParam);
}

BOOL WINAPI ScreenSaverConfigureDialog(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	return FALSE;
}

BOOL WINAPI RegisterDialogClasses(HANDLE hInst)
{
	return TRUE;
}


static void InitGL(HWND hWnd, HDC & hDC, HGLRC & hRC) {
	PIXELFORMATDESCRIPTOR pfd;
	ZeroMemory(&pfd, sizeof pfd);
	pfd.nSize = sizeof pfd;
	pfd.nVersion = 1;
	//pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL; //blaine's
	pfd.dwFlags = PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
	pfd.iPixelType = PFD_TYPE_RGBA;
	pfd.cColorBits = 24;

	hDC = GetDC(hWnd);

	int i = ChoosePixelFormat(hDC, &pfd);
	SetPixelFormat(hDC, i, &pfd);

	hRC = wglCreateContext(hDC);
	wglMakeCurrent(hDC, hRC);

}

static void CloseGL(HWND hWnd, HDC hDC, HGLRC hRC)
{
	wglMakeCurrent(NULL, NULL);
	wglDeleteContext(hRC);

	ReleaseDC(hWnd, hDC);
}