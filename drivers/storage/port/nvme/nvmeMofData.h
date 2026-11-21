/* Kush 1.1 / ReactOS: hand-written equivalent of the header that wmimofck
 * generates from nvmeMofData.mof (no MOF compiler in the ReactOS build). */
#ifndef _nvmeMofData_h_
#define _nvmeMofData_h_

// NVMe_DataType - {C105BB0D-C2D8-415B-9B0D-91A29979EBDD}
#define NVMe_DataTypeGuid { 0xc105bb0d,0xc2d8,0x415b, { 0x9b,0x0d,0x91,0xa2,0x99,0x79,0xeb,0xdd } }
typedef struct _NVMe_DataType
{
    LONG Id;
    ULONGLONG u64;
} NVMe_DataType, *PNVMe_DataType;

// NVMe_QueryDevInfo - {0F70DA81-FD3F-40CB-8EF1-52385F31DB2F}
#define NVMe_QueryDevInfoGuid { 0x0f70da81,0xfd3f,0x40cb, { 0x8e,0xf1,0x52,0x38,0x5f,0x31,0xdb,0x2f } }
DEFINE_GUID(NVMe_QueryDevInfo_GUID, 0x0f70da81,0xfd3f,0x40cb,0x8e,0xf1,0x52,0x38,0x5f,0x31,0xdb,0x2f);
typedef struct _NVMe_QueryDevInfo
{
    ULONG maxDataXferSize;
    ULONG numberOfNamespaces;
    NVMe_DataType Data;
} NVMe_QueryDevInfo, *PNVMe_QueryDevInfo;
#define NVMe_QueryDevInfo_SIZE (FIELD_OFFSET(NVMe_QueryDevInfo, Data) + sizeof(NVMe_DataType))

// NVMe_Method - {3865DDFA-7774-4CF4-B5CA-B7569A28FBB4}
#define NVMe_MethodGuid { 0x3865ddfa,0x7774,0x4cf4, { 0xb5,0xca,0xb7,0x56,0x9a,0x28,0xfb,0xb4 } }
DEFINE_GUID(NVMe_Method_GUID, 0x3865ddfa,0x7774,0x4cf4,0xb5,0xca,0xb7,0x56,0x9a,0x28,0xfb,0xb4);

#define GetControllerInfo 1
typedef struct _GetControllerInfo_OUT
{
    USHORT pciVendorId;
    USHORT pciSsVendId;
} GetControllerInfo_OUT, *PGetControllerInfo_OUT;
#define GetControllerInfo_OUT_SIZE (FIELD_OFFSET(GetControllerInfo_OUT, pciSsVendId) + sizeof(USHORT))

#define GetNameSpaceInfo 2
typedef struct _GetNameSpaceInfo_IN
{
    ULONG lunId;
} GetNameSpaceInfo_IN, *PGetNameSpaceInfo_IN;
#define GetNameSpaceInfo_IN_SIZE (FIELD_OFFSET(GetNameSpaceInfo_IN, lunId) + sizeof(ULONG))
typedef struct _GetNameSpaceInfo_OUT
{
    ULONGLONG nSize;
    ULONGLONG nCap;
} GetNameSpaceInfo_OUT, *PGetNameSpaceInfo_OUT;
#define GetNameSpaceInfo_OUT_SIZE (FIELD_OFFSET(GetNameSpaceInfo_OUT, nCap) + sizeof(ULONGLONG))

#endif
