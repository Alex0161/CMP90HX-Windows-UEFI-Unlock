#ifndef UEFI_MIN_H
#define UEFI_MIN_H

#include <stddef.h>
#include <stdint.h>

#if defined(__x86_64__)
#define EFIAPI __attribute__((ms_abi))
#define SYSVABI __attribute__((sysv_abi))
#else
#error This project supports x86-64 UEFI only
#endif

typedef uint8_t BOOLEAN;
typedef uint8_t UINT8;
typedef uint16_t UINT16;
typedef uint32_t UINT32;
typedef uint64_t UINT64;
typedef int64_t INT64;
typedef uint64_t UINTN;
typedef uint16_t CHAR16;
typedef void *EFI_HANDLE;
typedef UINT64 EFI_STATUS;
typedef UINT64 EFI_PHYSICAL_ADDRESS;
typedef UINT64 EFI_VIRTUAL_ADDRESS;
typedef UINTN EFI_TPL;

#define EFI_SUCCESS 0
#define EFIERR(n) (0x8000000000000000ULL | (n))
#define EFI_LOAD_ERROR EFIERR(1)
#define EFI_INVALID_PARAMETER EFIERR(2)
#define EFI_UNSUPPORTED EFIERR(3)
#define EFI_BAD_BUFFER_SIZE EFIERR(4)
#define EFI_BUFFER_TOO_SMALL EFIERR(5)
#define EFI_NOT_READY EFIERR(6)
#define EFI_DEVICE_ERROR EFIERR(7)
#define EFI_OUT_OF_RESOURCES EFIERR(9)
#define EFI_NOT_FOUND EFIERR(14)
#define EFI_ABORTED EFIERR(21)
#define EFI_ERROR(s) (((INT64)(s)) < 0)

typedef struct {
    UINT32 Data1;
    UINT16 Data2;
    UINT16 Data3;
    UINT8 Data4[8];
} EFI_GUID;

typedef struct {
    UINT64 Signature;
    UINT32 Revision;
    UINT32 HeaderSize;
    UINT32 CRC32;
    UINT32 Reserved;
} EFI_TABLE_HEADER;

struct efi_simple_text_output_protocol;
typedef EFI_STATUS (EFIAPI *EFI_TEXT_RESET)(
    struct efi_simple_text_output_protocol *, BOOLEAN);
typedef EFI_STATUS (EFIAPI *EFI_TEXT_STRING)(
    struct efi_simple_text_output_protocol *, const CHAR16 *);

typedef struct efi_simple_text_output_protocol {
    EFI_TEXT_RESET Reset;
    EFI_TEXT_STRING OutputString;
    void *TestString;
    void *QueryMode;
    void *SetMode;
    void *SetAttribute;
    void *ClearScreen;
    void *SetCursorPosition;
    void *EnableCursor;
    void *Mode;
} EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL;

typedef enum {
    AllocateAnyPages,
    AllocateMaxAddress,
    AllocateAddress,
    MaxAllocateType
} EFI_ALLOCATE_TYPE;

typedef enum {
    EfiReservedMemoryType,
    EfiLoaderCode,
    EfiLoaderData,
    EfiBootServicesCode,
    EfiBootServicesData,
    EfiRuntimeServicesCode,
    EfiRuntimeServicesData,
    EfiConventionalMemory,
    EfiUnusableMemory,
    EfiACPIReclaimMemory,
    EfiACPIMemoryNVS,
    EfiMemoryMappedIO,
    EfiMemoryMappedIOPortSpace,
    EfiPalCode,
    EfiPersistentMemory,
    EfiUnacceptedMemoryType,
    EfiMaxMemoryType
} EFI_MEMORY_TYPE;

typedef enum {
    AllHandles,
    ByRegisterNotify,
    ByProtocol
} EFI_LOCATE_SEARCH_TYPE;

typedef EFI_STATUS (EFIAPI *EFI_ALLOCATE_PAGES)(
    EFI_ALLOCATE_TYPE, EFI_MEMORY_TYPE, UINTN, EFI_PHYSICAL_ADDRESS *);
typedef EFI_STATUS (EFIAPI *EFI_FREE_PAGES)(EFI_PHYSICAL_ADDRESS, UINTN);
typedef EFI_STATUS (EFIAPI *EFI_ALLOCATE_POOL)(EFI_MEMORY_TYPE, UINTN, void **);
typedef EFI_STATUS (EFIAPI *EFI_FREE_POOL)(void *);
typedef EFI_STATUS (EFIAPI *EFI_HANDLE_PROTOCOL)(EFI_HANDLE, EFI_GUID *, void **);
typedef EFI_STATUS (EFIAPI *EFI_LOCATE_HANDLE_BUFFER)(
    EFI_LOCATE_SEARCH_TYPE, EFI_GUID *, void *, UINTN *, EFI_HANDLE **);
typedef EFI_STATUS (EFIAPI *EFI_LOCATE_PROTOCOL)(EFI_GUID *, void *, void **);
typedef EFI_STATUS (EFIAPI *EFI_STALL)(UINTN);
struct efi_device_path_protocol;
typedef EFI_STATUS (EFIAPI *EFI_LOAD_IMAGE)(
    BOOLEAN, EFI_HANDLE, struct efi_device_path_protocol *,
    void *, UINTN, EFI_HANDLE *);
typedef EFI_STATUS (EFIAPI *EFI_START_IMAGE)(EFI_HANDLE, UINTN *, CHAR16 **);
typedef EFI_STATUS (EFIAPI *EFI_UNLOAD_IMAGE)(EFI_HANDLE);
typedef EFI_STATUS (EFIAPI *EFI_SET_WATCHDOG_TIMER)(
    UINTN, UINT64, UINTN, CHAR16 *);

typedef struct {
    EFI_TABLE_HEADER Hdr;
    void *RaiseTPL;
    void *RestoreTPL;
    EFI_ALLOCATE_PAGES AllocatePages;
    EFI_FREE_PAGES FreePages;
    void *GetMemoryMap;
    EFI_ALLOCATE_POOL AllocatePool;
    EFI_FREE_POOL FreePool;
    void *CreateEvent;
    void *SetTimer;
    void *WaitForEvent;
    void *SignalEvent;
    void *CloseEvent;
    void *CheckEvent;
    void *InstallProtocolInterface;
    void *ReinstallProtocolInterface;
    void *UninstallProtocolInterface;
    EFI_HANDLE_PROTOCOL HandleProtocol;
    void *Reserved;
    void *RegisterProtocolNotify;
    void *LocateHandle;
    void *LocateDevicePath;
    void *InstallConfigurationTable;
    EFI_LOAD_IMAGE LoadImage;
    EFI_START_IMAGE StartImage;
    void *Exit;
    EFI_UNLOAD_IMAGE UnloadImage;
    void *ExitBootServices;
    void *GetNextMonotonicCount;
    EFI_STALL Stall;
    EFI_SET_WATCHDOG_TIMER SetWatchdogTimer;
    void *ConnectController;
    void *DisconnectController;
    void *OpenProtocol;
    void *CloseProtocol;
    void *OpenProtocolInformation;
    void *ProtocolsPerHandle;
    EFI_LOCATE_HANDLE_BUFFER LocateHandleBuffer;
    EFI_LOCATE_PROTOCOL LocateProtocol;
    void *InstallMultipleProtocolInterfaces;
    void *UninstallMultipleProtocolInterfaces;
    void *CalculateCrc32;
    void *CopyMem;
    void *SetMem;
    void *CreateEventEx;
} EFI_BOOT_SERVICES;

typedef struct {
    EFI_TABLE_HEADER Hdr;
    CHAR16 *FirmwareVendor;
    UINT32 FirmwareRevision;
    UINT32 Pad;
    EFI_HANDLE ConsoleInHandle;
    void *ConIn;
    EFI_HANDLE ConsoleOutHandle;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConOut;
    EFI_HANDLE StandardErrorHandle;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *StdErr;
    void *RuntimeServices;
    EFI_BOOT_SERVICES *BootServices;
    UINTN NumberOfTableEntries;
    void *ConfigurationTable;
} EFI_SYSTEM_TABLE;

typedef struct {
    UINT32 Revision;
    EFI_HANDLE ParentHandle;
    EFI_SYSTEM_TABLE *SystemTable;
    EFI_HANDLE DeviceHandle;
    void *FilePath;
    void *Reserved;
    UINT32 LoadOptionsSize;
    UINT32 Pad;
    void *LoadOptions;
    void *ImageBase;
    UINT64 ImageSize;
    EFI_MEMORY_TYPE ImageCodeType;
    EFI_MEMORY_TYPE ImageDataType;
    void *Unload;
} EFI_LOADED_IMAGE_PROTOCOL;

typedef enum {
    EfiPciWidthUint8,
    EfiPciWidthUint16,
    EfiPciWidthUint32,
    EfiPciWidthUint64,
    EfiPciWidthFifoUint8,
    EfiPciWidthFifoUint16,
    EfiPciWidthFifoUint32,
    EfiPciWidthFifoUint64,
    EfiPciWidthFillUint8,
    EfiPciWidthFillUint16,
    EfiPciWidthFillUint32,
    EfiPciWidthFillUint64,
    EfiPciWidthMaximum
} EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL_WIDTH;

struct efi_pci_root_bridge_io_protocol;
typedef EFI_STATUS (EFIAPI *EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL_IO_MEM)(
    struct efi_pci_root_bridge_io_protocol *,
    EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL_WIDTH,
    UINT64,
    UINTN,
    void *);

typedef struct {
    EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL_IO_MEM Read;
    EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL_IO_MEM Write;
} EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL_ACCESS;

typedef struct efi_pci_root_bridge_io_protocol {
    EFI_HANDLE ParentHandle;
    void *PollMem;
    void *PollIo;
    EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL_ACCESS Mem;
    EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL_ACCESS Io;
    EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL_ACCESS Pci;
    void *CopyMem;
    void *Map;
    void *Unmap;
    void *AllocateBuffer;
    void *FreeBuffer;
    void *Flush;
    void *GetAttributes;
    void *SetAttributes;
    void *Configuration;
    UINT32 SegmentNumber;
} EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL;

/* Minimal EFI_PCI_IO_PROTOCOL definition for fast enumeration. */
struct efi_pci_io_protocol;
typedef EFI_STATUS (EFIAPI *EFI_PCI_IO_PROTOCOL_CONFIG)(
    struct efi_pci_io_protocol *,
    EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL_WIDTH,
    UINT32, UINTN, void *);
typedef struct {
    EFI_PCI_IO_PROTOCOL_CONFIG Read;
    EFI_PCI_IO_PROTOCOL_CONFIG Write;
} EFI_PCI_IO_PROTOCOL_CONFIG_ACCESS;
typedef EFI_STATUS (EFIAPI *EFI_PCI_IO_PROTOCOL_GET_LOCATION)(
    struct efi_pci_io_protocol *, UINTN *, UINTN *, UINTN *, UINTN *);

typedef struct efi_pci_io_protocol {
    void *PollMem;
    void *PollIo;
    EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL_ACCESS Mem;
    EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL_ACCESS Io;
    EFI_PCI_IO_PROTOCOL_CONFIG_ACCESS Pci;
    void *CopyMem;
    void *Map;
    void *Unmap;
    void *AllocateBuffer;
    void *FreeBuffer;
    void *Flush;
    EFI_PCI_IO_PROTOCOL_GET_LOCATION GetLocation;
    void *Attributes;
    void *GetBarAttributes;
    void *SetBarAttributes;
    UINT64 RomSize;
    void *RomImage;
} EFI_PCI_IO_PROTOCOL;

typedef struct efi_device_path_protocol {
    UINT8 Type;
    UINT8 SubType;
    UINT8 Length[2];
} EFI_DEVICE_PATH_PROTOCOL;

struct efi_file_protocol;
typedef EFI_STATUS (EFIAPI *EFI_FILE_OPEN)(
    struct efi_file_protocol *, struct efi_file_protocol **,
    CHAR16 *, UINT64, UINT64);
typedef EFI_STATUS (EFIAPI *EFI_FILE_CLOSE)(struct efi_file_protocol *);

typedef struct efi_file_protocol {
    UINT64 Revision;
    EFI_FILE_OPEN Open;
    EFI_FILE_CLOSE Close;
    void *Delete;
    void *Read;
    void *Write;
    void *GetPosition;
    void *SetPosition;
    void *GetInfo;
    void *SetInfo;
    void *Flush;
    void *OpenEx;
    void *ReadEx;
    void *WriteEx;
    void *FlushEx;
} EFI_FILE_PROTOCOL;

struct efi_simple_file_system_protocol;
typedef EFI_STATUS (EFIAPI *EFI_OPEN_VOLUME)(
    struct efi_simple_file_system_protocol *, EFI_FILE_PROTOCOL **);

typedef struct efi_simple_file_system_protocol {
    UINT64 Revision;
    EFI_OPEN_VOLUME OpenVolume;
} EFI_SIMPLE_FILE_SYSTEM_PROTOCOL;

#define EFI_FILE_MODE_READ 0x0000000000000001ULL
#define MEDIA_DEVICE_PATH 0x04U
#define MEDIA_FILEPATH_DP 0x04U
#define END_DEVICE_PATH_TYPE 0x7fU
#define END_ENTIRE_DEVICE_PATH_SUBTYPE 0xffU

#define EFI_PCI_ADDRESS(bus, dev, func, reg) \
    ((((UINT64)(bus)) << 24) | (((UINT64)(dev)) << 16) | \
     (((UINT64)(func)) << 8) | ((UINT64)(reg)))

static const EFI_GUID EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL_GUID = {
    0x2f707ebb, 0x4a1a, 0x11d4,
    {0x9a, 0x38, 0x00, 0x90, 0x27, 0x3f, 0xc1, 0x4d}
};

static const EFI_GUID EFI_PCI_IO_PROTOCOL_GUID = {
    0x4cf5b200, 0x68b8, 0x4ca5,
    {0x9e, 0xec, 0xb2, 0x3e, 0x3f, 0x50, 0x02, 0x9a}
};

static const EFI_GUID EFI_LOADED_IMAGE_PROTOCOL_GUID = {
    0x5b1b31a1, 0x9562, 0x11d2,
    {0x8e, 0x3f, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b}
};

static const EFI_GUID EFI_DEVICE_PATH_PROTOCOL_GUID = {
    0x09576e91, 0x6d3f, 0x11d2,
    {0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b}
};

static const EFI_GUID EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID = {
    0x964e5b22, 0x6459, 0x11d2,
    {0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b}
};

_Static_assert(offsetof(EFI_SYSTEM_TABLE, BootServices) == 96,
               "EFI_SYSTEM_TABLE layout mismatch");
_Static_assert(offsetof(EFI_BOOT_SERVICES, HandleProtocol) == 152,
               "EFI_BOOT_SERVICES HandleProtocol offset mismatch");
_Static_assert(offsetof(EFI_BOOT_SERVICES, LoadImage) == 200,
               "EFI_BOOT_SERVICES LoadImage offset mismatch");
_Static_assert(offsetof(EFI_BOOT_SERVICES, StartImage) == 208,
               "EFI_BOOT_SERVICES StartImage offset mismatch");
_Static_assert(offsetof(EFI_BOOT_SERVICES, Stall) == 248,
               "EFI_BOOT_SERVICES Stall offset mismatch");
_Static_assert(offsetof(EFI_BOOT_SERVICES, SetWatchdogTimer) == 256,
               "EFI_BOOT_SERVICES SetWatchdogTimer offset mismatch");
_Static_assert(offsetof(EFI_BOOT_SERVICES, LocateHandleBuffer) == 312,
               "EFI_BOOT_SERVICES LocateHandleBuffer offset mismatch");
_Static_assert(offsetof(EFI_LOADED_IMAGE_PROTOCOL, LoadOptionsSize) == 48,
               "EFI_LOADED_IMAGE_PROTOCOL layout mismatch");
_Static_assert(offsetof(EFI_LOADED_IMAGE_PROTOCOL, LoadOptions) == 56,
               "EFI_LOADED_IMAGE_PROTOCOL load options offset mismatch");
_Static_assert(offsetof(EFI_PCI_IO_PROTOCOL, Pci) == 48,
               "EFI_PCI_IO_PROTOCOL.Pci ABI mismatch");
_Static_assert(offsetof(EFI_PCI_IO_PROTOCOL, GetLocation) == 112,
               "EFI_PCI_IO_PROTOCOL.GetLocation ABI mismatch");
_Static_assert(offsetof(EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL, Pci) == 56,
               "PCI Root Bridge I/O protocol layout mismatch");
_Static_assert(offsetof(EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL, SegmentNumber) == 144,
               "PCI Root Bridge segment offset mismatch");
_Static_assert(offsetof(EFI_FILE_PROTOCOL, Open) == 8,
               "EFI_FILE_PROTOCOL Open offset mismatch");
_Static_assert(offsetof(EFI_FILE_PROTOCOL, Close) == 16,
               "EFI_FILE_PROTOCOL Close offset mismatch");

#endif
