#include<cstdlib>
#include<cstring>
#include<cstdio>
#include<cassert>
#include "rm.h"
#include "rm_internal.h"

RM_FileScan::RM_FileScan  ():scanOpen_(false)
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
  if(!fileHandle.fileOpen_)
    return RM_NOT_OPEN_FILE;
  if(scanOpen_)
    return RM_SCAN_REOPEN;

  rmFileHandle = &fileHandle;
  attrType_ = attrType;
  attrLength_ = attrLength;
  assert(attrLength <= MAXSTRINGLEN);
  attrOffset_ = attrOffset;
  compOp_ = compOp;
  if(value == NULL && compOp != NO_OP)
    return RM_SCAN_NEED_VALUE;

  if(value != NULL) {
    memcpy(buf, value, attrLength);
    switch (attrType) {
      case INT: intVal_ = *(int *)buf; 
//                printf("....scan value %d\n", intVal_);
                break;
      case FLOAT: floatVal_ = *(float *)buf; break;
      case STRING: stringVal_ = string(buf, attrLength_); break;
    }
  }
  curScanId_ = RID(0, 0);
  scanOpen_ = true;

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

template<typename DataType>
bool RM_FileScan::check_scan_data_cond(const DataType attr,const DataType value)
{
  switch(compOp_) {
    case EQ_OP: return (attr == value);
    case LT_OP: return (attr < value);
    case GT_OP: return (attr > value);
    case LE_OP: return (attr <= value);
    case GE_OP: return (attr >= value);
    case NE_OP: return (attr != value);
    case NO_OP: return true;
  }
  return true;
}

bool RM_FileScan::check_scan_cond(char *recData)
{
  if(compOp_ == NO_OP)
    return true;

  int recInt;
  float recFloat;
  string recString;
  switch(attrType_) {
    case INT: recInt = *(int *) &recData[attrOffset_];
//              printf(".... check rec value %d\n", recInt);
              return check_scan_data_cond(recInt, intVal_);
    case FLOAT: recFloat = *(float *) &recData[attrOffset_];
              return check_scan_data_cond(recFloat, floatVal_);
    case STRING: recString = string(recData + attrOffset_, attrLength_);
              return check_scan_data_cond(recString, stringVal_);
  }
  return true;
}

// Get next matching record
RC RM_FileScan::GetNextRec(RM_Record &rec)               
{
  if(!scanOpen_)
    return RM_SCAN_NOT_OPEN;
  
  if(!rmFileHandle->fileOpen_) {
    this->CloseScan();
    return RM_NOT_OPEN_FILE;
  }

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
      || slotTaken(data, slotNum) == false) 
      slotNum = nextRecSlot(data->bitmap, rmFileHandle->bitmapSize, slotNum);

    if(slotNum >= rmFileHandle->recordPerPage){
      ++vPage;
//      printf("++++++++ scan to the next page\n");
      slotNum = 0;
      rmFileHandle->pfh_.UnpinPage(pageNum);
      continue;
    }
    while(slotNum < rmFileHandle->recordPerPage
         && check_scan_cond(&data->data[slotNum * recordSize]) == false )
      slotNum = nextRecSlot(data->bitmap, rmFileHandle->bitmapSize, slotNum);

    if(slotNum >= rmFileHandle->recordPerPage){
      ++vPage;
//      printf("++++++++ scan to the next page\n");
      slotNum = 0;
      rmFileHandle->pfh_.UnpinPage(pageNum);
      continue;
    } 
    
    rec.recordSize = rmFileHandle->recordSize;
    if(rec.data)
      free(rec.data);
//    printf("scan page number %d, slotNum %d\n", pageNum, slotNum);
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

  return RM_EOF;
}
RC RM_FileScan::CloseScan ()                             // Close the scan
{
  if(!scanOpen_)
    return RM_SCAN_NOT_OPEN;
  
  scanOpen_ = false;
  return OK_RC;
}
