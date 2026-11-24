#include "ros_wow64_private.h"

NTSTATUS
WINAPI
wow64_NtContinue(UINT *pArgs)
{
    PWOW64_CONTEXT pContext32 = get_ptr(&pArgs);
    BOOLEAN bRasieAlert = get_ulong(&pArgs);

    NTSTATUS Status = pContext32->Eax;
    struct user_apc_frame *Frame = NtCurrentTeb()->TlsSlots[WOW64_TLS_APCLIST];

    pBTCpuSetContext(NtCurrentThread(), NtCurrentProcess(), NULL, pContext32);

    while (Frame && Frame->wow_context != pContext32)
    {
        Frame = Frame->prev_frame;
    } 

    NtCurrentTeb()->TlsSlots[WOW64_TLS_APCLIST] = Frame ? Frame->prev_frame : NULL;
    
    if (Frame) 
    {   
        NtContinue(Frame->context, bRasieAlert);
    }
    else
    {
        /* Kush 1.1: an ordinary 32-bit NtContinue (KiUserExceptionDispatcher
         * after a handled exception, SetThreadContext-style resumes...).
         * BTCpuSetContext relies on ThreadWow64Context, which the kernel does
         * not implement, so nothing happened and ZwContinue simply returned
         * to its caller: every handled 32-bit exception then became a nested
         * "exception 0x0". Continue directly with a native context in
         * compatibility mode (CS 0x23), like the thread start does. */
        CONTEXT Context;

        Wow64CopyContext32To64(&Context, pContext32);
        Context.SegCs = 0x23;
        Context.SegSs = 0x2b;
        Context.SegDs = 0x2b;
        Context.SegEs = 0x2b;
        Context.SegGs = 0x2b;
        Context.SegFs = 0x53;
        return NtContinue(&Context, bRasieAlert);
    }

    if (bRasieAlert) NtTestAlert();
    return Status;
}

VOID 
WINAPI 
Wow64ApcRoutine(ULONG_PTR SystemArgument1, 
                ULONG_PTR SystemArgument2, 
                ULONG_PTR SystemArgument3, 
                PCONTEXT  Context)
{
#pragma pack(push, 1)
    struct Enter32
    {
        ULONG_PTR       Rip32;
        ULONG_PTR       SegCs32;
        ULONG           FarReturn32;
        ULONG           Func;
        UINT            SystemArgument1;
        UINT            SystemArgument2;
        UINT            SystemArgument3;
        UINT            Alertable;
        WOW64_CONTEXT   Context32;
        struct user_apc_frame Frame;
    };
    
    struct Leave32
    {
        ULONG Rip64;
        ULONG SegCs64;
    };        

    ULONG_PTR Enter32(PVOID enter);
    VOID FarReturn32();
    
    struct Leave32* leave;
    struct Enter32* enter;
    
    BYTE StackBuffer[sizeof(struct Enter32) + sizeof(struct Leave32)];
    
    leave = (struct Leave32*)(StackBuffer + sizeof(StackBuffer) - sizeof(*leave));
    enter = (struct Enter32*)(StackBuffer + sizeof(StackBuffer) - sizeof(*leave) - sizeof(*enter));
#pragma pack(pop)
    
    enter->FarReturn32 = PtrToUlong(FarReturn32);
    
    enter->Frame.prev_frame = NtCurrentTeb()->TlsSlots[WOW64_TLS_APCLIST];
    enter->Frame.context    = Context;
    NtCurrentTeb()->TlsSlots[WOW64_TLS_APCLIST] = &enter->Frame;

    Wow64CopyContext64To32(&enter->Context32, Context);

    enter->Func = SystemArgument1 >> 32;
    enter->SystemArgument1 = SystemArgument1;
    enter->SystemArgument2 = SystemArgument2;
    enter->SystemArgument3 = SystemArgument3;
    enter->Alertable = TRUE;
    enter->Frame.wow_context = &enter->Context32;

    enter->Rip32 = (ULONG_PTR) NtDll32KiUserApcDispatcher;
    enter->SegCs32 = 0x23;
    
    leave->Rip64 = PtrToUlong(_ReturnAddress());
    leave->SegCs64 = 0x33;
    
    Enter32(enter);
    
    /* We should never get here. */
    ASSERT(FALSE);
}