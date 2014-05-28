//
// rm.h
//
//   Record Manager component interface
//
// This file does not include the interface for the RID class.  This is
// found in rm_rid.h
//

#ifndef RM_H
#define RM_H

// Please DO NOT include any files other than redbase.h and pf.h in this
// file.  When you submit your code, the test program will be compiled
// with your rm.h and your redbase.h, along with the standard pf.h that
// was given to you.  Your rm.h, your redbase.h, and the standard pf.h
// should therefore be self-contained (i.e., should not depend upon
// declarations in any other file).

// Do not change the following includes
#include "redbase.h"
#include "rm_rid.h"
#include "pf.h"
#include <string>
#include <list>
#include <vector>
#include <map>
using namespace std;

//
// RM_Record: RM Record interface
//
class RM_Record {
  friend class RM_FileScan;
  friend class RM_FileHandle;
public:
    RM_Record ();
    ~RM_Record();

    // Return the data corresponding to the record.  Sets *pData to the
    // record contents.
    RC GetData(char *&pData) const;

    // Return the RID associated with the record
    RC GetRid (RID &rid) const;
private:
  int recordSize;
  char *data;
  RID rid_;
};

//
// RM_FileHandle: RM File interface
//
class RM_FileHandle {
  friend class RM_Manager;
  friend class RM_FileScan;
public:
    RM_FileHandle ();
    ~RM_FileHandle();

    // Given a RID, return the record
    RC GetRec     (const RID &rid, RM_Record &rec) const;

    RC InsertRec  (const char *pData, RID &rid);       // Insert a new record

    RC DeleteRec  (const RID &rid);                    // Delete a record
    RC UpdateRec  (const RM_Record &rec);              // Update a record

    // Forces a page (along with any contents stored in this class)
    // from the buffer pool to disk.  Default value forces all pages.
    RC ForcePages (PageNum pageNum = ALL_PAGES);
    inline int GetRecordPerPage() const { return recordPerPage; }
private:
  bool fileOpen_;
  PF_FileHandle pfh_;
  string fileName_;
  int recordSize;
  int bitmapSize;
  int recordPerPage;
  int totalPage;
  int totalEmptyPage;
  bool headerUpdate;
  vector<PageNum> totalPageList; // this is the actual page number
  list<PageNum> emptyPageList; // this is the virtual page number
  RC check_record_exist(const RID &, PageNum &, SlotNum &, 
                        PageNum &, char*&) const;
};

//
// RM_FileScan: condition-based scan of records in the file
//
class RM_FileScan {
public:
    RM_FileScan  ();
    ~RM_FileScan ();

    RC OpenScan  (const RM_FileHandle &fileHandle,
                  AttrType   attrType,
                  int        attrLength,
                  int        attrOffset,
                  CompOp     compOp,
                  void       *value,
                  ClientHint pinHint = NO_HINT); // Initialize a file scan
    RC GetNextRec(RM_Record &rec);               // Get next matching record
    RC CloseScan ();                             // Close the scan
private:
  bool scanOpen_;
  const RM_FileHandle *rmFileHandle;
  AttrType attrType_;
  int attrLength_;
  int attrOffset_;
  CompOp compOp_;
  char buf[MAXSTRINGLEN + 1];
  RID curScanId_;
  int intVal_;
  float floatVal_;
  string stringVal_;
  bool check_scan_cond(char *recData);
  SlotNum nextRecSlot(const unsigned char *bitmap, int bitmapSize, SlotNum start);
  template<typename DataType>
  bool check_scan_data_cond(const DataType attr, const DataType value);
};

//
// RM_Manager: provides RM file management
//
class RM_Manager {
public:
    RM_Manager    (PF_Manager &pfm);
    ~RM_Manager   ();

    RC CreateFile (const char *fileName, int recordSize);
    RC DestroyFile(const char *fileName);
    RC OpenFile   (const char *fileName, RM_FileHandle &fileHandle);

    RC CloseFile  (RM_FileHandle &fileHandle);
private:
  PF_Manager &pfm_;
  map<string, int> openFile_;
  RC install_page_list(const PF_FileHandle &, RM_FileHandle &, void *, bool);
  RC write_back_total_page(RM_FileHandle &, void*, void *, bool);
  RC recur_dispose_dir_page(RM_FileHandle &, PageNum);
};

//
// Print-error function
//
void RM_PrintError(RC rc);

#define RM_EOF 21 //todo
#endif

#define RM_CREATE_FILE_RECORD_SIZE 1
#define RM_DESTROY_FILE_WHILE_OPEN 2
#define RM_OPEN_FILE_W_OPEN_HANDLE 3
#define RM_CLOSE_FILE_W_CLOSED_HANDLE 4
#define RM_CREATE_FILE_HDR_PAGE_WRITE_ERROR 5

#define RM_OPEN_FILE_HDR_PAGE_ERROR 6
#define RM_RM_ERROR_END 6

#define RM_NOT_OPEN_FILE 11
#define RM_REC_NO_EXIST 12
#define RM_REC_LEN_NO_MATCH 13
#define RM_FH_ERROR_END 13

#define RM_SCAN_NOT_OPEN 22
#define RM_SCAN_REOPEN 23
#define RM_SCAN_NEED_VALUE 24
#define RM_SCAN_ERROR_END 24
