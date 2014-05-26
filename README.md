on ubuntu 64 bit machine, need to install multilib, 
  sudo apt-get intall g++-multilib

Record Management: store records in paged file provided by PF component, 
first page as special header page

Record Manager: create a file with a fixed record size, delete, open, 
close file.

create file: write meta data of header page, record size total pages, 
page count on this header page, next page directory; total empty page, 
empty page count on this header page, next empty page directory;

RM_FileHandle: after RM_Manger open a file, the RM_FileHandle should be able 
to locate a record, move a record
