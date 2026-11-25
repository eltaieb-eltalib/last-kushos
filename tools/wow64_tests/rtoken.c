/* rtoken: the steps Chromium's sandbox takes to build a sandboxed child's
 * token (Firefox content processes), each reported with its error code:
 * CreateRestrictedToken (deny-only groups, restricting SIDs, no privileges),
 * removing privileges, the default DACL, Low integrity, an impersonation
 * copy, and starting a suspended process with it. */
#include <windows.h>
#include <aclapi.h>
#include <sddl.h>
#include <stdio.h>

#define STEP(name, expr) do { BOOL ok_ = (expr); \
    printf("%-34s %s (err %lu)\n", name, ok_ ? "ok" : "FAILED", ok_ ? 0 : GetLastError()); } while (0)

static BYTE buf[8192], buf2[8192];

int main(void)
{
    HANDLE tok = NULL, rtok = NULL, dup = NULL, imp = NULL;
    DWORD len, i, nDeny = 0, nRestrict = 0;
    TOKEN_GROUPS *groups = (TOKEN_GROUPS *)buf;
    SID_AND_ATTRIBUTES deny[64], restrict_[8];
    PSID restricted = NULL, everyone = NULL, low = NULL;
    BYTE userbuf[256];
    TOKEN_USER *user = (TOKEN_USER *)userbuf;

    STEP("OpenProcessToken", OpenProcessToken(GetCurrentProcess(), TOKEN_ALL_ACCESS, &tok));
    STEP("GetTokenInformation(Groups)", GetTokenInformation(tok, TokenGroups, buf, sizeof(buf), &len));
    {
        BOOL ok = GetTokenInformation(tok, TokenLogonSid, buf2, sizeof(buf2), &len);
        DWORD e = ok ? 0 : GetLastError();
        printf("%-34s %s (err %lu)\n", "GetTokenInformation(LogonSid)",
               (ok || e == ERROR_NOT_FOUND) ? "ok" : "FAILED", e);
    }
    STEP("GetTokenInformation(User)", GetTokenInformation(tok, TokenUser, userbuf, sizeof(userbuf), &len));
    for (i = 0; i < groups->GroupCount && nDeny < 64; i++)
    {
        if (groups->Groups[i].Attributes & (SE_GROUP_INTEGRITY | SE_GROUP_LOGON_ID)) continue;
        deny[nDeny].Sid = groups->Groups[i].Sid; deny[nDeny].Attributes = 0; nDeny++;
    }
    deny[nDeny].Sid = user->User.Sid; deny[nDeny].Attributes = 0; nDeny++;
    ConvertStringSidToSidA("S-1-5-12", &restricted);
    ConvertStringSidToSidA("S-1-1-0", &everyone);
    ConvertStringSidToSidA("S-1-16-4096", &low);
    restrict_[nRestrict].Sid = restricted; restrict_[nRestrict++].Attributes = 0;
    restrict_[nRestrict].Sid = everyone; restrict_[nRestrict++].Attributes = 0;
    restrict_[nRestrict].Sid = user->User.Sid; restrict_[nRestrict++].Attributes = 0;
    printf("deny-only %lu, restricting %lu\n", nDeny, nRestrict);

    STEP("CreateRestrictedToken(DISABLE_MAX)", CreateRestrictedToken(tok, DISABLE_MAX_PRIVILEGE,
         nDeny, deny, 0, NULL, nRestrict, restrict_, &rtok));
    if (!rtok) return 1;
    STEP("DuplicateTokenEx(primary)", DuplicateTokenEx(rtok, TOKEN_ALL_ACCESS, NULL, SecurityIdentification, TokenPrimary, &dup));
    {
        TOKEN_PRIVILEGES *tp = (TOKEN_PRIVILEGES *)buf2;
        STEP("GetTokenInformation(Privileges)", GetTokenInformation(rtok, TokenPrivileges, buf2, sizeof(buf2), &len));
        printf("privileges left: %lu\n", tp->PrivilegeCount);
        for (i = 0; i < tp->PrivilegeCount; i++) tp->Privileges[i].Attributes = SE_PRIVILEGE_REMOVED;
        STEP("AdjustTokenPrivileges(REMOVED)", AdjustTokenPrivileges(rtok, FALSE, tp, len, NULL, NULL));
    }
    {
        TOKEN_DEFAULT_DACL *dd = (TOKEN_DEFAULT_DACL *)buf2;
        EXPLICIT_ACCESSW ea[2] = {0};
        PACL newacl = NULL;
        DWORD r;
        STEP("GetTokenInformation(DefaultDacl)", GetTokenInformation(rtok, TokenDefaultDacl, buf2, sizeof(buf2), &len));
        ea[0].grfAccessPermissions = GENERIC_ALL; ea[0].grfAccessMode = GRANT_ACCESS;
        ea[0].Trustee.TrusteeForm = TRUSTEE_IS_SID; ea[0].Trustee.ptstrName = (LPWSTR)restricted;
        ea[1].grfAccessPermissions = GENERIC_ALL; ea[1].grfAccessMode = GRANT_ACCESS;
        ea[1].Trustee.TrusteeForm = TRUSTEE_IS_SID; ea[1].Trustee.ptstrName = (LPWSTR)user->User.Sid;
        r = SetEntriesInAclW(2, ea, dd->DefaultDacl, &newacl);
        printf("%-34s %s (err %lu)\n", "SetEntriesInAcl", r == ERROR_SUCCESS ? "ok" : "FAILED", r);
        if (newacl)
        {
            TOKEN_DEFAULT_DACL nd; nd.DefaultDacl = newacl;
            STEP("SetTokenInformation(DefaultDacl)", SetTokenInformation(rtok, TokenDefaultDacl, &nd, sizeof(nd)));
        }
    }
    {
        TOKEN_MANDATORY_LABEL tml = {0};
        tml.Label.Attributes = SE_GROUP_INTEGRITY; tml.Label.Sid = low;
        STEP("SetTokenInformation(Integrity Low)", SetTokenInformation(rtok, TokenIntegrityLevel, &tml, sizeof(tml)));
        STEP("GetTokenInformation(Integrity)", GetTokenInformation(rtok, TokenIntegrityLevel, buf2, sizeof(buf2), &len));
        if (len) printf("integrity RID: 0x%lx\n", *GetSidSubAuthority(((TOKEN_MANDATORY_LABEL *)buf2)->Label.Sid, 0));
    }
    STEP("DuplicateTokenEx(impersonation)", DuplicateTokenEx(rtok, TOKEN_ALL_ACCESS, NULL, SecurityImpersonation, TokenImpersonation, &imp));
    {
        STARTUPINFOW si = {sizeof(si)};
        PROCESS_INFORMATION pi = {0};
        WCHAR cmd[] = L"C:\\gltest64.exe";
        BOOL ok = CreateProcessAsUserW(rtok, NULL, cmd, NULL, NULL, FALSE, CREATE_SUSPENDED, NULL, NULL, &si, &pi);
        printf("%-34s %s (err %lu)\n", "CreateProcessAsUser(suspended)", ok ? "ok" : "FAILED", ok ? 0 : GetLastError());
        if (ok)
        {
            HANDLE t = pi.hThread;
            STEP("SetThreadToken(child thread)", SetThreadToken(&t, imp));
            TerminateProcess(pi.hProcess, 0);
        }
    }
    return 0;
}
