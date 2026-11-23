/*
 * Kush 1.1: named pipe stress test that mimics Chromium/Firefox IPC on Windows
 * (ipc/chromium/src/chrome/common/ipc_channel_win.cc):
 *   - one byte-mode duplex pipe, FILE_FLAG_OVERLAPPED on both ends,
 *     4096-byte pipe buffers;
 *   - each end has one I/O completion port; reads are always outstanding
 *     (overlapped ReadFile into a 4096-byte buffer), writes send a whole
 *     "message" at once (often much larger than the pipe buffer), one at a time;
 *   - both directions run at the same time (full duplex).
 * Every byte of each direction's stream is a known function of its offset,
 * so the reader detects lost, duplicated, reordered or corrupted data and
 * prints the first bad offset.
 *
 * Build: x86_64-w64-mingw32-gcc -O2 -o pipetest.exe pipetest.c
 * Run:   pipetest.exe [MB per direction, default 64] [maxmsg KB, default 256] [proc]
 *        "proc" runs the client end in a second process (like Firefox parent/content).
 */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define READ_SIZE 4096
#define PT_KEY_READ  1
#define PT_KEY_WRITE 2

static unsigned char pattern(unsigned long long off, int dir)
{
    unsigned long long x = off * 2654435761ULL + (unsigned)dir * 0x9E3779B9u;
    return (unsigned char)(x ^ (x >> 13) ^ (x >> 29));
}

typedef struct
{
    HANDLE Pipe, Port;
    int Dir;                       /* direction this end WRITES */
    unsigned long long Total;      /* bytes to send/receive */
    unsigned MaxMsg;
    volatile LONG Failed;
    unsigned long long Received, Sent;
    unsigned long long FirstBad;
    unsigned char BadGot, BadWant;
} END;

static DWORD WINAPI EndThread(LPVOID p)
{
    END *e = p;
    OVERLAPPED rov = {0}, wov = {0};
    unsigned char *rbuf = malloc(READ_SIZE);
    unsigned char *wbuf = malloc(e->MaxMsg);
    unsigned seed = 12345 + e->Dir;
    BOOL writing = FALSE, readDone = FALSE, writeDone = FALSE;
    DWORD n;

    /* start the first read */
    if (!ReadFile(e->Pipe, rbuf, READ_SIZE, NULL, &rov) && GetLastError() != ERROR_IO_PENDING)
    {
        printf("dir %d: ReadFile failed %lu\n", e->Dir, GetLastError());
        e->Failed = 1;
        return 1;
    }

    while (!(readDone && writeDone) && !e->Failed)
    {
        ULONG_PTR key;
        LPOVERLAPPED ov;

        /* queue the next message write */
        if (!writing && !writeDone)
        {
            unsigned len;
            unsigned long long i;
            seed = seed * 1103515245 + 12345;
            len = 1 + (seed >> 8) % e->MaxMsg;
            if (len > e->Total - e->Sent) len = (unsigned)(e->Total - e->Sent);
            for (i = 0; i < len; i++) wbuf[i] = pattern(e->Sent + i, e->Dir);
            if (!WriteFile(e->Pipe, wbuf, len, NULL, &wov) && GetLastError() != ERROR_IO_PENDING)
            {
                printf("dir %d: WriteFile failed %lu\n", e->Dir, GetLastError());
                e->Failed = 1;
                break;
            }
            writing = TRUE;
        }

        if (!GetQueuedCompletionStatus(e->Port, &n, &key, &ov, 60000))
        {
            if (!ov)
            {
                printf("dir %d: TIMEOUT (60 s) sent %llu received %llu - hang\n", e->Dir, e->Sent, e->Received);
                e->Failed = 2;
                break;
            }
            printf("dir %d: I/O failed %lu (%s)\n", e->Dir, GetLastError(), ov == &rov ? "read" : "write");
            e->Failed = 1;
            break;
        }

        if (ov == &wov)
        {
            e->Sent += n;
            writing = FALSE;
            if (e->Sent >= e->Total) writeDone = TRUE;
        }
        else if (ov == &rov)
        {
            DWORD i;
            for (i = 0; i < n; i++)
            {
                unsigned char want = pattern(e->Received + i, !e->Dir);
                if (rbuf[i] != want)
                {
                    e->FirstBad = e->Received + i;
                    e->BadGot = rbuf[i];
                    e->BadWant = want;
                    e->Failed = 3;
                    break;
                }
            }
            if (e->Failed) break;
            e->Received += n;
            if (e->Received >= e->Total)
                readDone = TRUE;
            else if (!ReadFile(e->Pipe, rbuf, READ_SIZE, NULL, &rov) && GetLastError() != ERROR_IO_PENDING)
            {
                printf("dir %d: ReadFile failed %lu\n", e->Dir, GetLastError());
                e->Failed = 1;
            }
        }
    }
    return 0;
}

static int RunOneEnd(END *e)
{
    HANDLE th = CreateThread(NULL, 0, EndThread, e, 0, NULL);
    WaitForSingleObject(th, INFINITE);
    if (!e->Failed)
    {
        printf("[%s] PASS: %llu bytes received, %llu sent\n", e->Dir ? "child" : "parent", e->Received, e->Sent);
        return 0;
    }
    if (e->Failed == 3)
        printf("[%s] FAIL: data corrupted at stream offset %llu: got 0x%02x want 0x%02x\n",
               e->Dir ? "child" : "parent", e->FirstBad, e->BadGot, e->BadWant);
    printf("[%s] FAIL code %ld sent %llu received %llu\n", e->Dir ? "child" : "parent", e->Failed, e->Sent, e->Received);
    return 1;
}

int main(int argc, char **argv)
{
    unsigned long long mb = argc > 1 ? strtoull(argv[1], NULL, 10) : 64;
    unsigned maxkb = argc > 2 ? (unsigned)strtoul(argv[2], NULL, 10) : 256;
    int mode = argc > 3 ? (!strcmp(argv[3], "child") ? 2 : !strcmp(argv[3], "proc") ? 1 : 0) : 0;
    const char *name = "\\\\.\\pipe\\kush.pipetest";
    END a = {0}, b = {0};
    HANDLE th[2];
    DWORD t0;
    OVERLAPPED cov = {0};

    a.Total = b.Total = mb << 20;
    a.MaxMsg = b.MaxMsg = maxkb << 10;
    a.Dir = 0; b.Dir = 1;

    if (mode == 2) /* child: client end in its own process */
    {
        b.Pipe = CreateFileA(name, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING,
                             FILE_FLAG_OVERLAPPED | SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION, NULL);
        if (b.Pipe == INVALID_HANDLE_VALUE) { printf("[child] CreateFile failed %lu\n", GetLastError()); return 1; }
        b.Port = CreateIoCompletionPort(b.Pipe, NULL, PT_KEY_READ, 1);
        return RunOneEnd(&b);
    }

    a.Pipe = CreateNamedPipeA(name, PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED | FILE_FLAG_FIRST_PIPE_INSTANCE,
                              PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, 1, 4096, 4096, 5000, NULL);
    if (a.Pipe == INVALID_HANDLE_VALUE) { printf("CreateNamedPipe failed %lu\n", GetLastError()); return 1; }
    cov.hEvent = CreateEventA(NULL, TRUE, FALSE, NULL);
    ConnectNamedPipe(a.Pipe, &cov);

    printf("pipetest: %llu MB each way, messages up to %u KB, 4 KB reads, full duplex, IOCP, %s\n",
           mb, maxkb, mode == 1 ? "TWO PROCESSES" : "one process");
    t0 = GetTickCount();

    if (mode == 1) /* parent: server end here, client end in a child process */
    {
        char cmd[MAX_PATH + 64], exe[MAX_PATH];
        STARTUPINFOA si = { sizeof(si) };
        PROCESS_INFORMATION pi;
        DWORD code = 1;
        int r;
        GetModuleFileNameA(NULL, exe, MAX_PATH);
        sprintf(cmd, "\"%s\" %llu %u child", exe, mb, maxkb);
        if (!CreateProcessA(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
        { printf("CreateProcess failed %lu\n", GetLastError()); return 1; }
        WaitForSingleObject(cov.hEvent, 10000);
        a.Port = CreateIoCompletionPort(a.Pipe, NULL, PT_KEY_READ, 1);
        r = RunOneEnd(&a);
        WaitForSingleObject(pi.hProcess, INFINITE);
        GetExitCodeProcess(pi.hProcess, &code);
        printf("%s (%lu ms)\n", (!r && !code) ? "PASS: both processes verified their streams" : "FAIL", GetTickCount() - t0);
        return r || code;
    }

    b.Pipe = CreateFileA(name, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING,
                         FILE_FLAG_OVERLAPPED | SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION, NULL);
    if (b.Pipe == INVALID_HANDLE_VALUE) { printf("CreateFile failed %lu\n", GetLastError()); return 1; }
    WaitForSingleObject(cov.hEvent, 5000);
    a.Port = CreateIoCompletionPort(a.Pipe, NULL, PT_KEY_READ, 1);
    b.Port = CreateIoCompletionPort(b.Pipe, NULL, PT_KEY_READ, 1);

    th[0] = CreateThread(NULL, 0, EndThread, &a, 0, NULL);
    th[1] = CreateThread(NULL, 0, EndThread, &b, 0, NULL);
    WaitForMultipleObjects(2, th, TRUE, INFINITE);

    if (!a.Failed && !b.Failed)
    {
        printf("PASS: %llu bytes each way verified in %lu ms\n", a.Received, GetTickCount() - t0);
        return 0;
    }
    if (a.Failed == 3 || b.Failed == 3)
    {
        END *e = a.Failed == 3 ? &a : &b;
        printf("FAIL: data corrupted, reader of dir %d at stream offset %llu: got 0x%02x want 0x%02x\n",
               !e->Dir, e->FirstBad, e->BadGot, e->BadWant);
    }
    printf("FAIL: a.Failed=%ld b.Failed=%ld sent %llu/%llu received %llu/%llu\n",
           a.Failed, b.Failed, a.Sent, b.Sent, a.Received, b.Received);
    return 1;
}
