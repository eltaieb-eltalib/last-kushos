#define UNICODE
#define _UNICODE
#define OEMRESOURCE
#include <windows.h>
static const LPCWSTR Names[]={L"Arrow",L"Text",L"Busy (static)",L"Precision",L"Alternate",L"NW/SE resize",L"NE/SW resize",L"Horizontal",L"Vertical",L"Move",L"Unavailable",L"Working (static)",L"Help",L"Link"};
static const WORD Ids[]={OCR_NORMAL,OCR_IBEAM,OCR_WAIT,OCR_CROSS,OCR_UP,OCR_SIZENWSE,OCR_SIZENESW,OCR_SIZEWE,OCR_SIZENS,OCR_SIZEALL,OCR_NO,OCR_APPSTARTING,32651,OCR_HAND};
static HCURSOR Cursors[14];
static int Frame;
static BOOL Classic;
static LRESULT CALLBACK Proc(HWND w,UINT m,WPARAM a,LPARAM b){
 if(m==WM_CREATE){for(int i=0;i<14;i++)Cursors[i]=LoadCursorW(NULL,MAKEINTRESOURCEW(Ids[i]));SetTimer(w,1,75,NULL);return 0;}
 if(m==WM_TIMER){Frame++;InvalidateRect(w,NULL,FALSE);return 0;}
 if(m==WM_SETCURSOR){POINT p;GetCursorPos(&p);ScreenToClient(w,&p);int col=p.x/140,row=(p.y-44)/105;int i=row*4+col;if(p.y>=44&&col>=0&&col<4&&row>=0&&i<14){SetCursor(Cursors[i]);return TRUE;}}
 if(m==WM_PAINT){PAINTSTRUCT ps;HDC dc=BeginPaint(w,&ps);RECT area;GetClientRect(w,&area);HBRUSH bg=CreateSolidBrush(RGB(247,249,252));FillRect(dc,&area,bg);DeleteObject(bg);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(17,70,72));LPCWSTR title=Classic?L"ClassicCursors=1: original resources":L"Kush system cursors: exact upstream assets";TextOutW(dc,16,14,title,lstrlenW(title));for(int i=0;i<14;i++){int x=(i%4)*140,y=44+(i/4)*105;RECT r={x+6,y+4,x+134,y+99};HBRUSH card=CreateSolidBrush(RGB(230,235,242));FillRect(dc,&r,card);DeleteObject(card);int step=i==2?Frame%36:i==11?Frame%37:0;DrawIconEx(dc,x+51,y+12,Cursors[i],32,32,step,NULL,DI_NORMAL);SetTextColor(dc,Cursors[i]?RGB(20,40,50):RGB(180,0,0));TextOutW(dc,x+14,y+64,Names[i],lstrlenW(Names[i]));}EndPaint(w,&ps);return 0;}
 if(m==WM_DESTROY){PostQuitMessage(0);return 0;}return DefWindowProcW(w,m,a,b);}
int WINAPI WinMain(HINSTANCE h,HINSTANCE prev,LPSTR args,int show){if(lstrcmpiA(args,"classic")==0||lstrcmpiA(args,"modern")==0){HKEY k;DWORD v=lstrcmpiA(args,"classic")==0;if(RegCreateKeyExW(HKEY_CURRENT_USER,L"Software\\Kush\\Shell",0,NULL,0,KEY_SET_VALUE,NULL,&k,NULL)==ERROR_SUCCESS){RegSetValueExW(k,L"ClassicCursors",0,REG_DWORD,(BYTE*)&v,sizeof(v));RegCloseKey(k);}Classic=v;}WNDCLASSW wc={0};wc.lpfnWndProc=Proc;wc.hInstance=h;wc.hCursor=LoadCursorW(NULL,IDC_ARROW);wc.lpszClassName=L"KushCursorProbe";RegisterClassW(&wc);CreateWindowW(wc.lpszClassName,L"Kush cursor test, real USER32 resources",WS_OVERLAPPEDWINDOW|WS_VISIBLE,100,45,584,510,NULL,NULL,h,NULL);MSG msg;while(GetMessageW(&msg,NULL,0,0)>0){TranslateMessage(&msg);DispatchMessageW(&msg);}return 0;}
