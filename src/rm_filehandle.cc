#include <cstdlib>
#include <cstdio>
#include "rm.h"
#include "rm_internal.h"
#include <cstring>
#include <cassert>
#include <iostream>
using namespace std;

RM_FileHandle::RM_FileHandle  ()
{
  fileOpen_ = false;
}

RM_FileHandle::~RM_FileHandle  ()
{
  totalPageList.clear();
  emptyPageList.clear();
}

RC RM_FileHandle::check_record_exist(const RID & rid, PageNum &pageNum,
                  SlotNum &slotNum, PageNum & actualPageNum, char *&data) const
{
  rid.GetPageNum(pageNum);
  rid.GetSlotNum(slotNum);
  if(pageNum < 0 || slotNum < 0 || pageNum >= totalPage
    || slotNum >= recordPerPage ) {
    return RM_REC_NO_EXIST;
  }
  actualPageNum = totalPageList[pageNum];
  PF_PageHandle pageHdl;
  pfh_.GetThisPage(actualPageNum, pageHdl);
  pageHdl.GetData((char * &) data);

  if(!slotTaken((struct RM_FileRecPage *)data, slotNum)) {
    pfh_.UnpinPage(actualPageNum);
    return RM_REC_NO_EXIST;
  } else 
    return OK_RC;

}

RC RM_FileHandle::GetRec     (const RID &rid, RM_Record &rec) const
{
  if(!fileOpen_)
    return RM_NOT_OPEN_FILE;
  PageNum pageNum, actualPageNum;
  SlotNum slotNum;

  struct RM_FileRecPage * data;
  if(check_record_exist(rid, pageNum, slotNum, actualPageNum, (char * &)data)
    == RM_REC_NO_EXIST)
    return RM_REC_NO_EXIST;

  if(rec.data)
    free(rec.data);
  rec.data = (char *)malloc(sizeof(char) * recordSize);
  memcpy(rec.data, & data->data[recordSize * slotNum], recordSize);

  pfh_.UnpinPage(actualPageNum);
  return OK_RC;
}

// Insert a new record
RC RM_FileHandle::InsertRec  (const char *pData, RID &rid)       
{
  if(!fileOpen_)
    return RM_NOT_OPEN_FILE;

  PF_PageHandle pageHdl;
  PageNum pageNum; //actual page number
  int pageIdx;

  if(!totalEmptyPage){
    pfh_.AllocatePage(pageHdl);
    pageHdl.GetPageNum(pageNum);
    pageIdx = totalPage++;
    emptyPageList.push_back(pageIdx); //empty list is just idx
    totalEmptyPage = 1;
    totalPageList.push_back(pageNum);
    headerUpdate = true;

    char * data;
    pageHdl.GetData(data);
    memset(data, 0, sizeof(struct RM_FileRecPage));
  } else {
    pageIdx = emptyPageList.front();
    pageNum = totalPageList[pageIdx];
    pfh_.GetThisPage(pageNum, pageHdl);
//    cout << "insert on page with empty id "<< pageIdx << endl;
  }
//  cout << "insert on page no "<< pageNum << endl;
  struct RM_FileRecPage * data;
  pageHdl.GetData((char *&)data);

  SlotNum slotNum;
  assert(findFirstEmptySlot(data, slotNum));
  setEmptySlot(data, slotNum);

//  cout << "slot number "<< slotNum << ", page number "<< pageNum << endl;
  assert(slotNum < recordPerPage);

  SlotNum nextSlotNum;
  if(!findFirstEmptySlot(data, nextSlotNum, slotNum) 
     || nextSlotNum >= recordPerPage){
    emptyPageList.pop_front();
    --totalEmptyPage;
    headerUpdate = true;
  }
 
  memcpy(& data->data[recordSize * slotNum], pData, recordSize);
  
  rid = RID(pageIdx, slotNum);

  pfh_.MarkDirty(pageNum);
  pfh_.UnpinPage(pageNum);

  return OK_RC;
}

// Delete a record
RC RM_FileHandle::DeleteRec  (const RID &rid)                    
{
  if(!fileOpen_)
    return RM_NOT_OPEN_FILE;
  PageNum pageNum, actualPageNum;
  SlotNum slotNum;

  struct RM_FileRecPage * data;

  if(check_record_exist(rid, pageNum, slotNum, actualPageNum, (char * &)data)
    == RM_REC_NO_EXIST)
    return RM_REC_NO_EXIST;

  //find if this is a full page, if so, this page become empty page
  int i, j;
  for(i = 0; i<bitmapSize; ++i) 
    if(data->bitmap[i] != 0xff)
      break;  
  for(j = 0; j<8; ++j)
    if( ((data->bitmap[i] >> j) & 1) == 0)
      break;
  SlotNum emptySlotNum = i * 8 + j;

  i = slotNum / 8;
  j = slotNum & 7;
  data->bitmap[i] ^= 1 << j; //change the jth bit

  if(emptySlotNum >= recordPerPage) {// this is a full page
    emptyPageList.push_back(pageNum); //virtual page
    ++totalEmptyPage;
    headerUpdate = true;
  }
  pfh_.MarkDirty(actualPageNum);
  pfh_.UnpinPage(actualPageNum);

  return OK_RC;
}

// Update a record
RC RM_FileHandle::UpdateRec  (const RM_Record &rec)              
{
  if(!fileOpen_)
    return RM_NOT_OPEN_FILE;
  if(rec.recordSize != recordSize)
    return RM_REC_LEN_NO_MATCH;

  PageNum pageNum, actualPageNum;
  SlotNum slotNum;

  struct RM_FileRecPage * data;

  if(check_record_exist(rec.rid_, pageNum, slotNum, actualPageNum, 
    (char * &)data) == RM_REC_NO_EXIST)
    return RM_REC_NO_EXIST;

  memcpy(&data->data[slotNum * recordSize], rec.data, recordSize);

  pfh_.MarkDirty(actualPageNum);
  pfh_.UnpinPage(actualPageNum);

  return OK_RC;
}

    // Forces a page (along with any contents stored in this class)
    // from the buffer pool to disk.  Default value forces all pages.
RC RM_FileHandle::ForcePages (PageNum pageNum)
{
  return pfh_.ForcePages(pageNum);
}

