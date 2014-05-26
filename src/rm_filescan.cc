#include<cstdlib>
#include<cstring>
#include<cstdio>
#include "rm.h"
#include "rm_internal.h"

RM_FileScan::RM_FileScan  ()
{
}

RM_FileScan::~RM_FileScan ()
{
}

RC RM_FileScan::OpenScan  (const RM_FileHandle &fileHandle,
                  AttrType   attrType,
                  int        attrLength,
                  int        attrOffset,
                  CompOp     compOp,
                  void       *value,
                  ClientHint pinHint) // Initialize a file scan
{
  rmFileHandle = &fileHandle;
  attrType_ = attrType;
  attrLength_ = attrLength;
  attrOffset_ = attrOffset;
  compOp_ = compOp_;
  value_ = NULL;
  RID curScanId_ = RID(0, 0);
  return OK_RC;
}

SlotNum RM_FileScan::nextRecSlot(const unsigned char *bitmap, int bitmapSize, 
                                SlotNum start)
{
  int i = start >> 3;
  int j = start & 7;

  for(j = j + 1; j < 8; ++j)
    if((bitmap[i] >> j) & 1)
      return i * 8 + j;

  for(i = i + 1; i <bitmapSize; ++i)
    if(bitmap[i])
      break;
  for(j=0; j<8; ++j)
    if((bitmap[i] >> j ) & 1)
      break;
  return i * 8 + j;
}

inline bool slotTaken(const unsigned char *bitmap, SlotNum slotNum){
  int i = slotNum / 8;
  int j = slotNum & 7;
  if((bitmap[i] >> j) & 1) 
    return true;
  else
    return false;
}

RC RM_FileScan::GetNextRec(RM_Record &rec)               // Get next matching record
{
  PageNum vPage, pageNum;
  SlotNum slotNum;
  curScanId_.GetPageNum(vPage);
  curScanId_.GetSlotNum(slotNum);
  int recordSize = rmFileHandle->recordSize;
  
  while(vPage < rmFileHandle->totalPage) {
    pageNum = rmFileHandle->totalPageList[vPage];
    PF_PageHandle pageHandle;
    rmFileHandle->pfh_.GetThisPage(pageNum, pageHandle);
    struct RM_FileRecPage * data;
    pageHandle.GetData((char * &)data);
    
    if(slotNum >= rmFileHandle->recordPerPage 
      || slotTaken(data->bitmap, slotNum) == false) 
      slotNum = nextRecSlot(data->bitmap, rmFileHandle->bitmapSize, slotNum);

    if(slotNum >= rmFileHandle->recordPerPage){
      ++vPage;
      rmFileHandle->pfh_.UnpinPage(pageNum);
      continue;
    } else {
      rec.recordSize = rmFileHandle->recordSize;
      if(rec.data)
        free(rec.data);
      printf("page number %d, slotNum %d\n", pageNum, slotNum);
      rec.data = (char *)malloc(sizeof(char)*recordSize);
      memcpy(rec.data, &(data->data[slotNum * recordSize]), recordSize);
      rec.rid_ = RID(vPage, slotNum);

      slotNum = nextRecSlot(data->bitmap, rmFileHandle->bitmapSize, slotNum);
      if(slotNum >= rmFileHandle->recordPerPage)
        curScanId_ = RID(vPage+1, 0);
      else
        curScanId_ = RID(vPage, slotNum);

      rmFileHandle->pfh_.UnpinPage(pageNum);

      return OK_RC;
    }
  }

  return RM_EOF;
}
RC RM_FileScan::CloseScan ()                             // Close the scan
{
  return OK_RC;
}
