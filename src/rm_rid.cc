#include "rm_rid.h"
RID::RID()                                         // Default constructor
{
  pageNum_ = 0;
  slotNum_ = 0;
}
RID::RID(PageNum pageNum, SlotNum slotNum)
{
  pageNum_ = pageNum;
  slotNum_ = slotNum;
}
RID::RID(const RID & rid)
{
  pageNum_ = rid.pageNum_;
  slotNum_ = rid.slotNum_;
}
RID::~RID()                                        // Destructor
{
}

RC RID::GetPageNum(PageNum &pageNum) const         // Return page number
{
  pageNum = pageNum_;
  return OK_RC;
}
RC RID::GetSlotNum(SlotNum &slotNum) const         // Return slot number
{
  slotNum = slotNum_;
  return OK_RC;
}

