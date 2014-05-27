//only used for RM
#define END_PAGE_LIST -1
#define NXT_PAGE_DIR -2 //indicate the next entry is for the next page dir
#define HEADER_LIST_HALF ((PF_PAGE_SIZE - sizeof(int)*7)/2)/4
#define PAGE_DIR_LIST_SIZE (PF_PAGE_SIZE - sizeof(int)*2)/4
#define DATA_ON_RECORD_PAGE (PF_PAGE_SIZE - sizeof(char)*16)

struct RM_FileHeaderPage {
  int recordSize;

  int totalPage; 
  int pageOnThis;
  int nextPageDir; // Linklist of the next Empty Page Dir 

  int totalEmptyPage;
  int emptyPageOnThis; 
  int nextEmptyPageDir; // Linklist of the next Empty Page Directory

  int totalPageList[HEADER_LIST_HALF]; //all data can store
  int emptyPageList[HEADER_LIST_HALF];
};

struct RM_FilePageDirPage {
  int pageListSize;
  int nextPageDir;
  int data[PAGE_DIR_LIST_SIZE];
};

struct RM_FileRecPage {
  unsigned char bitmap[16];
  char data[DATA_ON_RECORD_PAGE]; 
};

inline bool slotTaken(struct RM_FileRecPage * data, int slotNum)
{
  int i = slotNum >> 3;
  int j = slotNum & 7;
  return ((data->bitmap[i]) >> j) & 1;
}

inline void setEmptySlot(RM_FileRecPage * data, int i, int j)
{
  data->bitmap[i] |= 1 << j;
}

inline void setEmptySlot(RM_FileRecPage * data, int slotNum)
{
  setEmptySlot(data, slotNum >> 3, slotNum & 0x7);
}

inline bool findFirstEmptySlot(struct RM_FileRecPage * data, 
                        int &slotNum, int startSlotNum = 0)
{
  int i = startSlotNum >> 3;
  int j = startSlotNum & 0x7;
  for(; j < 8; ++j)
    if((((data->bitmap[i]) >> j) & 1) == 0){
      slotNum = i * 8 + j;
      return true;
    }
  for(++i; i < 16; ++i)
    if( data->bitmap[i] != 0xff)
      break;
  if(i == 16) {
    slotNum = i * 8;
    return false;
  }
  for(j = 0; j < 8; ++j)
    if( (((data->bitmap[i]) >> j) & 1) == 0 ){
      slotNum = i * 8 + j;
      return true;
    }
  return false;  
}

