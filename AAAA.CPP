#include "AAAA.h"

void MyReadFile(PVOID* pFileBuffer, PDWORD BufferLenth, const char* szFilePath) {
	FILE* File;
	File = fopen(szFilePath, "rb");

	if (File == NULL) {
		printf("文件句柄打开失败");
		return;
	}


	//读取文件
	fseek(File, 0, SEEK_END);
	*BufferLenth = ftell(File);
	//重新把File指针指向文件的开头
	fseek(File, 0, SEEK_SET);

	//开辟新空间
	*pFileBuffer = (PVOID)malloc(*BufferLenth);

	//内存清零
	memset(*pFileBuffer, 0, *BufferLenth);

	//读取到内存缓冲区
	fread(*pFileBuffer, *BufferLenth, 1, File);// 一次读入*bufferlenth个字节，重复1次

	//关闭文件句柄
	fclose(File);
}

//FOA_TO_RVA:FOA 转换 RVA							
DWORD FOA_TO_RVA(PVOID FileAddress, DWORD FOA, PDWORD pRVA)
{
	int ret = 0;
	int i;

	PIMAGE_DOS_HEADER pDosHeader = (PIMAGE_DOS_HEADER)(FileAddress);
	PIMAGE_FILE_HEADER pFileHeader = (PIMAGE_FILE_HEADER)((DWORD)pDosHeader + pDosHeader->e_lfanew + 4);
	PIMAGE_OPTIONAL_HEADER32 pOptionalHeader = (PIMAGE_OPTIONAL_HEADER32)((DWORD)pFileHeader + sizeof(IMAGE_FILE_HEADER));
	PIMAGE_SECTION_HEADER pSectionGroup = (PIMAGE_SECTION_HEADER)((DWORD)pOptionalHeader + pFileHeader->SizeOfOptionalHeader);

	//RVA在文件头中 或 SectionAlignment 等于 FileAlignment 时RVA等于FOA
	if (FOA < pOptionalHeader->SizeOfHeaders || pOptionalHeader->SectionAlignment == pOptionalHeader->FileAlignment)
	{
		*pRVA = FOA;
		return ret;
	}

	//循环判断FOA在节区中
	for (i = 0; i < pFileHeader->NumberOfSections; i++)
	{
		if (FOA >= pSectionGroup[i].PointerToRawData && FOA < pSectionGroup[i].PointerToRawData + pSectionGroup[i].SizeOfRawData)
		{
			*pRVA = FOA - pSectionGroup[i].PointerToRawData + pSectionGroup[i].VirtualAddress;
			return *pRVA;
		}
	}

	//没有找到地址
	ret = -4;
	printf("func FOA_TO_RVA() Error: %d 地址转换失败！\n", ret);
	return ret;
}

//功能：RVA 转换 FOA
// RVA_TO_FOA(pFileBuffer,pOptionHeader->DataDirectory[5].VirtualAddress,&FOA);
DWORD RVA_TO_FOA(PVOID FileAddress, DWORD RVA, PDWORD pFOA)
{
	int ret = 0;
	int i = 0;
	PIMAGE_DOS_HEADER pDosHeader = (PIMAGE_DOS_HEADER)(FileAddress);
	PIMAGE_FILE_HEADER pFileHeader = (PIMAGE_FILE_HEADER)((DWORD)pDosHeader + pDosHeader->e_lfanew + 4);
	PIMAGE_OPTIONAL_HEADER32 pOptionalHeader = (PIMAGE_OPTIONAL_HEADER32)((DWORD)pFileHeader + sizeof(IMAGE_FILE_HEADER));
	PIMAGE_SECTION_HEADER pSectionGroup = (PIMAGE_SECTION_HEADER)((DWORD)pOptionalHeader + pFileHeader->SizeOfOptionalHeader);


	//RVA在文件头中 或 SectionAlignment(内存对齐) 等于 FileAlignment(文件对齐) 时 RVA等于FOA
	if (RVA < pOptionalHeader->SizeOfHeaders || pOptionalHeader->SectionAlignment == pOptionalHeader->FileAlignment)
	{
		// 37000
		*pFOA = RVA;
		return ret;
	}

	/*
		第一步：指定节.VirtualAddress <= RVA <= 指定节.VirtualAddress + Misc.VirtualSize(当前节内存实际大小)
		第二步：差值 = RVA - 指定节.VirtualAddress
		第三步：FOA = 指定节.PointerToRawData + 差值
	*/

	//循环判断RVA在节区中
	for (i = 0; i < pFileHeader->NumberOfSections; i++)
	{
		// RVA > 当前节在内存中的偏移地址 并且 RVA < 当前节的内存偏移地址+文件偏移地址
		if (RVA >= pSectionGroup[i].VirtualAddress && RVA < pSectionGroup[i].VirtualAddress + pSectionGroup[i].Misc.VirtualSize)
		{
			*pFOA = RVA - pSectionGroup[i].VirtualAddress + pSectionGroup[i].PointerToRawData;
			return ret;
		}
	}

	//没有找到地址
	ret = -4;
	printf("func RVA_TO_FOA() Error: %d 地址转换失败！\n", ret);
	return ret;
}

//功能：添加新节
void AddNewSection(PVOID pFileBuffer, PDWORD OldBufferSize, PVOID* pNewBuffer) {
	PIMAGE_DOS_HEADER pImageDosHeader = NULL;
	PIMAGE_FILE_HEADER pImageFileHeader = NULL;
	PIMAGE_OPTIONAL_HEADER32 pImageOptionalHeader = NULL;
	PIMAGE_SECTION_HEADER pImageSectionHeaderGroup = NULL;
	PIMAGE_SECTION_HEADER NewSec = NULL;

	DWORD isOk;
	DWORD NewLength = 0;
	PVOID LastSection = NULL;
	PVOID CodeSection = NULL;
	PVOID AddressOfSectionTable = NULL;


	pImageDosHeader = (PIMAGE_DOS_HEADER)pFileBuffer;
	pImageFileHeader = (PIMAGE_FILE_HEADER)((DWORD)pImageDosHeader + pImageDosHeader->e_lfanew + 4);
	pImageOptionalHeader = (PIMAGE_OPTIONAL_HEADER32)((DWORD)pImageFileHeader + sizeof(IMAGE_FILE_HEADER));
	pImageSectionHeaderGroup = (PIMAGE_SECTION_HEADER)((DWORD)pImageOptionalHeader + pImageFileHeader->SizeOfOptionalHeader);

	//判断是否可以容纳相应的节表
	isOk = (DWORD)pImageOptionalHeader->SizeOfHeaders - ((DWORD)pImageDosHeader->e_lfanew + IMAGE_SIZEOF_FILE_HEADER + pImageFileHeader->SizeOfOptionalHeader + 40 * pImageFileHeader->NumberOfSections);
	if (isOk < 80) {
		printf("空间太小 无法进行添加!");
		return;
	}

	//生成对应的内存大小的空间
	NewLength += *OldBufferSize + 0x1000;
	*pNewBuffer = (PVOID)malloc(NewLength);
	ZeroMemory(*pNewBuffer, NewLength);

	//拷贝之前内存空间 到 当前新生成的内存空间
	memcpy(*pNewBuffer, pFileBuffer, *OldBufferSize);

	//获取新的结构体
	pImageDosHeader = (PIMAGE_DOS_HEADER)(*pNewBuffer);
	pImageFileHeader = (PIMAGE_FILE_HEADER)((DWORD)pImageDosHeader + pImageDosHeader->e_lfanew + 4);
	pImageOptionalHeader = (PIMAGE_OPTIONAL_HEADER32)((DWORD)pImageFileHeader + sizeof(IMAGE_FILE_HEADER));
	pImageSectionHeaderGroup = (PIMAGE_SECTION_HEADER)((DWORD)pImageOptionalHeader + pImageFileHeader->SizeOfOptionalHeader);

	// pImageFileHeader->NumberOfSections修改
	pImageFileHeader->NumberOfSections = pImageFileHeader->NumberOfSections + 1;

	// pImageOptionalHeader->SizeOfImage修改
	pImageOptionalHeader->SizeOfImage = (DWORD)pImageOptionalHeader->SizeOfImage + 0x1000;

	// 复制代码段的节数据到 当前最后一个节数据后面
	CodeSection = (PVOID)(&pImageSectionHeaderGroup[0]);

	LastSection = (PVOID)(DWORD)(&pImageSectionHeaderGroup[pImageFileHeader->NumberOfSections - 1]);
	memcpy(LastSection, CodeSection, 40);

	//修正相关属性
	NewSec = (PIMAGE_SECTION_HEADER)LastSection;
	strcpy((PCHAR)NewSec, ".NewSec");
	NewSec->Misc.VirtualSize = 0x1000;
	NewSec->SizeOfRawData = 0x1000;
	NewSec->VirtualAddress = pImageSectionHeaderGroup[pImageFileHeader->NumberOfSections - 2].VirtualAddress + pImageSectionHeaderGroup[pImageFileHeader->NumberOfSections - 2].SizeOfRawData;
	NewSec->PointerToRawData = pImageSectionHeaderGroup[pImageFileHeader->NumberOfSections - 2].PointerToRawData + pImageSectionHeaderGroup[pImageFileHeader->NumberOfSections - 2].SizeOfRawData;

	//修改大小长度
	*OldBufferSize = NewLength;

	//
	AddressOfSectionTable = (PVOID)((DWORD)*pNewBuffer + (DWORD)NewSec->PointerToRawData);
}


void ShellAddNewSectionAndData(PVOID pFileBufferShell, PDWORD dwBufferLengthShell, PVOID* pFileNewBufferShell, PVOID pFileBufferSrc, DWORD dwBufferLengthSrc) {
	PIMAGE_DOS_HEADER pImageDosHeader = NULL;
	PIMAGE_FILE_HEADER pImageFileHeader = NULL;
	PIMAGE_OPTIONAL_HEADER32 pImageOptionalHeader = NULL;
	PIMAGE_SECTION_HEADER pImageSectionHeaderGroup = NULL;
	PIMAGE_SECTION_HEADER NewSec = NULL;

	DWORD isOk;
	DWORD NewLength = 0;
	PVOID LastSection = NULL;
	PVOID CodeSection = NULL;
	PVOID AddressOfSectionTable = NULL;


	pImageDosHeader = (PIMAGE_DOS_HEADER)pFileBufferShell;
	pImageFileHeader = (PIMAGE_FILE_HEADER)((DWORD)pImageDosHeader + pImageDosHeader->e_lfanew + 4);
	pImageOptionalHeader = (PIMAGE_OPTIONAL_HEADER32)((DWORD)pImageFileHeader + sizeof(IMAGE_FILE_HEADER));
	pImageSectionHeaderGroup = (PIMAGE_SECTION_HEADER)((DWORD)pImageOptionalHeader + pImageFileHeader->SizeOfOptionalHeader);

	//判断是否可以容纳相应的节表
	isOk = (DWORD)pImageOptionalHeader->SizeOfHeaders - ((DWORD)pImageDosHeader->e_lfanew + IMAGE_SIZEOF_FILE_HEADER + pImageFileHeader->SizeOfOptionalHeader + 40 * pImageFileHeader->NumberOfSections);
	if (isOk < 80) {
		printf("空间太小 无法进行添加!");
		return;
	}

	//生成对应的内存大小的空间
	NewLength += *dwBufferLengthShell + dwBufferLengthSrc;
	*pFileNewBufferShell = (PVOID)malloc(NewLength);
	ZeroMemory(*pFileNewBufferShell, NewLength);

	//拷贝之前内存空间 到 当前新生成的内存空间
	memcpy(*pFileNewBufferShell, pFileBufferShell, *dwBufferLengthShell);

	//获取新的结构体
	pImageDosHeader = (PIMAGE_DOS_HEADER)(*pFileNewBufferShell);
	pImageFileHeader = (PIMAGE_FILE_HEADER)((DWORD)pImageDosHeader + pImageDosHeader->e_lfanew + 4);
	pImageOptionalHeader = (PIMAGE_OPTIONAL_HEADER32)((DWORD)pImageFileHeader + sizeof(IMAGE_FILE_HEADER));
	pImageSectionHeaderGroup = (PIMAGE_SECTION_HEADER)((DWORD)pImageOptionalHeader + pImageFileHeader->SizeOfOptionalHeader);

	// pImageFileHeader->NumberOfSections修改
	pImageFileHeader->NumberOfSections = pImageFileHeader->NumberOfSections + 1;

	// pImageOptionalHeader->SizeOfImage修改
	pImageOptionalHeader->SizeOfImage = (DWORD)pImageOptionalHeader->SizeOfImage + dwBufferLengthSrc;

	// 复制代码段的节数据到 当前最后一个节数据后面
	CodeSection = (PVOID)(&pImageSectionHeaderGroup[0]);

	LastSection = (PVOID)(DWORD)(&pImageSectionHeaderGroup[pImageFileHeader->NumberOfSections - 1]);
	memcpy(LastSection, CodeSection, 40);

	//修正相关属性
	NewSec = (PIMAGE_SECTION_HEADER)LastSection;
	strcpy((PCHAR)NewSec, ".NewSec");
	NewSec->Misc.VirtualSize = dwBufferLengthSrc;
	NewSec->SizeOfRawData = dwBufferLengthSrc;
	NewSec->VirtualAddress = pImageSectionHeaderGroup[pImageFileHeader->NumberOfSections - 2].VirtualAddress + pImageSectionHeaderGroup[pImageFileHeader->NumberOfSections - 2].SizeOfRawData;
	NewSec->PointerToRawData = pImageSectionHeaderGroup[pImageFileHeader->NumberOfSections - 2].PointerToRawData + pImageSectionHeaderGroup[pImageFileHeader->NumberOfSections - 2].SizeOfRawData;
	NewSec->Characteristics = 0xC0000040;

	//修改大小长度	
	*dwBufferLengthShell = NewLength;

	AddressOfSectionTable = (PVOID)((DWORD)*pFileNewBufferShell + (DWORD)NewSec->PointerToRawData);

	memcpy(AddressOfSectionTable, pFileBufferSrc, dwBufferLengthSrc);


}

//**************************************************************************								
//CopyFileBufferToImageBuffer:将文件从FileBuffer复制到ImageBuffer								
//参数说明：								
//pFileBuffer  FileBuffer指针								
//pImageBuffer ImageBuffer指针								
//返回值说明：								
//读取失败返回0  否则返回复制的大小								

DWORD CopyFileBufferToImageBuffer(PVOID pFileBuffer, PVOID* pImageBuffer) {
	PIMAGE_DOS_HEADER pImageDosHeader = NULL;
	PIMAGE_NT_HEADERS pImageNtHeader = NULL;
	PIMAGE_FILE_HEADER pImageFileHeader = NULL;
	PIMAGE_OPTIONAL_HEADER32 pImageOptionalHeader = NULL;
	PIMAGE_SECTION_HEADER pImageSectionHeaderGroup = NULL;
	DWORD ImageBufferSize = 0;
	int i = 0;

	// DOS头
	pImageDosHeader = (PIMAGE_DOS_HEADER)pFileBuffer;

	// 标准PE
	pImageFileHeader = (PIMAGE_FILE_HEADER)((DWORD)pImageDosHeader + pImageDosHeader->e_lfanew + 4);

	// 可选PE
	pImageOptionalHeader = (PIMAGE_OPTIONAL_HEADER32)((DWORD)pImageFileHeader + IMAGE_SIZEOF_FILE_HEADER);

	//节表组
	pImageSectionHeaderGroup = (PIMAGE_SECTION_HEADER)((DWORD)pImageOptionalHeader + pImageFileHeader->SizeOfOptionalHeader);

	//获取ImageBufffer的内存大小
	ImageBufferSize = pImageOptionalHeader->SizeOfImage;

	//为pImageBuffer分配内存空间
	*pImageBuffer = (PVOID)malloc(ImageBufferSize);

	if (*pImageBuffer == NULL)
	{
		printf("malloc failed");
		return -1;
	}

	//清零
	memset(*pImageBuffer, 0, ImageBufferSize);

	// 拷贝头+节表
	memcpy(*pImageBuffer, pFileBuffer, pImageOptionalHeader->SizeOfHeaders);


	//循环拷贝节表
	for (i = 0; i < pImageFileHeader->NumberOfSections; i++) {
		memcpy(
			(PVOID)((DWORD)*pImageBuffer + pImageSectionHeaderGroup[i].VirtualAddress), // 要拷贝的位置 ImageBuffer中的每个节数据的偏移位置
			(PVOID)((DWORD)pFileBuffer + pImageSectionHeaderGroup[i].PointerToRawData), // 被拷贝的位置是 Filebuffer中的每个节数据的偏移位置
			pImageSectionHeaderGroup[i].SizeOfRawData // 被拷贝的大小为 每个节数据的文件对齐大小
		);
	}
	free(pFileBuffer);
	return 0;
}

//功能：保存文件 
void MyWriteFile(PVOID pNewBuffer, size_t size, char* szFile) {

	FILE* File;
	File = fopen(szFile, "wb");

	if (File == NULL) {
		printf("文件句柄打开失败");
		return;
	}
	fwrite(pNewBuffer, size, 1, File);
	fclose(File);
	free(pNewBuffer);
}


DWORD GetSizeOfImage(PVOID pImageBuffer) {
	PIMAGE_DOS_HEADER pImageDosHeader = NULL;
	PIMAGE_FILE_HEADER pImageFileHeader = NULL;
	PIMAGE_OPTIONAL_HEADER32 pImageOptionalHeader = NULL;

	pImageDosHeader = (PIMAGE_DOS_HEADER)pImageBuffer;
	pImageFileHeader = (PIMAGE_FILE_HEADER)((DWORD)pImageDosHeader + pImageDosHeader->e_lfanew + 4);
	pImageOptionalHeader = (PIMAGE_OPTIONAL_HEADER32)((DWORD)pImageFileHeader + sizeof(IMAGE_FILE_HEADER));

	return pImageOptionalHeader->SizeOfImage;
}


DWORD GetImageBase(PVOID pImageBuffer) {
	PIMAGE_DOS_HEADER pImageDosHeader = NULL;
	PIMAGE_FILE_HEADER pImageFileHeader = NULL;
	PIMAGE_OPTIONAL_HEADER32 pImageOptionalHeader = NULL;

	pImageDosHeader = (PIMAGE_DOS_HEADER)pImageBuffer;
	pImageFileHeader = (PIMAGE_FILE_HEADER)((DWORD)pImageDosHeader + pImageDosHeader->e_lfanew + 4);
	pImageOptionalHeader = (PIMAGE_OPTIONAL_HEADER32)((DWORD)pImageFileHeader + sizeof(IMAGE_FILE_HEADER));

	return pImageOptionalHeader->ImageBase;
}

PIMAGE_BASE_RELOCATION GetRelocationTable(PVOID pFileBuffer) {
	PIMAGE_DOS_HEADER pImageDosHeader = NULL;
	PIMAGE_FILE_HEADER pImageFileHeader = NULL;
	PIMAGE_OPTIONAL_HEADER32 pImageOptionalHeader = NULL;
	PIMAGE_SECTION_HEADER pImageSectionHeaderGroup = NULL;
	PIMAGE_SECTION_HEADER NewSec = NULL;
	PIMAGE_BASE_RELOCATION pRelocationDirectory;

	DWORD NewLength = 0;
	PVOID LastSection = NULL;
	PVOID CodeSection = NULL;
	PVOID AddressOfSectionTable = NULL;
	DWORD FOA;

	pImageDosHeader = (PIMAGE_DOS_HEADER)pFileBuffer;
	pImageFileHeader = (PIMAGE_FILE_HEADER)((DWORD)pImageDosHeader + pImageDosHeader->e_lfanew + 4);
	pImageOptionalHeader = (PIMAGE_OPTIONAL_HEADER32)((DWORD)pImageFileHeader + sizeof(IMAGE_FILE_HEADER));
	pImageSectionHeaderGroup = (PIMAGE_SECTION_HEADER)((DWORD)pImageOptionalHeader + pImageFileHeader->SizeOfOptionalHeader);


	RVA_TO_FOA(pFileBuffer, pImageOptionalHeader->DataDirectory[5].VirtualAddress, &FOA);
	pRelocationDirectory = (PIMAGE_BASE_RELOCATION)((DWORD)pFileBuffer + FOA);

	return pRelocationDirectory;
}


DWORD GetOep(PVOID pFileBuffer) {
	PIMAGE_DOS_HEADER pImageDosHeader = NULL;
	PIMAGE_FILE_HEADER pImageFileHeader = NULL;
	PIMAGE_OPTIONAL_HEADER32 pImageOptionalHeader = NULL;
	PIMAGE_SECTION_HEADER pImageSectionHeaderGroup = NULL;
	PIMAGE_SECTION_HEADER NewSec = NULL;

	pImageDosHeader = (PIMAGE_DOS_HEADER)pFileBuffer;
	pImageFileHeader = (PIMAGE_FILE_HEADER)((DWORD)pImageDosHeader + pImageDosHeader->e_lfanew + 4);
	pImageOptionalHeader = (PIMAGE_OPTIONAL_HEADER32)((DWORD)pImageFileHeader + sizeof(IMAGE_FILE_HEADER));
	pImageSectionHeaderGroup = (PIMAGE_SECTION_HEADER)((DWORD)pImageOptionalHeader + pImageFileHeader->SizeOfOptionalHeader);

	return pImageOptionalHeader->AddressOfEntryPoint;
}

void ChangesImageBase(PVOID pFileBuffer, DWORD TempImageBase) {
	PIMAGE_DOS_HEADER pImageDosHeader = NULL;
	PIMAGE_FILE_HEADER pImageFileHeader = NULL;
	PIMAGE_OPTIONAL_HEADER32 pImageOptionalHeader = NULL;
	PIMAGE_SECTION_HEADER pImageSectionHeaderGroup = NULL;
	PIMAGE_SECTION_HEADER NewSec = NULL;

	pImageDosHeader = (PIMAGE_DOS_HEADER)pFileBuffer;
	pImageFileHeader = (PIMAGE_FILE_HEADER)((DWORD)pImageDosHeader + pImageDosHeader->e_lfanew + 4);
	pImageOptionalHeader = (PIMAGE_OPTIONAL_HEADER32)((DWORD)pImageFileHeader + sizeof(IMAGE_FILE_HEADER));
	pImageSectionHeaderGroup = (PIMAGE_SECTION_HEADER)((DWORD)pImageOptionalHeader + pImageFileHeader->SizeOfOptionalHeader);

	pImageOptionalHeader->ImageBase = TempImageBase;
}

void FixRelocation(PVOID pFileBuffer, DWORD dwFixValue) {
	PIMAGE_DOS_HEADER pDosHeader = NULL;
	PIMAGE_NT_HEADERS pNTHeader = NULL;
	PIMAGE_FILE_HEADER pPEHeader = NULL;
	PIMAGE_OPTIONAL_HEADER32 pOptionHeader = NULL;
	PIMAGE_SECTION_HEADER pSectionHeader = NULL;
	PIMAGE_BASE_RELOCATION pRelocationDirectory = NULL;
	DWORD NumberOfRelocation = 0;
	PWORD Location = NULL;
	WORD reloData;
	DWORD i;

	pDosHeader = (PIMAGE_DOS_HEADER)pFileBuffer;
	pNTHeader = (PIMAGE_NT_HEADERS)((DWORD)pFileBuffer + pDosHeader->e_lfanew);
	pPEHeader = (PIMAGE_FILE_HEADER)(((DWORD)pNTHeader) + 4);
	pOptionHeader = (PIMAGE_OPTIONAL_HEADER32)((DWORD)pPEHeader + IMAGE_SIZEOF_FILE_HEADER);
	pSectionHeader = (PIMAGE_SECTION_HEADER)((DWORD)pOptionHeader + sizeof(IMAGE_OPTIONAL_HEADER));

	//定位第一张重定位表 文件中的地址
	pRelocationDirectory = (PIMAGE_BASE_RELOCATION)((DWORD)pFileBuffer + pOptionHeader->DataDirectory[5].VirtualAddress); 

	while (pRelocationDirectory->SizeOfBlock && pRelocationDirectory->VirtualAddress) {
		NumberOfRelocation = (pRelocationDirectory->SizeOfBlock - 8) / 2;// 每个重定位块中的数据项的数量
		Location = (PWORD)((DWORD)pRelocationDirectory + 8); // 加上8个字节
		for (i = 0; i < NumberOfRelocation; i++) {
			if (Location[i] >> 12 != 0) { //判断是否是垃圾数据
				// WORD类型的变量进行接收
				reloData = (Location[i] & 0xFFF); //这里进行与操作 只取后后面12位的数字
				*(PDWORD)((DWORD)pFileBuffer + (DWORD)pRelocationDirectory->VirtualAddress + reloData) =
					*(PDWORD)((DWORD)pFileBuffer + (DWORD)pRelocationDirectory->VirtualAddress + reloData) + dwFixValue;
			}
		}

		//上面的for循环完成之后，跳转到下个重定位块 继续如上的操作
		pRelocationDirectory = (PIMAGE_BASE_RELOCATION)((DWORD)pRelocationDirectory + (DWORD)pRelocationDirectory->SizeOfBlock); 
	}
}