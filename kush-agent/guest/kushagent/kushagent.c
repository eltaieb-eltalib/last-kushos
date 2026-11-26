/* kushagent.exe - in-guest agent for kush-agent.
 *
 * Runs inside Kush OS / ReactOS / Windows, in the logged-in user's session (so SendInput and
 * EnumWindows reach the visible desktop). Reads one JSON request per line from a serial port
 * and answers with one JSON line. See docs/guest-agent-protocol.md.
 *
 * Usage:  kushagent.exe [port]        default port: \\.\COM2
 *         kushagent.exe \\.\Global\org.kush.agent.0   (virtio-serial)
 */
#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0501
#include <windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "protocol.h"

#define KUSHAGENT_VERSION "0.1.0"
#define MAX_LINE (8 * 1024 * 1024)
#define MAX_OUTPUT (256 * 1024)
#define MAX_FILE (16 * 1024 * 1024)

static HANDLE g_port = INVALID_HANDLE_VALUE;

/* ---------- UTF-8 <-> UTF-16 ------------------------------------------------------------- */
static WCHAR *to_wide(const char *utf8)
{
    int n = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0);
    WCHAR *w = (WCHAR *)malloc(sizeof(WCHAR) * (n > 0 ? n : 1));
    if (!w) return NULL;
    if (n <= 0) { w[0] = 0; return w; }
    MultiByteToWideChar(CP_UTF8, 0, utf8, -1, w, n);
    return w;
}

static char *to_utf8_n(const WCHAR *w, int wlen)
{
    int n = WideCharToMultiByte(CP_UTF8, 0, w, wlen, NULL, 0, NULL, NULL);
    char *s = (char *)malloc(n + 1);
    if (!s) return NULL;
    WideCharToMultiByte(CP_UTF8, 0, w, wlen, s, n, NULL, NULL);
    s[n] = 0;
    return s;
}

static char *oem_to_utf8(const char *oem, int len)
{
    int wn = MultiByteToWideChar(CP_OEMCP, 0, oem, len, NULL, 0);
    WCHAR *w = (WCHAR *)malloc(sizeof(WCHAR) * (wn + 1));
    char *s;
    if (!w) return NULL;
    MultiByteToWideChar(CP_OEMCP, 0, oem, len, w, wn);
    s = to_utf8_n(w, wn);
    free(w);
    return s;
}

/* ---------- port I/O ---------------------------------------------------------------------- */
static int open_port(const char *name)
{
    DCB dcb;
    COMMTIMEOUTS to;
    g_port = CreateFileA(name, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (g_port == INVALID_HANDLE_VALUE) return 0;
    memset(&dcb, 0, sizeof(dcb));
    dcb.DCBlength = sizeof(dcb);
    if (GetCommState(g_port, &dcb)) { /* a COM port: 115200 8N1; virtio ports skip this */
        dcb.BaudRate = CBR_115200;
        dcb.ByteSize = 8;
        dcb.Parity = NOPARITY;
        dcb.StopBits = ONESTOPBIT;
        dcb.fBinary = TRUE;
        SetCommState(g_port, &dcb);
        memset(&to, 0, sizeof(to));
        to.ReadIntervalTimeout = 50;
        to.ReadTotalTimeoutConstant = 1000;
        to.WriteTotalTimeoutConstant = 5000;
        SetCommTimeouts(g_port, &to);
    }
    return 1;
}

static void send_line(const char *s, size_t n)
{
    DWORD w;
    size_t off = 0;
    while (off < n) {
        if (!WriteFile(g_port, s + off, (DWORD)(n - off), &w, NULL) || w == 0) return;
        off += w;
    }
    WriteFile(g_port, "\n", 1, &w, NULL);
}

static void reply_ok(long id, const char *result_json)
{
    sbuf b;
    sb_init(&b);
    sb_append(&b, "{\"id\":");
    sb_appendf_long(&b, id);
    sb_append(&b, ",\"result\":");
    sb_append(&b, result_json);
    sb_append(&b, "}");
    send_line(b.data, b.len);
    sb_free(&b);
}

static void reply_err(long id, const char *msg)
{
    sbuf b;
    sb_init(&b);
    sb_append(&b, "{\"id\":");
    sb_appendf_long(&b, id);
    sb_append(&b, ",\"error\":");
    sb_append_json_string(&b, msg);
    sb_append(&b, "}");
    send_line(b.data, b.len);
    sb_free(&b);
}

/* ---------- methods ----------------------------------------------------------------------- */
static void m_ping(long id)
{
    char name[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD n = sizeof(name);
    OSVERSIONINFOA vi;
    sbuf b;
    char ver[64];
    if (!GetComputerNameA(name, &n)) strcpy(name, "?");
    memset(&vi, 0, sizeof(vi));
    vi.dwOSVersionInfoSize = sizeof(vi);
    GetVersionExA(&vi);
    sprintf(ver, "%lu.%lu.%lu", vi.dwMajorVersion, vi.dwMinorVersion, vi.dwBuildNumber);
    sb_init(&b);
    sb_append(&b, "{\"ok\":true,\"agent\":\"" KUSHAGENT_VERSION "\",\"computer\":");
    sb_append_json_string(&b, name);
    sb_append(&b, ",\"os_version\":");
    sb_append_json_string(&b, ver);
    sb_append(&b, "}");
    reply_ok(id, b.data);
    sb_free(&b);
}

static void m_run(long id, const char *req)
{
    char *cmd = json_get_string(req, "cmd");
    long timeout_ms = 30000;
    SECURITY_ATTRIBUTES sa;
    HANDLE rd = NULL, wr = NULL;
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    WCHAR *wcmd;
    sbuf out, res;
    DWORD start, code = (DWORD)-1, avail, got;
    char buf[4096];
    int timed_out = 0;
    char *utf;

    if (!cmd) { reply_err(id, "missing params.cmd"); return; }
    json_get_long(req, "timeout_ms", &timeout_ms);
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;
    if (!CreatePipe(&rd, &wr, &sa, 0)) { free(cmd); reply_err(id, "CreatePipe failed"); return; }
    SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    si.hStdOutput = wr;
    si.hStdError = wr;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    wcmd = to_wide(cmd);
    free(cmd);
    if (!wcmd || !CreateProcessW(NULL, wcmd, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        char msg[96];
        sprintf(msg, "CreateProcess failed (error %lu)", GetLastError());
        free(wcmd);
        CloseHandle(rd);
        CloseHandle(wr);
        reply_err(id, msg);
        return;
    }
    free(wcmd);
    CloseHandle(wr);
    sb_init(&out);
    sb_append(&out, "");
    start = GetTickCount();
    for (;;) {
        while (PeekNamedPipe(rd, NULL, 0, NULL, &avail, NULL) && avail > 0) {
            if (!ReadFile(rd, buf, sizeof(buf), &got, NULL) || got == 0) break;
            if (out.len < MAX_OUTPUT) sb_appendn(&out, buf, got);
        }
        if (WaitForSingleObject(pi.hProcess, 50) == WAIT_OBJECT_0) {
            while (ReadFile(rd, buf, sizeof(buf), &got, NULL) && got > 0)
                if (out.len < MAX_OUTPUT) sb_appendn(&out, buf, got);
            break;
        }
        if ((long)(GetTickCount() - start) > timeout_ms) {
            TerminateProcess(pi.hProcess, 1);
            timed_out = 1;
            break;
        }
    }
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(rd);
    utf = oem_to_utf8(out.data, (int)out.len);
    sb_init(&res);
    sb_append(&res, "{\"exit_code\":");
    sb_appendf_long(&res, (long)code);
    sb_append(&res, timed_out ? ",\"timed_out\":true" : ",\"timed_out\":false");
    sb_append(&res, ",\"output\":");
    sb_append_json_string(&res, utf ? utf : "");
    sb_append(&res, "}");
    reply_ok(id, res.data);
    free(utf);
    sb_free(&out);
    sb_free(&res);
}

static void m_type_unicode(long id, const char *req)
{
    char *text = json_get_string(req, "text");
    WCHAR *w;
    size_t i, n;
    INPUT in[2];
    if (!text) { reply_err(id, "missing params.text"); return; }
    w = to_wide(text);
    free(text);
    if (!w) { reply_err(id, "out of memory"); return; }
    n = wcslen(w);
    for (i = 0; i < n; i++) {
        memset(in, 0, sizeof(in));
        if (w[i] == L'\n') { /* Enter as a real key so dialogs and editors react */
            in[0].type = INPUT_KEYBOARD; in[0].ki.wVk = VK_RETURN;
            in[1] = in[0]; in[1].ki.dwFlags = KEYEVENTF_KEYUP;
        } else {
            in[0].type = INPUT_KEYBOARD; in[0].ki.wScan = w[i]; in[0].ki.dwFlags = KEYEVENTF_UNICODE;
            in[1] = in[0]; in[1].ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
        }
        SendInput(2, in, sizeof(INPUT));
        Sleep(5);
    }
    free(w);
    reply_ok(id, "{\"ok\":true}");
}

static BOOL CALLBACK enum_cb(HWND h, LPARAM lp)
{
    sbuf *b = (sbuf *)lp;
    WCHAR title[512], cls[256];
    char *t, *c;
    RECT r;
    if (!IsWindowVisible(h)) return TRUE;
    if (GetWindowTextW(h, title, 512) == 0) return TRUE;
    GetClassNameW(h, cls, 256);
    GetWindowRect(h, &r);
    t = to_utf8_n(title, -1);
    c = to_utf8_n(cls, -1);
    if (b->len > 1) sb_append(b, ",");
    sb_append(b, "{\"hwnd\":");
    sb_appendf_long(b, (long)(LONG_PTR)h);
    sb_append(b, ",\"title\":");
    sb_append_json_string(b, t ? t : "");
    sb_append(b, ",\"class\":");
    sb_append_json_string(b, c ? c : "");
    sb_append(b, ",\"rect\":[");
    sb_appendf_long(b, r.left); sb_append(b, ",");
    sb_appendf_long(b, r.top); sb_append(b, ",");
    sb_appendf_long(b, r.right); sb_append(b, ",");
    sb_appendf_long(b, r.bottom);
    sb_append(b, "],\"foreground\":");
    sb_append(b, h == GetForegroundWindow() ? "true}" : "false}");
    free(t);
    free(c);
    return TRUE;
}

static void m_list_windows(long id)
{
    sbuf b;
    sb_init(&b);
    sb_append(&b, "[");
    EnumWindows(enum_cb, (LPARAM)&b);
    sb_append(&b, "]");
    reply_ok(id, b.data);
    sb_free(&b);
}

static void m_read_file(long id, const char *req)
{
    char *path = json_get_string(req, "path");
    WCHAR *wp;
    HANDLE f;
    DWORD size, got;
    unsigned char *data;
    char *enc;
    sbuf b;
    if (!path) { reply_err(id, "missing params.path"); return; }
    wp = to_wide(path);
    free(path);
    f = CreateFileW(wp, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    free(wp);
    if (f == INVALID_HANDLE_VALUE) { reply_err(id, "cannot open file"); return; }
    size = GetFileSize(f, NULL);
    if (size > MAX_FILE) { CloseHandle(f); reply_err(id, "file too large"); return; }
    data = (unsigned char *)malloc(size + 1);
    if (!data || !ReadFile(f, data, size, &got, NULL)) { free(data); CloseHandle(f); reply_err(id, "read failed"); return; }
    CloseHandle(f);
    enc = b64_encode(data, got);
    free(data);
    sb_init(&b);
    sb_append(&b, "{\"size\":");
    sb_appendf_long(&b, (long)got);
    sb_append(&b, ",\"data\":\"");
    sb_append(&b, enc ? enc : "");
    sb_append(&b, "\"}");
    reply_ok(id, b.data);
    free(enc);
    sb_free(&b);
}

static void m_write_file(long id, const char *req)
{
    char *path = json_get_string(req, "path");
    char *enc = json_get_string(req, "data");
    WCHAR *wp;
    HANDLE f;
    unsigned char *data;
    size_t n;
    DWORD w;
    if (!path || !enc) { free(path); free(enc); reply_err(id, "missing params.path or params.data"); return; }
    data = b64_decode(enc, &n);
    free(enc);
    wp = to_wide(path);
    free(path);
    f = CreateFileW(wp, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    free(wp);
    if (f == INVALID_HANDLE_VALUE) { free(data); reply_err(id, "cannot create file"); return; }
    if (!WriteFile(f, data, (DWORD)n, &w, NULL) || w != n) { CloseHandle(f); free(data); reply_err(id, "write failed"); return; }
    CloseHandle(f);
    free(data);
    reply_ok(id, "{\"ok\":true}");
}

static void handle(const char *line)
{
    long id = 0;
    char *method;
    if (!json_get_long(line, "id", &id)) return;
    method = json_get_string(line, "method");
    if (!method) { reply_err(id, "missing method"); return; }
    if (!strcmp(method, "ping")) m_ping(id);
    else if (!strcmp(method, "run")) m_run(id, line);
    else if (!strcmp(method, "type_unicode")) m_type_unicode(id, line);
    else if (!strcmp(method, "list_windows")) m_list_windows(id);
    else if (!strcmp(method, "read_file")) m_read_file(id, line);
    else if (!strcmp(method, "write_file")) m_write_file(id, line);
    else reply_err(id, "unknown method");
    free(method);
}

int main(int argc, char **argv)
{
    const char *port = argc > 1 ? argv[1] : "\\\\.\\COM2";
    sbuf line;
    char buf[4096];
    DWORD got, i;

    while (!open_port(port)) Sleep(2000); /* port may appear after boot */
    sb_init(&line);
    for (;;) {
        if (!ReadFile(g_port, buf, sizeof(buf), &got, NULL)) {
            Sleep(500);
            continue;
        }
        for (i = 0; i < got; i++) {
            if (buf[i] == '\n') {
                if (line.len) handle(line.data);
                line.len = 0;
                if (line.data) line.data[0] = 0;
            } else if (buf[i] != '\r' && line.len < MAX_LINE) {
                sb_appendn(&line, &buf[i], 1);
            }
        }
    }
    return 0;
}
