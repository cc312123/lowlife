#pragma once
#include <windows.h>
#include <winternl.h>

struct _STRING64
{
    USHORT Length;                                                          
    USHORT MaximumLength;                                                   
    ULONGLONG Buffer;                                                       
};

struct _PEB64
{
    UCHAR InheritedAddressSpace;                                            
    UCHAR ReadImageFileExecOptions;                                         
    UCHAR BeingDebugged;                                                    
    union
    {
        UCHAR BitField;                                                     
        struct
        {
            UCHAR ImageUsesLargePages : 1;                                  
            UCHAR IsProtectedProcess : 1;                                   
            UCHAR IsImageDynamicallyRelocated : 1;                          
            UCHAR SkipPatchingUser32Forwarders : 1;                         
            UCHAR IsPackagedProcess : 1;                                    
            UCHAR IsAppContainer : 1;                                       
            UCHAR IsProtectedProcessLight : 1;                              
            UCHAR IsLongPathAwareProcess : 1;                               
        };
    };
    UCHAR Padding0[4];                                                      
    ULONGLONG Mutant;                                                       
    ULONGLONG ImageBaseAddress;                                             
    ULONGLONG Ldr;                                                          
    ULONGLONG ProcessParameters;                                            
    ULONGLONG SubSystemData;                                                
    ULONGLONG ProcessHeap;                                                  
    ULONGLONG FastPebLock;                                                  
    ULONGLONG AtlThunkSListPtr;                                             
    ULONGLONG IFEOKey;                                                      
    union
    {
        ULONG CrossProcessFlags;                                            
        struct
        {
            ULONG ProcessInJob : 1;                                         
            ULONG ProcessInitializing : 1;                                  
            ULONG ProcessUsingVEH : 1;                                      
            ULONG ProcessUsingVCH : 1;                                      
            ULONG ProcessUsingFTH : 1;                                      
            ULONG ProcessPreviouslyThrottled : 1;                           
            ULONG ProcessCurrentlyThrottled : 1;                            
            ULONG ProcessImagesHotPatched : 1;                              
            ULONG ReservedBits0 : 24;                                       
        };
    };
    UCHAR Padding1[4];                                                      
    union
    {
        ULONGLONG KernelCallbackTable;                                      
        ULONGLONG UserSharedInfoPtr;                                        
    };
    ULONG SystemReserved;                                                   
    ULONG AtlThunkSListPtr32;                                               
    ULONGLONG ApiSetMap;                                                    
    ULONG TlsExpansionCounter;                                              
    UCHAR Padding2[4];                                                      
    ULONGLONG TlsBitmap;                                                    
    ULONG TlsBitmapBits[2];                                                 
    ULONGLONG ReadOnlySharedMemoryBase;                                     
    ULONGLONG SharedData;                                                   
    ULONGLONG ReadOnlyStaticServerData;                                     
    ULONGLONG AnsiCodePageData;                                             
    ULONGLONG OemCodePageData;                                              
    ULONGLONG UnicodeCaseTableData;                                         
    ULONG NumberOfProcessors;                                               
    ULONG NtGlobalFlag;                                                     
    union _LARGE_INTEGER CriticalSectionTimeout;                            
    ULONGLONG HeapSegmentReserve;                                           
    ULONGLONG HeapSegmentCommit;                                            
    ULONGLONG HeapDeCommitTotalFreeThreshold;                               
    ULONGLONG HeapDeCommitFreeBlockThreshold;                               
    ULONG NumberOfHeaps;                                                    
    ULONG MaximumNumberOfHeaps;                                             
    ULONGLONG ProcessHeaps;                                                 
    ULONGLONG GdiSharedHandleTable;                                         
    ULONGLONG ProcessStarterHelper;                                         
    ULONG GdiDCAttributeList;                                               
    UCHAR Padding3[4];                                                      
    ULONGLONG LoaderLock;                                                   
    ULONG OSMajorVersion;                                                   
    ULONG OSMinorVersion;                                                   
    USHORT OSBuildNumber;                                                   
    USHORT OSCSDVersion;                                                    
    ULONG OSPlatformId;                                                     
    ULONG ImageSubsystem;                                                   
    ULONG ImageSubsystemMajorVersion;                                       
    ULONG ImageSubsystemMinorVersion;                                       
    UCHAR Padding4[4];                                                      
    ULONGLONG ActiveProcessAffinityMask;                                    
    ULONG GdiHandleBuffer[60];                                              
    ULONGLONG PostProcessInitRoutine;                                       
    ULONGLONG TlsExpansionBitmap;                                           
    ULONG TlsExpansionBitmapBits[32];                                       
    ULONG SessionId;                                                        
    UCHAR Padding5[4];                                                      
    _ULARGE_INTEGER AppCompatFlags;                                         
    _ULARGE_INTEGER AppCompatFlagsUser;                                     
    ULONGLONG pShimData;                                                    
    ULONGLONG AppCompatInfo;                                                
    _STRING64 CSDVersion;                                                   
    ULONGLONG ActivationContextData;                                        
    ULONGLONG ProcessAssemblyStorageMap;                                    
    ULONGLONG SystemDefaultActivationContextData;                           
    ULONGLONG SystemAssemblyStorageMap;                                     
    ULONGLONG MinimumStackCommit;                                           
    ULONGLONG SparePointers[2];                                             
    ULONGLONG PatchLoaderData;                                              
    ULONGLONG ChpeV2ProcessInfo;                                            
    ULONG AppModelFeatureState;                                             
    ULONG SpareUlongs[2];                                                   
    USHORT ActiveCodePage;                                                  
    USHORT OemCodePage;                                                     
    USHORT UseCaseMapping;                                                  
    USHORT UnusedNlsField;                                                  
    ULONGLONG WerRegistrationData;                                          
    ULONGLONG WerShipAssertPtr;                                             
    ULONGLONG EcCodeBitMap;                                                 
    ULONGLONG pImageHeaderHash;                                             
    union
    {
        ULONG TracingFlags;                                                 
        struct
        {
            ULONG HeapTracingEnabled : 1;                                   
            ULONG CritSecTracingEnabled : 1;                                
            ULONG LibLoaderTracingEnabled : 1;                              
            ULONG SpareTracingBits : 29;                                    
        };
    };
    UCHAR Padding6[4];                                                      
    ULONGLONG CsrServerReadOnlySharedMemoryBase;                            
    ULONGLONG TppWorkerpListLock;                                           
    struct LIST_ENTRY64 TppWorkerpList;                                     
    ULONGLONG WaitOnAddressHashTable[128];                                  
    ULONGLONG TelemetryCoverageHeader;                                      
    ULONG CloudFileFlags;                                                   
    ULONG CloudFileDiagFlags;                                               
    CHAR PlaceholderCompatibilityMode;                                      
    CHAR PlaceholderCompatibilityModeReserved[7];                           
    ULONGLONG LeapSecondData;                                               
    union
    {
        ULONG LeapSecondFlags;                                              
        struct
        {
            ULONG SixtySecondEnabled : 1;                                   
            ULONG Reserved : 31;                                            
        };
    };
    ULONG NtGlobalFlag2;                                                    
    ULONGLONG ExtendedFeatureDisableMask;                                   
};

void debugger_detection();