#include<cstdlib>
#include<cstdio>
#include<cstring>
#include<cassert>
#include "rm.h"
#include "rm_internal.h"

RM_Manager::RM_Manager (PF_Manager &pfm): pfm_(pfm)
{
  openFile_.clear();
}

RM_Manager::~RM_Manager()
{
}

RC RM_Manager::CreateFile (const char *fileName, int recordSize)
{
  if(recordSize > int(DATA_ON_RECORD_PAGE))
    return RM_CREATE_FILE_RECORD_SIZE;
  RC r = pfm_.CreateFile(fileName);
  if(r)
    return r;

  //write the file header
  PF_FileHandle fileHandle;
  r = pfm_.OpenFile(fileName, fileHandle); 
  if(r) {
    DestroyFile(fileName);
    return r;
  }

  PF_PageHandle filePage;
  r = fileHandle.AllocatePage(filePage);
  if(r) {
    pfm_.CloseFile(fileHandle);
    DestroyFile(fileName);
    return r;
  }

  char * page;
  int pageNum;
  if( filePage.GetData(page) || filePage.GetPageNum(pageNum) ) {
    pfm_.CloseFile(fileHandle);
    DestroyFile(fileName);
    return RM_CREATE_FILE_HDR_PAGE_WRITE_ERROR;
  }

  struct RM_FileHeaderPage hdr;
  memset(&hdr, 0, sizeof(RM_FileHeaderPage));
  if(recordSize < MIN_RECORD_SIZE)
    recordSize = MIN_RECORD_SIZE;
  hdr.recordSize = recordSize;
  hdr.nextPageDir = END_PAGE_LIST;
  hdr.nextEmptyPageDir = END_PAGE_LIST;

  memcpy(page, (const char *)&hdr, PF_PAGE_SIZE);
  
  fileHandle.MarkDirty(pageNum);
  fileHandle.UnpinPage(pageNum);
  pfm_.CloseFile(fileHandle);
  return r;
}

RC RM_Manager::DestroyFile(const char *fileName)
{
  if(openFile_.find(string(fileName)) != openFile_.end() &&
    openFile_[string(fileName)] > 0 )
    return RM_DESTROY_FILE_WHILE_OPEN;
  RC r = pfm_.DestroyFile(fileName);
  return r;
}

RC RM_Manager::OpenFile   (const char *fileName, RM_FileHandle &fileHandle)
{
  if(fileHandle.fileOpen_)
    return RM_OPEN_FILE_W_OPEN_HANDLE;
  PF_FileHandle pfh;
  RC r = pfm_.OpenFile(fileName, pfh);
  if(r)
    return r;

  if(openFile_.find(string(fileName)) == openFile_.end())
    openFile_.insert(pair<string, int>(string(fileName), 1));
  else
    openFile_[string(fileName)] += 1;

  
  fileHandle.fileOpen_ = true;
  fileHandle.pfh_ = pfh;
  fileHandle.fileName_ = string(fileName);
  PF_PageHandle pfp;
  struct RM_FileHeaderPage * data;
  int pageNum;
  if( pfh.GetFirstPage(pfp) || pfp.GetData((char * &) data) 
      || pfp.GetPageNum(pageNum) ) {
    openFile_[string(fileName)] -= 1;
    pfm_.CloseFile(pfh);
    return RM_OPEN_FILE_HDR_PAGE_ERROR;
  }
  fileHandle.recordSize = data->recordSize;
  fileHandle.recordPerPage = (PF_PAGE_SIZE - MAX_BITMAP_SIZE)/data->recordSize;
  printf("++ recordPerPage %d\n", fileHandle.recordPerPage);
  fileHandle.bitmapSize = fileHandle.recordPerPage/8;
  if(fileHandle.recordPerPage & 7)
    fileHandle.bitmapSize += 1;
  fileHandle.totalPage = data->totalPage;
  fileHandle.totalEmptyPage = data->totalEmptyPage;

  // read in the total page list and total empty page list
  install_page_list(pfh, fileHandle, (void *)data, false); //total page
  install_page_list(pfh, fileHandle, (void *)data, true); //empty page
  fileHandle.headerUpdate = false;
    
  // Unpin the header Page
  pfh.UnpinPage(pageNum);
  return OK_RC;
}

//setting up empty page or total page
int RM_Manager::install_page_list(const PF_FileHandle &pfh, 
            RM_FileHandle &fileHandle, void *hdr, bool emptyPage)
{
  struct RM_FileHeaderPage * data = (struct RM_FileHeaderPage *)hdr;
  if(emptyPage)
    fileHandle.emptyPageList.clear();
  else
    fileHandle.totalPageList.clear();

  if(emptyPage)
    for(int i = 0; i < data->emptyPageOnThis; ++i)
      fileHandle.emptyPageList.push_back(data->emptyPageList[i]);
  else
    for(int i = 0; i < data->pageOnThis; ++i)
      fileHandle.totalPageList.push_back(data->totalPageList[i]);

//  printf("Look Deeper\n");

  int nextPageDir; 
  if(emptyPage)
    nextPageDir = data->nextEmptyPageDir;
  else
    nextPageDir = data->nextPageDir;
//  printf("nextPageDir %d\n", nextPageDir);
  PF_PageHandle pageDir;
  while(nextPageDir != -1) {
    int thisPageDir = nextPageDir;
    pfh.GetThisPage(thisPageDir, pageDir);

    struct RM_FilePageDirPage * pageDirData;
    pageDir.GetData((char * &) pageDirData);

    if(emptyPage)
      for(int i = 0; i < pageDirData->pageListSize; ++i)
        fileHandle.emptyPageList.push_back(pageDirData->data[i]);
    else
      for(int i=0; i < pageDirData->pageListSize; ++i)
        fileHandle.totalPageList.push_back(pageDirData->data[i]);

    nextPageDir = pageDirData->nextPageDir;
    pfh.UnpinPage(thisPageDir);
  }
  return OK_RC;
}

RC RM_Manager::write_back_total_page(RM_FileHandle & fileHandle, 
                                    void * fileHdrP, bool emptyPage)
{
  struct RM_FileHeaderPage & fileHdr = *((struct RM_FileHeaderPage *)fileHdrP);

  const vector<PageNum> & totalPageList = fileHandle.totalPageList;
  const list<PageNum> & emptyPageList = fileHandle.emptyPageList;
  list<PageNum>::const_iterator it;

  PF_PageHandle thisOverFlow, nextOverFlow;
  PageNum thisOverFlowNum, nextOverFlowNum;

  assert(size_t(fileHandle.totalPage) == totalPageList.size());
  if(emptyPage) {
    if(fileHandle.totalEmptyPage <= int(PAGE_DIR_LIST_SIZE)) {
      fileHdr.emptyPageOnThis = fileHandle.totalEmptyPage;
      fileHdr.nextEmptyPageDir = END_PAGE_LIST;
    } else {
      fileHdr.emptyPageOnThis = HEADER_LIST_HALF;
      fileHandle.pfh_.AllocatePage(thisOverFlow);
      thisOverFlow.GetPageNum(thisOverFlowNum);
      fileHdr.nextEmptyPageDir =   thisOverFlowNum;
    }
  } else {
    if(fileHandle.totalPage <= int(PAGE_DIR_LIST_SIZE)) {
      fileHdr.pageOnThis = fileHandle.totalPage;
      fileHdr.nextPageDir = END_PAGE_LIST;
    } else {
      fileHdr.pageOnThis = HEADER_LIST_HALF;
      fileHandle.pfh_.AllocatePage(thisOverFlow);
      thisOverFlow.GetPageNum(thisOverFlowNum);
      fileHdr.nextPageDir =   thisOverFlowNum;
    }
  }

  if(emptyPage){
    it = emptyPageList.begin();
    for(int i=0; i<fileHdr.emptyPageOnThis; ++i, ++it)
      fileHdr.emptyPageList[i] = *it;
  } else {
    for(int i=0; i<fileHdr.pageOnThis; ++i)
      fileHdr.totalPageList[i] = totalPageList[i];
  }

  int leftOver; 
  if(emptyPage)
    leftOver = fileHdr.totalEmptyPage - fileHdr.emptyPageOnThis;
  else
    leftOver = fileHdr.totalPage - fileHdr.pageOnThis;

  int beginIdx = fileHdr.pageOnThis;
  while(leftOver) {
    struct RM_FilePageDirPage * data;
    thisOverFlow.GetData((char *&) data);
    data->pageListSize = leftOver <= int(PAGE_DIR_LIST_SIZE) ? 
                         leftOver : int(PAGE_DIR_LIST_SIZE);
    if(emptyPage){
      for(int i = 0; i < data->pageListSize; ++i, ++it )
        data->data[i] = *it;
    } else {
      for(int i = 0; i < data->pageListSize; ++i )
        data->data[i] = totalPageList[beginIdx + i];
    }
    leftOver -= data->pageListSize;
    beginIdx += data->pageListSize;
    if(!leftOver) {
      data->nextPageDir = END_PAGE_LIST;
    } else {
      fileHandle.pfh_.AllocatePage(nextOverFlow);
      nextOverFlow.GetPageNum(nextOverFlowNum);
      data->nextPageDir = nextOverFlowNum;
    }
    fileHandle.pfh_.MarkDirty(thisOverFlowNum);
    fileHandle.pfh_.UnpinPage(thisOverFlowNum);
    thisOverFlowNum = nextOverFlowNum;
    thisOverFlow = nextOverFlow; 
  }
  return OK_RC;
}

RC RM_Manager::CloseFile (RM_FileHandle &fileHandle)
{
  if(!fileHandle.fileOpen_)
    return RM_CLOSE_FILE_W_CLOSED_HANDLE;

  fileHandle.fileOpen_ = false;
  if(openFile_.find(fileHandle.fileName_) != openFile_.end()) {
    if(openFile_[fileHandle.fileName_] <= 1)
      openFile_.erase(fileHandle.fileName_);
    else
      openFile_[fileHandle.fileName_] -= 1;
  }

  if(fileHandle.headerUpdate) {
    PF_PageHandle hdrPage; 
    fileHandle.pfh_.GetFirstPage(hdrPage);
    struct RM_FileHeaderPage fileHdr;
    fileHdr.recordSize = fileHandle.recordSize;
    fileHdr.totalPage = fileHandle.totalPage;
    fileHdr.totalEmptyPage = fileHandle.totalEmptyPage;
 
    //write total page list
    write_back_total_page(fileHandle, &fileHdr, false);
    //write empty page list
    write_back_total_page(fileHandle, &fileHdr, true); 

    char * data;
    hdrPage.GetData(data);
    memcpy(data, (const char *)&fileHdr, PF_PAGE_SIZE);

    PageNum pageNum;
    hdrPage.GetPageNum(pageNum);
    fileHandle.pfh_.MarkDirty(pageNum);
    fileHandle.pfh_.UnpinPage(pageNum);
  }

  pfm_.CloseFile(fileHandle.pfh_);
  return OK_RC;
}

