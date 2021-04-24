// 06_04_Inject_process.cpp : Defines the entry point for the console application.
//

#include "AAAA.h"

// 06_04_Inject_process.cpp : Defines the entry point for the console application.
//

int main(int argc, char* argv[])
{
	LPVOID pFileBuffer = NULL;
	DWORD dwBufferLength = 0;

	PIMAGE_DOS_HEADER pDosHeader = NULL;
	PIMAGE_NT_HEADERS pNTHeader = NULL;
	PIMAGE_FILE_HEADER pPEHeader = NULL;
	PIMAGE_OPTIONAL_HEADER32 pOptionHeader = NULL;
	PIMAGE_SECTION_HEADER pSectionHeader = NULL;
	PIMAGE_IMPORT_DESCRIPTOR pIMPORT_DESCRIPTOR = NULL;
	PIMAGE_IMPORT_BY_NAME pImage_IMPORT_BY_NAME = NULL;
	
	MyReadFile(&pFileBuffer, &dwBufferLength, argv[1]);
	pDosHeader = (PIMAGE_DOS_HEADER)pFileBuffer;
	pNTHeader = (PIMAGE_NT_HEADERS32)((DWORD)pDosHeader + pDosHeader->e_lfanew);
	pPEHeader = (PIMAGE_FILE_HEADER)((DWORD)pNTHeader + 4);
	pOptionHeader = (PIMAGE_OPTIONAL_HEADER)((DWORD)pPEHeader + IMAGE_SIZEOF_FILE_HEADER);

	//检测是否是'MZ'标志
	if (*(PWORD)pFileBuffer != IMAGE_DOS_SIGNATURE)
	{
		printf("Not 'MZ' signature!\n");
		free(pFileBuffer);
		return -1;
	}

	
	//检测是否是'PE'标志
	pNTHeader = (PIMAGE_NT_HEADERS32)((DWORD)pFileBuffer + pDosHeader->e_lfanew);
	if (pNTHeader->Signature != IMAGE_NT_SIGNATURE)
	{
		printf("Not 'PE' signature\n");
		free(pFileBuffer);
		return -1;
	}

	//检测是否存在重定位表
	DWORD dwFlagRelocation = 0;
	if (pOptionHeader->DataDirectory[5].VirtualAddress == 0)
	{
		dwFlagRelocation = 1;
		printf("EXE Have Not Base Relocation Table!\n");
	}

	LPVOID pImageBuffer = NULL;
	CopyFileBufferToImageBuffer(pFileBuffer, &pImageBuffer); // free pFileBuffer

	// get ImageBase
	DWORD dwImageBase;
	dwImageBase = GetImageBase(pImageBuffer);

	// get SizeOfImage
	DWORD dwSizeOfImage = 0;
	dwSizeOfImage = GetSizeOfImage(pImageBuffer);
	
	// to alloc memory
	LPVOID pAllocAddr = NULL;
	pAllocAddr = VirtualAlloc((PVOID)dwImageBase, (SIZE_T)dwSizeOfImage, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
	if (pAllocAddr == NULL) {
		printf("PE's ImageBase VirtualAlloc Fail, The Error is %d.\n", GetLastError());
		if(dwFlagRelocation){
			printf("No Base Relocation Table, And Pe's ImageBase VirtualAlloc Fail.\n");
			free(pImageBuffer);
			return -1;
		}

		pAllocAddr = VirtualAlloc(NULL, (SIZE_T)dwSizeOfImage, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
		printf("Start Second VirtualAlloc....\n");
		if(pAllocAddr == NULL){
			printf("Second VirtualAlloc Fail, The Error is %d.\n", GetLastError());
			free(pImageBuffer);
			return -1;
		}
		printf("Second VirtualAlloc: %x\n", pAllocAddr);
	}

	printf("Fina VirtualAlloc: %x\n", pAllocAddr);

	// memcpy
	memcpy(pAllocAddr, pImageBuffer, dwSizeOfImage);
	//CopyMemory(pAllocAddr, pImageBuffer, dwSizeOfImage);

	// load pe
	PDWORD pIAT = NULL;
	DWORD Original = 0;

	pDosHeader = (PIMAGE_DOS_HEADER)pAllocAddr;
	pNTHeader = (PIMAGE_NT_HEADERS)((DWORD)pAllocAddr + pDosHeader->e_lfanew);
	pPEHeader = (PIMAGE_FILE_HEADER)(((DWORD)pNTHeader) + 4);
	pOptionHeader = (PIMAGE_OPTIONAL_HEADER32)((DWORD)pPEHeader + IMAGE_SIZEOF_FILE_HEADER);
	pSectionHeader = (PIMAGE_SECTION_HEADER)((DWORD)pOptionHeader + sizeof(IMAGE_OPTIONAL_HEADER32));
	
	
	//	修复重定位表
	FixRelocation(pAllocAddr, (DWORD)pAllocAddr - dwImageBase);
	
	//获取导入表的位置,每个导入表的相关信息占20个字节
	pIMPORT_DESCRIPTOR = (PIMAGE_IMPORT_DESCRIPTOR)((DWORD)pAllocAddr + (DWORD)pOptionHeader->DataDirectory[1].VirtualAddress);
	
	HMODULE hModule;
	//这里可以进行while操作，这里while的判断依据为pIMPORT_DESCRIPTOR个数
	while (pIMPORT_DESCRIPTOR->FirstThunk && pIMPORT_DESCRIPTOR->OriginalFirstThunk) {
		hModule = LoadLibrary((PCHAR)((DWORD)pAllocAddr + (DWORD)pIMPORT_DESCRIPTOR->Name));
		pIAT = (PDWORD)((DWORD)pAllocAddr + (DWORD)pIMPORT_DESCRIPTOR->FirstThunk);
		while (*pIAT) {
			if (*pIAT & 0x80000000) {
				//高位为1 则 除去最高位的值就是函数的导出序号
				Original = *pIAT & 0x7FFFFFFF;	//去除最高标志位。
				*pIAT = (DWORD)GetProcAddress(hModule, (PCHAR)Original);
			}
			else
			{
				//高位不为1 则指向IMAGE_IMPORT_BY_NAME;
				pImage_IMPORT_BY_NAME = (PIMAGE_IMPORT_BY_NAME)((DWORD)pAllocAddr + *pIAT);
				*pIAT = (DWORD)GetProcAddress(hModule, (PCHAR)pImage_IMPORT_BY_NAME->Name);
			}
			pIAT++;
		}
		
		pIMPORT_DESCRIPTOR++;
	}
	
	LPVOID dwOep = (LPVOID)(GetOep(pAllocAddr) + (DWORD)pAllocAddr);
	printf("oep: %x\n", GetOep(pAllocAddr));
	printf("pAllocAddr: %x\n", pAllocAddr);
	printf("oep+pAllocAddr: %x\n", (DWORD)dwOep);
	printf("=================PE Loader=================\n");
	((void(*)())((DWORD)pDosHeader + pOptionHeader->AddressOfEntryPoint))();
	
	system("pause");
	
	return 0;
}