#include<cstdlib>
#include<cerrno>
#include<cstdio>
#include<iostream>
#include<string>
#include<cstring>
#include "rm.h"
using namespace std;

// Error table

static char *RM_RecordManagerMsg[] = {
  (char *)"rm record size not good in creating file",
  (char *)"destroy file while it is still open",
  (char *)"open file with a file handler already opened a file",
  (char *)"close file with a file handler already closed",
  (char *)"creating file, but header page has error",
  (char *)"header page error when opening the file"
};

static char *RM_FileHandleMsg[] = {
  (char *)"file handler did not open file",
  (char *)"the record does not exist",
  (char *)"record length does not match"
};

static char *RM_FileScanMsg[] = {
  (char *)"end of file scan",
  (char *)"file scan is not open",
  (char *)"reopen file scan while it is already opened",
  (char *)"need to have some values for scan"
};

void RM_PrintError(RC rc)
{
  if(rc >= 1 && rc <= RM_RM_ERROR_END)
    cerr << "RM error: "<<RM_RecordManagerMsg[rc - 1] << endl;
  else if ( rc >= 11 && rc <= RM_FH_ERROR_END)
    cerr << "RM error: "<<RM_FileHandleMsg[rc - 11] << endl;
  else if ( rc >= 21 && rc <= RM_SCAN_ERROR_END)
    cerr << "RM error: "<<RM_FileScanMsg[rc-21] << endl; 
  else if ( rc == 0 )
    cerr << "RM_PrintError called with return code of 0\n";
  else
    cerr << "OS error?\n" << strerror(errno) << "\n";
}
