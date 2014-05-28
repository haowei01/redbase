//
// File:        rm_testshell.cc
// Description: Test RM component
// Authors:     Jan Jannink
//              Dallan Quass (quass@cs.stanford.edu)
//              Jason McHugh (mchughj@cs.stanford.edu)
//
// This test shell contains a number of functions that will be useful
// in testing your RM component code.  In addition, a couple of sample
// tests are provided.  The tests are by no means comprehensive, however,
// and you are expected to devise your own tests to test your code.
//
// 1997:  Tester has been modified to reflect the change in the 1997
// interface.  For example, FileHandle no longer supports a Scan over the
// relation.  All scans are done via a FileScan.
//

#include <cstdio>
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <cstdlib>

#include "redbase.h"
#include "pf.h"
#include "rm.h"
#include "rm_internal.h"
using namespace std;

//
// Defines
//
#define FILENAME   (char*)("testrel")         // test file name
#define STRLEN      29               // length of string in testrec
#define PROG_UNIT   50               // how frequently to give progress
                                      //   reports when adding lots of recs
#define FEW_RECS   200                // number of records added in
#define MANY_RECS  200000           // stress test with many records
#define HUGE_RECS  2000000
//
// Computes the offset of a field in a record (should be in <stddef.h>)
//
#ifndef offsetof
#       define offsetof(type, field)   ((size_t)&(((type *)0) -> field))
#endif

//
// Structure of the records we will be using for the tests
//
struct TestRec {
    char  str[STRLEN];
    int   num;
    float r;
//    char data[4000];
};

//
// Global PF_Manager and RM_Manager variables
//
PF_Manager pfm;
RM_Manager rmm(pfm);

//
// Function declarations
//
RC Test1(void);
RC Test2(void);
RC Test3(void);
RC Test4(void);
RC Test5(void);

int dummyInt;

void PrintError(RC rc);
void LsFile(char *fileName);
void PrintRecord(TestRec &recBuf);
RC AddRecs(RM_FileHandle &fh, int numRecs, int offset = 0);
RC VerifyFile(RM_FileHandle &fh, int numRecs);
RC PrintFile(RM_FileHandle &fh);
RC DumpFile(char *fileName, int & totalPages = dummyInt);

RC CreateFile(char *fileName, int recordSize);
RC DestroyFile(char *fileName);
RC OpenFile(char *fileName, RM_FileHandle &fh);
RC CloseFile(char *fileName, RM_FileHandle &fh);
RC InsertRec(RM_FileHandle &fh, char *record, RID &rid);
RC UpdateRec(RM_FileHandle &fh, RM_Record &rec);
RC DeleteRec(RM_FileHandle &fh, RID &rid);
RC GetNextRecScan(RM_FileScan &fs, RM_Record &rec);

//
// Array of pointers to the test functions
//
#define NUM_TESTS       5               // number of tests
int (*tests[])() =                      // RC doesn't work on some compilers
{
    Test1,
    Test2,
    Test3,
    Test4,
    Test5
};

//
// main
//
int main(int argc, char *argv[])
{
    RC   rc;
    char *progName = argv[0];   // since we will be changing argv
    int  testNum;

    // Write out initial starting message
    cerr.flush();
    cout.flush();
    cout << "Starting RM component test.\n";
    cout.flush();

    // Delete files from last time
    unlink(FILENAME);

    // If no argument given, do all tests
    if (argc == 1) {
        for (testNum = 0; testNum < NUM_TESTS; testNum++)
            if ((rc = (tests[testNum])())) {

                // Print the error and exit
                PrintError(rc);
                return (1);
            }
    }
    else {

        // Otherwise, perform specific tests
        while (*++argv != NULL) {

            // Make sure it's a number
            if (sscanf(*argv, "%d", &testNum) != 1) {
                cerr << progName << ": " << *argv << " is not a number\n";
                continue;
            }

            // Make sure it's in range
            if (testNum < 1 || testNum > NUM_TESTS) {
                cerr << "Valid test numbers are between 1 and " << NUM_TESTS << "\n";
                continue;
            }

            // Perform the test
            if ((rc = (tests[testNum - 1])())) {

                // Print the error and exit
                PrintError(rc);
                return (1);
            }
        }
    }

    // Write ending message and exit
    cout << "Ending RM component test.\n\n";

    return (0);
}

//
// PrintError
//
// Desc: Print an error message by calling the proper component-specific
//       print-error function
//
void PrintError(RC rc)
{
    if (abs(rc) <= END_PF_WARN)
        PF_PrintError(rc);
    else if (abs(rc) <= END_RM_WARN)
        RM_PrintError(rc);
    else
        cerr << "Error code out of range: " << rc << "\n";
}

////////////////////////////////////////////////////////////////////
// The following functions may be useful in tests that you devise //
////////////////////////////////////////////////////////////////////

//
// LsFile
//
// Desc: list the filename's directory entry
//
void LsFile(char *fileName)
{
    char command[80];

    sprintf(command, "ls -l %s", fileName);
    printf("doing \"%s\"\n", command);
    system(command);
}

//
// PrintRecord
//
// Desc: Print the TestRec record components
//
void PrintRecord(TestRec &recBuf)
{
    printf("[%s, %d, %f]\n", recBuf.str, recBuf.num, recBuf.r);
}

//
// AddRecs
//
// Desc: Add a number of records to the file
//
RC AddRecs(RM_FileHandle &fh, int numRecs, int offset)
{
    RC      rc;
    int     i;
    TestRec recBuf;
    RID     rid;
    PageNum pageNum;
    SlotNum slotNum;
    RM_Record check;

    // We set all of the TestRec to be 0 initially.  This heads off
    // warnings that Purify will give regarding UMR since sizeof(TestRec)
    // is 40, whereas actual size is 37.
    memset((void *)&recBuf, 0, sizeof(recBuf));

    printf("\nadding %d records\n", numRecs);
    for (i = 0; i < numRecs; i++) {
        memset(recBuf.str, ' ', STRLEN);
        sprintf(recBuf.str, "a%d", i + offset);
        recBuf.num = i + offset;
        recBuf.r = (float)(i+offset);
        if ((rc = InsertRec(fh, (char *)&recBuf, rid)) ||
            (rc = rid.GetPageNum(pageNum)) ||
            (rc = rid.GetSlotNum(slotNum)) || 
            (rc = fh.GetRec(rid, check)) )
            return (rc);

        if ((i + 1) % PROG_UNIT == 0){
            printf("%d  ", i + 1);
            fflush(stdout);
        }
    }
    if (i % PROG_UNIT != 0)
        printf("%d\n", i);
    else
        putchar('\n');

    // Return ok
    return (0);
}

//
// VerifyFile
//
// Desc: verify that a file has records as added by AddRecs
//
RC VerifyFile(RM_FileHandle &fh, int numRecs)
{
    RC        rc;
    int       n;
    TestRec   *pRecBuf;
    RID       rid;
    char      stringBuf[STRLEN];
    char      *found;
    RM_Record rec;
    int scanCount = 0;

    found = new char[numRecs];
    memset(found, 0, numRecs);

    printf("\nverifying file contents\n");

    RM_FileScan fs;
    if ((rc=fs.OpenScan(fh,INT,sizeof(int),offsetof(TestRec, num),
                        NO_OP, NULL, NO_HINT)))
        return (rc);

    // For each record in the file
    for (rc = GetNextRecScan(fs, rec), n = 0;
         rc == 0;
         rc = GetNextRecScan(fs, rec), n++) {

        // Make sure the record is correct
        if ((rc = rec.GetData((char *&)pRecBuf)) ||
            (rc = rec.GetRid(rid)))
            goto err;

        memset(stringBuf,' ', STRLEN);
        sprintf(stringBuf, "a%d", pRecBuf->num);

        if (pRecBuf->num < 0 || pRecBuf->num >= numRecs ||
            strcmp(pRecBuf->str, stringBuf) ||
            pRecBuf->r != (float)pRecBuf->num) {
            printf("VerifyFile: invalid record = [%s, %d, %f]\n",
                   pRecBuf->str, pRecBuf->num, pRecBuf->r);
            exit(1);
        }

        if (found[pRecBuf->num]) {
            printf("VerifyFile: duplicate record = [%s, %d, %f]\n",
                   pRecBuf->str, pRecBuf->num, pRecBuf->r);
            exit(1);
        }

        found[pRecBuf->num] = 1;
        scanCount ++;
    }

    if (rc != RM_EOF)
        goto err;
    printf("++++ Total Scanned %d\n", scanCount);

    if ((rc=fs.CloseScan()))
        return (rc);

    // make sure we had the right number of records in the file
    if (n != numRecs) {
        printf("%d records in file (supposed to be %d)\n",
               n, numRecs);
        exit(1);
    }

    // Return ok
    rc = 0;

err:
    fs.CloseScan();
    delete[] found;
    return (rc);
}

RC DumpFile(char *fileName, int & totalPages )
{
  PF_FileHandle pf;
  PF_PageHandle ph;
  pfm.OpenFile(fileName, pf);
  pf.GetFirstPage(ph);
  struct RM_FileHeaderPage *data;
  ph.GetData((char * &) data);
  printf("+++ Dump Page +++\n"); 
  printf("++ record size %d\n", data->recordSize);
  printf("++ next page dir %d\n", data->nextPageDir);
  printf("++ const %d\n", END_PAGE_LIST);
  printf("total page %d\n", data->totalPage);
  totalPages = data->totalPage;
  printf("first data page %d\n", data->totalPageList[0]);
  printf("total empty page %d\n", data->totalEmptyPage);
  printf("first empty page idx %d\n", data->emptyPageList[0]);
  pfm.CloseFile(pf);
}

//
// PrintFile
//
// Desc: Print the contents of the file
//
RC PrintFile(RM_FileScan &fs)
{
    RC        rc;
    int       n;
    TestRec   *pRecBuf;
    RID       rid;
    RM_Record rec;

    printf("\nprinting file contents\n");

    // for each record in the file
    for (rc = GetNextRecScan(fs, rec), n = 0;
         rc == 0;
         rc = GetNextRecScan(fs, rec), n++) {

        // Get the record data and record id
        if ((rc = rec.GetData((char *&)pRecBuf)) ||
            (rc = rec.GetRid(rid)))
            return (rc);

        // Print the record contents
        PrintRecord(*pRecBuf);
    }

    if (rc != RM_EOF)
        return (rc);

    printf("%d records found\n", n);

    // Return ok
    return (0);
}

////////////////////////////////////////////////////////////////////////
// The following functions are wrappers for some of the RM component  //
// methods.  They give you an opportunity to add debugging statements //
// and/or set breakpoints when testing these methods.                 //
////////////////////////////////////////////////////////////////////////

//
// CreateFile
//
// Desc: call RM_Manager::CreateFile
//
RC CreateFile(char *fileName, int recordSize)
{
    printf("\ncreating %s\n", fileName);
    return (rmm.CreateFile(fileName, recordSize));
}

//
// DestroyFile
//
// Desc: call RM_Manager::DestroyFile
//
RC DestroyFile(char *fileName)
{
    printf("\ndestroying %s\n", fileName);
    return (rmm.DestroyFile(fileName));
}

//
// OpenFile
//
// Desc: call RM_Manager::OpenFile
//
RC OpenFile(char *fileName, RM_FileHandle &fh)
{
    printf("\nopening %s\n", fileName);
    return (rmm.OpenFile(fileName, fh));
}

//
// CloseFile
//
// Desc: call RM_Manager::CloseFile
//
RC CloseFile(char *fileName, RM_FileHandle &fh)
{
    if (fileName != NULL)
        printf("\nClosing %s\n", fileName);
    return (rmm.CloseFile(fh));
}

//
// InsertRec
//
// Desc: call RM_FileHandle::InsertRec
//
RC InsertRec(RM_FileHandle &fh, char *record, RID &rid)
{
    return (fh.InsertRec(record, rid));
}

//
// DeleteRec
//
// Desc: call RM_FileHandle::DeleteRec
//
RC DeleteRec(RM_FileHandle &fh, RID &rid)
{
    return (fh.DeleteRec(rid));
}

//
// UpdateRec
//
// Desc: call RM_FileHandle::UpdateRec
//
RC UpdateRec(RM_FileHandle &fh, RM_Record &rec)
{
    return (fh.UpdateRec(rec));
}

//
// GetNextRecScan
//
// Desc: call RM_FileScan::GetNextRec
//
RC GetNextRecScan(RM_FileScan &fs, RM_Record &rec)
{
    return (fs.GetNextRec(rec));
}

/////////////////////////////////////////////////////////////////////
// Sample test functions follow.                                   //
/////////////////////////////////////////////////////////////////////

//
// Test1 tests simple creation, opening, closing, and deletion of files
//
RC Test1(void)
{
    RC            rc;
    RM_FileHandle fh;

    printf("test1 starting ****************\n");

    if ((rc = CreateFile(FILENAME, sizeof(TestRec))) ||
        (rc = OpenFile(FILENAME, fh)) ||
        (rc = CloseFile(FILENAME, fh)))
        return (rc);

    LsFile(FILENAME);
    DumpFile(FILENAME);

    if ((rc = DestroyFile(FILENAME)))
        return (rc);

    printf("\ntest1 done ********************\n");
    return (0);
}

//
// Test2 tests adding a few records to a file.
//
RC Test2(void)
{
    RC            rc;
    RM_FileHandle fh;

    printf("test2 starting ****************\n");

    if ((rc = CreateFile(FILENAME, sizeof(TestRec))) ||
        (rc = OpenFile(FILENAME, fh)) ||
        (rc = AddRecs(fh, FEW_RECS)) ||
        (rc = CloseFile(FILENAME, fh)))
        return (rc);

    LsFile(FILENAME);

    rc = OpenFile(FILENAME, fh);
    DumpFile(FILENAME);
    rc = VerifyFile(fh, FEW_RECS); 
    rc = CloseFile(FILENAME, fh);
    
    printf("**** delete one record and then scan\n");
    int recordPerPage = fh.GetRecordPerPage();
    RID deleteId(0, recordPerPage - 1);
    if( (rc = fh.DeleteRec(deleteId)) == 0 ){
      printf("Cannot delete while file close\n");
      exit(1);
    }
    rc = OpenFile(FILENAME, fh);
    for(int i=0; i < recordPerPage; ++ i) {
      deleteId = RID(0, i);
      if( (rc = fh.DeleteRec(deleteId)) != 0 ){
        printf("delete last record on one page failed\n");
        exit(1);
      }
    }
    RM_FileScan fs;
    RM_Record rec;
    rc=fs.OpenScan(fh,INT,sizeof(int),offsetof(TestRec, num),
                        NO_OP, NULL, NO_HINT);
    for(int i=0; i<FEW_RECS - recordPerPage ; ++ i) {
      rc = fs.GetNextRec(rec);
      if( rc != 0) {
        printf("scan fail at %d th scan\n", i);
        exit(1);
      }
    }
    rc = CloseFile(FILENAME, fh);

    if ((rc = DestroyFile(FILENAME)))
        return (rc);

    printf("\ntest2 done ********************\n");
    return (0);
}

RC Test3(void)
{
  RM_FileHandle fh;
  RC rc;
  if((rc = DestroyFile(FILENAME)) != 0)
  	printf("RC: %d destroy nonexist\n", rc);
  else
    exit(1);

  if( (rc = CreateFile(FILENAME, 10000)) != 0)
  	printf("RC: %d create record too big\n", rc);
  else
    exit(1);

  if((rc = CreateFile(FILENAME, sizeof(TestRec))) == 0)
  	printf("RC: %d create normal\n", rc);
	else
    exit(1);

  if( (rc = CreateFile(FILENAME, sizeof(TestRec))) != 0)
    printf("RC: %d create same name\n", rc);
  else
    exit(1);

  if( (rc =OpenFile(FILENAME, fh) ) == 0 )
    printf("RC: %d open file normal\n", rc);
  else
    exit(1);

  RID no_exist_id( 1, 1);
  RM_Record rec;
  if( (rc = fh.GetRec(no_exist_id, rec ) ) != 0 )
    printf("RC: %d Get No Exist Record\n", rc);
  else
    exit(1);

  if( (rc = DestroyFile(FILENAME)) != 0)
    printf("RC: %d destroy file when it is open\n", rc);
  else
    exit(1);
  CloseFile(FILENAME, fh);

  if( (rc = CloseFile(FILENAME, fh)) != 0)
    printf("RC: %d close twice\n", rc);
  else
    exit(1);

  if( (rc =DestroyFile(FILENAME) ) == 0)
    printf("RC: %d destroy normal\n", rc);
  else
    exit(1);

  printf("test3 starting ************\n");
  printf("test3 done **************\n");
  return (0);
}

RC Test4(void)
{
    RC            rc;
    RM_FileHandle fh;

    printf("test4 starting ****************\n");

    if ((rc = CreateFile(FILENAME, sizeof(TestRec))) ) {
      cerr << "xxxx cannot create " <<FILENAME <<endl;
      return (rc); 
    }
    int round = MANY_RECS/FEW_RECS;
    for(int i=0; i<round; ++i) {
//      cout << "round "<< i << endl;
      if ((rc = OpenFile(FILENAME, fh)) ||
        (rc = AddRecs(fh, FEW_RECS, i*FEW_RECS)) ||
        (rc = CloseFile(FILENAME, fh))) {
        cerr<< "xxxx cannot insert in round "<< i << endl;
        return (rc);
      }
    }

    LsFile(FILENAME);

    int totalPages;
    rc = OpenFile(FILENAME, fh);
    DumpFile(FILENAME, totalPages);
    rc = VerifyFile(fh, MANY_RECS);

    printf("**** check delete\n");
    for(int i=0; i<totalPages; ++i) {
      RID deleteId(i, 0);
      if((rc = fh.DeleteRec(deleteId)) != 0) {
        printf("Cannot delete Record at VPage %d\n", i);
        exit(1);
      }
    }
    rc = CloseFile(FILENAME, fh);

    // check if delete persistent
    rc = OpenFile(FILENAME, fh);
    RM_Record rec;
    for(int i=0; i<totalPages; ++i) {
      RID deleteId(i, 0);
      if((rc = fh.GetRec(deleteId, rec)) == 0) {
        printf("Delete Record at VPage %d did not success\n", i);
        exit(1);
      }
    }
    // check if the delted slot is usable for insert
    TestRec recBuf;
    bool insertToDelted = false;
    for(int i=0; i<totalPages; ++i) {
      RID insertId;
      if(fh.InsertRec((const char *) &recBuf, insertId) != 0){
        printf("cannot insert into deleted empty slot\n");
        exit(1);
      }
      int pageNum, slotNum;
      insertId.GetPageNum(pageNum);
      if(pageNum == 0) {
        insertToDelted = true;
        printf("**** Begin to check Insert To Delted Position\n");
      }
      insertId.GetSlotNum(slotNum);
      if(insertToDelted && slotNum != 0) {
        printf("Insert Record at VPage %d, Slot %d\n", pageNum, slotNum);
        exit(1);
      }
    } 
    rc = CloseFile(FILENAME, fh);
    if ((rc = DestroyFile(FILENAME)))
        return (rc);

    printf("sizeof float %d\n", sizeof(float));
    printf("\ntest4 done ********************\n");
    return (0);
}

//
// Test5 tests various scan condition
//
RC Test5(void)
{
    RC            rc;
    RM_FileHandle fh;

    printf("test5 starting ****************\n");

    if ((rc = CreateFile(FILENAME, sizeof(TestRec))) ||
        (rc = OpenFile(FILENAME, fh)) ||
        (rc = AddRecs(fh, FEW_RECS)) ||
        (rc = CloseFile(FILENAME, fh)))
        return (rc);

    LsFile(FILENAME);

    rc = OpenFile(FILENAME, fh);
    DumpFile(FILENAME);
    rc = VerifyFile(fh, FEW_RECS); 
    rc = CloseFile(FILENAME, fh);

    rc = OpenFile(FILENAME, fh);

    RM_FileScan fs;
    RM_Record rec;

    int scanCount = 0;
    int equal_val = 1;
    printf("********* check equal scan\n");
    rc = fs.OpenScan(fh, INT, sizeof(int), offsetof(TestRec, num), 
                    EQ_OP, &equal_val, NO_HINT);
    for(int i=0; i<FEW_RECS && fs.GetNextRec(rec) == 0; ++i)
      ++scanCount;

    fs.CloseScan();
    if(scanCount != 1) {
      cout << "equal scan fail, scanCount " << scanCount << endl;
      exit(1);
    }
    
    printf("********* check less than scan\n");
    int lt_val = FEW_RECS/2;
    scanCount = 0;
    rc = fs.OpenScan(fh, INT, sizeof(int), offsetof(TestRec, num), 
                    LT_OP, &lt_val, NO_HINT);
    for(int i=0; i<FEW_RECS && fs.GetNextRec(rec) == 0; ++i)
      ++scanCount;

    fs.CloseScan();
    if(scanCount != FEW_RECS/2) {
      cout << "LT_OP scan fail, scanCount " << scanCount << endl;
      exit(1);
    }
    
    printf("********* check greater than or equal scan\n");
    int ge_val = FEW_RECS/2;
    scanCount = 0;
    rc = fs.OpenScan(fh, INT, sizeof(int), offsetof(TestRec, num),
                    GE_OP, &ge_val, NO_HINT);
    for(int i=0; i<FEW_RECS && fs.GetNextRec(rec) == 0; ++i)
      ++scanCount;

    fs.CloseScan();
    if(scanCount != FEW_RECS/2) {
      cout << "GE_OP scan fail, scanCount " << scanCount << endl;
      exit(1);
    }
    
    printf("**** delete record of the whole page and then scan\n");
    int recordPerPage = fh.GetRecordPerPage();
    RID deleteId;

    for(int i=0; i < recordPerPage; ++ i) {
      deleteId = RID(0, i);
      if( (rc = fh.DeleteRec(deleteId)) != 0 ){
        printf("delete last record on one page failed\n");
        exit(1);
      }
    }
    
    printf("********* check greater than or equal scan after delete\n");
    rc=fs.OpenScan(fh,INT,sizeof(int),offsetof(TestRec, num),
                        GE_OP, &ge_val, NO_HINT);
    scanCount = 0;
    for(int i=0; i<FEW_RECS && fs.GetNextRec(rec) == 0 ; ++ i) 
      ++scanCount;
   
    if(scanCount != FEW_RECS/2 - (recordPerPage - ge_val)) {
      cout << "GE_OP scan fail after delete, scanCount " << scanCount << endl;
      exit(1);
    } 
    fs.CloseScan();
  
    rc = CloseFile(FILENAME, fh);

    if ((rc = DestroyFile(FILENAME)))
        return (rc);

    printf("\ntest5 done ********************\n");
    return (0);
}
