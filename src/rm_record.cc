#include "rm.h"
#include <cstdlib>

RM_Record::RM_Record ()
{
  recordSize = 0;
  data = NULL;
}

RM_Record::~RM_Record()
{
  if(data != NULL)
    free(data);
}

    // Return the data corresponding to the record.  Sets *pData to the
    // record contents.
RC RM_Record::GetData(char *&pData) const
{
  pData = data;
  return OK_RC;
}

    // Return the RID associated with the record
RC RM_Record::GetRid (RID &rid) const
{
  rid = rid_;
  return OK_RC;
}

