#pragma once
#include<Windows.h>
#include<stdio.h>


DWORD CopyFileBufferToImageBuffer(PVOID pFileBuffer, PVOID* pImageBuffer);
DWORD CopyImageBufferToNewBuffer(PVOID pImageBuffer, PVOID* pNewBuffer);
void FileBufferToAddShellcode(PVOID pFileBuffer);

void AddNewSection(PVOID pFileBuffer, PDWORD OldBufferSize, PVOID* pNewBuffer);
void ShellAddNewSectionAndData(PVOID pFileBufferShell, PDWORD dwBufferLengthShell, PVOID* pFileNewBufferShell, PVOID pFileBufferSrc, DWORD dwBufferLengthSrc);

void ExpandSection(PVOID pFileBuffer, PDWORD OldBufferSize, PVOID* pNewBuffer);
void printfPE(PVOID pFileBuffer);
void PrintRelocation(PVOID pFileBuffer); //打印重定位表
void FixRelocation(PVOID pFileBuffer, DWORD value);
DWORD FOA_TO_RVA(PVOID FileAddress, DWORD FOA, PDWORD pRVA);
DWORD RVA_TO_FOA(PVOID FileAddress, DWORD RVA, PDWORD pFOA);
void MyReadFile(PVOID* pFileBuffer, PDWORD BufferLenth, const char* szFilePath);
void MyWriteFile(PVOID pNewBuffer, size_t size, char* szFile);
void PrintfImportTable(PVOID pFileBuffer); //打印导入表
void MoveExportTable(PVOID pFileBuffer, PDWORD OldBufferSize, PVOID* pNewBuffer); //移动导出表
void MoveRelocationTable(PVOID pFileBuffer, PDWORD OldBufferSize, PVOID* pNewBuffer); //移动重定位表
void PrintBindImportTable(PVOID pFileBuffer); //打印绑定导入表
void MoveAndInjectImportTable(PVOID pFileBuffer, PDWORD OldBufferSize, PVOID* pNewBuffer); //移动导入表、并且尝试进行注入
void printfResourceTable(PVOID pFileBuffer);

DWORD GetSizeOfImage(PVOID pFileBuffer);
DWORD GetImageBase(PVOID pFileBuffer);
PIMAGE_BASE_RELOCATION GetRelocationTable(PVOID pFileBuffer);
DWORD GetOep(PVOID pFileBuffer);
void ChangesImageBase(PVOID pFileBuffer, DWORD TempImageBase);

