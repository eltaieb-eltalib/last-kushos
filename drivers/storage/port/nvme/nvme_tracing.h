/* Kush 1.1 / ReactOS: WPP tracing is not available; map the OFA NVMe trace
 * macros to nothing (release) so the driver builds with the ReactOS toolchain. */
#pragma once
#ifndef NVME_TRACING_H
#define NVME_TRACING_H
#define TraceFatal(...)       ((void)0)
#define TraceError(...)       ((void)0)
#define TraceWarning(...)     ((void)0)
#define TraceInformation(...) ((void)0)
#define TraceVerbose(...)     ((void)0)
#define WPP_INIT_TRACING(...) ((void)0)
#define WPP_CLEANUP(...)      ((void)0)
#endif
