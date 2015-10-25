/** @file
    A simple, basic, application showing how the Hello application could be
    built using the "Standard C Libraries" from StdLib.

    Copyright (c) 2010 - 2011, Intel Corporation. All rights reserved.<BR>
    This program and the accompanying materials
    are licensed and made available under the terms and conditions of the BSD License
    which accompanies this distribution. The full text of the license may be found at
    http://opensource.org/licenses/bsd-license.

    THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
    WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
**/
#include  <Uefi.h>
#include  <Library/UefiLib.h>
#include <Protocol/SimpleFileSystem.h>
#include <Guid/FileInfo.h>

#include  <stdio.h>
#include  <stdint.h>

//#include "elfparse.h"


EFI_SYSTEM_TABLE  *gST;
EFI_BOOT_SERVICES *gBS;

#define PAGE_SIZE 4096

#define ADDR_CMDLINE 0x7e00
#define ADDR_TARGET 0x200000
#define ADDR_MB_INFO 0x1000
#define ADDR_E820DATA 0x1100
#define ADDR_STACK 0x1200

#define ENTRY_ADDR 0x000000000021022e

#define E820_USABLE 1
#define E820_RESERVED 2


struct e820ent {
  uint32_t ent_size;
  uint64_t addr;
  uint64_t size;
  uint32_t type;
} __attribute__((packed));

struct multiboot_info_type {
  uint32_t flags;
  uint32_t mem_lower;
  uint32_t mem_upper;
  uint32_t boot_device;
  uint32_t cmdline;
  uint32_t mods_count;
  uint32_t mods_addr;
  uint32_t syms[4];
  uint32_t mmap_length;
  uint32_t mmap_addr;
  uint32_t drives_length;
  uint32_t drives_addr;
  uint32_t config_table;
  uint32_t boot_loader_name;
  uint32_t apm_table;
  uint32_t vbe_control_info;
  uint32_t vbe_mode_info;
  uint16_t vbe_mode;
  uint16_t vbe_interface_seg;
  uint16_t vbe_interface_off;
  uint16_t vbe_interface_len;
} __attribute__((packed));


void Memmap_to_e820(struct e820ent *e, EFI_MEMORY_DESCRIPTOR *md)
{
  e->ent_size = 20;
  e->addr = md->PhysicalStart;
  e->size = md->NumberOfPages * PAGE_SIZE; // PAGE_SIZE = 4096(4KiB)
  switch (md->Type) {
  case EfiLoaderCode:
  case EfiLoaderData:
  case EfiBootServicesCode:
  case EfiBootServicesData:
  case EfiConventionalMemory:
    if (md->Attribute & EFI_MEMORY_WB) {
      e->type = E820_USABLE;
    }
    else {
      e->type = E820_RESERVED;
    }
    break;
  default:
    e->type = E820_RESERVED;
    break;
  }
}

int
memory_verify(uint8_t *src, uint8_t *dest, int size){
  for (int i = 0; i < size; i++) {
    if (src[i] != dest[i]) {
      return -1;
    }
  }
  return 0;
}

EFI_STATUS EFIAPI loader2(  IN VOID *Kernel,  IN VOID *E820,  IN VOID *MB_INFO,  IN VOID *CMDLINE  );


EFI_STATUS
EFIAPI
UefiMain (
	  IN     EFI_HANDLE        ImageHandle,
	  IN     EFI_SYSTEM_TABLE  *SystemTable
	  )
{
  EFI_STATUS Status = EFI_SUCCESS;

  gST = SystemTable;
  gBS = gST->BootServices;

  UINTN MemmapSize = PAGE_SIZE;
  VOID *Memmap;
  
  UINTN MapKey;
  UINTN DescriptorSize;
  UINT32 DescriptorVersion;

  
  Status = gBS->AllocatePool(
			     EfiLoaderData,
			     MemmapSize,
			     (VOID **)&Memmap
			     );
  if (EFI_ERROR (Status)) {
    Print(L"%Could not allocate memory pool %r\n", Status);
    return Status;
  }
  
  Status = gBS->GetMemoryMap(
			     &MemmapSize,
			     Memmap,
			     &MapKey,
			     &DescriptorSize,
			     &DescriptorVersion
			     );
  if (EFI_ERROR (Status)) {
    Print(L"%Could not get memory map %r\n", Status);
    return Status;
  }

  /* Print(L"Memmap = %p\n", Memmap); */
  /* Print(L"MemmapSize = %d\n", MemmapSize); */
  /* Print(L"MapKey = %d\n", MapKey); */
  /* Print(L"sizeof(EFI_MEMORY_DESCRIPTOR) = %d\n", sizeof(EFI_MEMORY_DESCRIPTOR)); */
  /* Print(L"DescriptorSize = %d\n", DescriptorSize); */
  /* Print(L"DescriptorVersion = %d\n", DescriptorVersion); */

  struct e820ent *e820data;
  UINT32 e820_size = sizeof(struct e820ent) * (MemmapSize / DescriptorSize);
  Status = gBS->AllocatePool(
			     EfiLoaderData,
			     e820_size,
			     (VOID **)&e820data
			     );
  if (EFI_ERROR (Status)) {
    Print(L"%Could not allocate memory pool %r\n", Status);
    return Status;
  }
  

  for (int i = 0; i < (MemmapSize / DescriptorSize); i++)
  {
    EFI_MEMORY_DESCRIPTOR *md = Memmap + (i * DescriptorSize);
    /* Print(L"memmap: 0x%08x, 0x%016x, 0x%016x, %10ld, 0x%016x\n", */
    /* 	  md->Type, */
    /* 	  md->PhysicalStart, */
    /* 	  md->VirtualStart, */
    /* 	  md->NumberOfPages, */
    /* 	  md->Attribute */
    /* 	  ); */
    Memmap_to_e820(&(e820data[i]), md);
    Print(L"E820: %d, 0x%016x-0x%016x\n",
    	  e820data[i].type,
    	  e820data[i].addr,
    	  e820data[i].addr + e820data[i].size
    	  );   
  }  

  // set mb_info //
  // zero clear
  struct multiboot_info_type mb_info;
  UINT8 *mb = (UINT8 *)&mb_info;
  for (int i = 0; i < sizeof(mb_info); i++) {
    mb[i] = 0;
  }
  mb_info.cmdline = ADDR_CMDLINE;
  mb_info.mmap_addr = ADDR_E820DATA;
  mb_info.mmap_length = e820_size;
  // set mb_info to ADDR_MB_INFO
  UINTN mb_info_mem_size = 0;
  EFI_PHYSICAL_ADDRESS mb_info_mem;
  mb_info_mem_size = sizeof(mb_info) / 4096 + 1;
  mb_info_mem = (EFI_PHYSICAL_ADDRESS)ADDR_MB_INFO;
  Status = gBS->AllocatePages(
			     AllocateAddress,
			     EfiLoaderData,
			     mb_info_mem_size, // 1page = 4KiB 
			     &mb_info_mem
			     );
  if (EFI_ERROR (Status)) {
    Print(L"%Could not allocate memory pool at ADDR_MB_INFO %r\n", Status);
    return Status;
  }  

  // load kernel
  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL  *SimpleFile;
  EFI_FILE_PROTOCOL                *Root;
  EFI_FILE_PROTOCOL                *File;
  CHAR16 *Path = L"loader-stripped.elf";

  Status = gBS->LocateProtocol (
				&gEfiSimpleFileSystemProtocolGuid,
				NULL,
				(VOID **)&SimpleFile
				);
  if (EFI_ERROR (Status)) {
    Print(L"%r on Locate EFI Simple File System Protocol.\n", Status);
    return Status;
  }

  Status = SimpleFile->OpenVolume(SimpleFile, &Root);
  if (EFI_ERROR (Status)) {
    Print(L"%r on Open volume.\n", Status);
    return Status;
  }

  Status = Root->Open(Root, &File, Path, EFI_FILE_MODE_READ, EFI_FILE_READ_ONLY);
  if (EFI_ERROR (Status)) {
    Print(L"%r on Open file.\n", Status);
    return Status;
  }

  // get file info
  EFI_FILE_INFO *FileInfo;
  UINTN FileInfoSize = sizeof(EFI_FILE_INFO) * 2;
  Status = gBS->AllocatePool(
			     EfiLoaderData,
			     FileInfoSize,
			     (VOID **)&FileInfo
			     );
  if (EFI_ERROR (Status)) {
    Print(L"%Could not allocate memory pool %r\n", Status);
    return Status;
  }

  Status = File->GetInfo(File, &gEfiFileInfoGuid, &FileInfoSize, FileInfo);
  if (EFI_ERROR (Status)) {
    Print(L"%Could not get FileInfo: %r\n", Status);
    return Status;
  }

  // get file size
  UINT64 BufferSize = FileInfo->FileSize;
  Print(L"FileSize = %d\n", BufferSize);
  
  // allocate FileBuffer
  VOID *Buffer;
  Status = gBS->AllocatePool(
			     EfiLoaderData,
			     BufferSize,
			     (VOID **)&Buffer
			     );
  if (EFI_ERROR (Status)) {
    Print(L"%Could not allocate memory pool %r\n", Status);
    return Status;
  }

  Status = File->Read(
		      File,
		      &BufferSize,
		      (VOID *)Buffer
		      );
  if (EFI_ERROR (Status)) {
    Print(L"%r on Open file.\n", Status);
    return Status;
  }

  File->Close(File);
  Root->Close(Root);


  gBS->FreePool(Memmap);
  gBS->FreePool(e820data);
  gBS->FreePages(mb_info_mem, mb_info_mem_size);
  gBS->FreePool(Buffer);
    
  return EFI_SUCCESS;
}

