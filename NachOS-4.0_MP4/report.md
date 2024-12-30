## Contributions
|  | 陳安楷 | 林庭宇 | 
|----------|----------|----------|
|  Code tracing  |   *   |   *   |
|   Report part I  |   *   |      |  
|   Report of implementation  |   *   |      |  
|   Implementation  |      |   *   | 
|   Bonus Implementation  |      |   *   |  


## Answering 5 questions:
(1) How does the NachOS FS manage and find free block space? Where is this information stored on the raw disk (which sector)?
ANS:  
1. Managing free block space:
NachOS FS allocated 2 things in the constructor `FileSystem::FileSystem(bool format)` to manage and find free block space 
* `OpenFile *freeMapfile` for managing free disk space:
    Because it is `class PersistentBitmap` a inheritance from `Bitmap`. Every bit in the bitmap corresponds to whether a sector is used.

* `OpenFile *directoryFile` for managing file name to disk sector mappings
2. Finding free blocks: 
* Iterative method find an unused sector using `int Bitmap::FindAndSet()` function and `bool FileHeader::Allocate(PersistentBitmap *freeMap, int fileSize)`
* `int Bitmap::FindAndSet()`: returns which sector is empty ()
* `bool FileHeader::Allocate(PersistentBitmap *freeMap, int fileSize)`: return sectors based on file size
3. Information stored on raw disk:
* We can see in `FileSystem::FileSystem(bool format)` the file information is being written inside `FreeMapSector` which is *sector 0*. And when we want to see the content we can use `        freeMapFile = new OpenFile(FreeMapSector);` to get the information on raw disk


 (2) What is the maximum disk size that can be handled by the current implementation? Explain why.
ANS: 
Inside `disk.h` we can see
```
const int SectorSize = 128;		// number of bytes per disk sector
const int SectorsPerTrack  = 32;	// number of sectors per disk track 
const int NumTracks = 32;		// number of tracks per disk
const int NumSectors = (SectorsPerTrack * NumTracks); // total # of sectors per disk
```
* disk size = sectorsize * numbersector =  sectorsize * sectorPerTrack * numTracks = 128 * 32 * 32 = 128KB

 (3) How does the NachOS FS manage the directory data structure? Where is this information stored on the raw disk (which sector)?
 ANS:
* In `FileSystem::FileSystem(bool format)` it create `directoryFile` to store directory entry which contains file name, sector number, id.
```
class DirectoryEntry
{
public:
    bool inUse;                    // Is this directory entry in use?
    int sector;                    // Location on disk to find the
                                   //   FileHeader for this file
    char name[FileNameMaxLen + 1]; // Text name for file, with +1 for
                                   // the trailing '\0'
};
```
* The `directoryFile` is stored within `DirectorySector` which is sector 1.
```
#define FreeMapSector 0
#define DirectorySector 1
```

 (4) What information is stored in an inode? Use a figure to illustrate the disk allocation scheme of the current implementation.
 ANS:

inode is `FileHeader` the member `numBytes`stores the size of file, `numSectors` stores the number of sectors used, and `dataSectors` records the sector number for data inside the file.
* Uses direct access to store data inside sector
```
class FileHeader
 {
    int numBytes;   // Number of bytes in the file
    int numSectors; // Number of data sectors in the file
    int dataSectors[NumDirect]; // Disk sector numbers for each data block in the file
 };
```
# 這邊要放圖片

 (5) What is the maximum file size that can be handled by the current implementation? Explain why.
 ANS:
 Because it uses direct access, there is only 128 bytes in a sector. Since a integer is 4 bytes => 128/4 = 32, we can direct access to 32 other sectors (each 128 bytes). Therefore, total size is 128*32 = 4KB.  

## Implementation part II
We used a linked list structure to support larger file size. This increases file size because each file header can store 4kB and by connecting n of them we can handle files that is n*4KB large
### `filehdr.cc`:
`In FileHeader::FileHeader()`
Added `nextFileHeader` and `nextFileHeaderSector` to link to the next sector.
```
FileHeader::FileHeader()
{
	nextFileHeader = NULL;
	nextFileHeaderSector = -1;
	numBytes = -1;
	numSectors = -1;
	memset(dataSectors, -1, sizeof(dataSectors));
}
```
`In FileHeader::~FileHeader()`
Release `nextFileHeader`
```
FileHeader::~FileHeader()
{
	if(nextFileHeader!=NULL)
		delete nextFileHeader;
}
```
`In bool FileHeader::Allocate(PersistentBitmap *freeMap, int fileSize)`
We linked fileheaders using a recursive call. Which allows us to support larger file size.
* We calculated the numBytes that could be write into the sector and subtract from `filesize`
* calculated the number of sectors needed using divRoundUp
```
	nextFileHeader = NULL;
	numBytes = fileSize<=MaxFileSize?fileSize:MaxFileSize;
	fileSize-=numBytes;
	numSectors = divRoundUp(numBytes, SectorSize);
```
Recursive part
* Firstly, check if there is still free sector using `BitMap::FindAndSet()`
* allocate memory for `nextFileHeader`, then recursively call `Allocate(freeMap, fileSize)`
```
    if (fileSize > 0) {
        DEBUG(dbgFile, "ALLOCATE NEXT FILE HEADER");
        nextFileHeaderSector = freeMap->FindAndSet();  
        if (nextFileHeaderSector == -1)
                return FALSE;
        else {
                nextFileHeader = new FileHeader;
                DEBUG(dbgFile, "nextFileHeaderSector: " << nextFileHeaderSector);
                return nextFileHeader->Allocate(freeMap, fileSize);
        }
    }
```
In `void FileHeader::Deallocate(PersistentBitmap *freeMap)`
* Recursively deallocate the linked fileHeaders
```
	if (nextFileHeaderSector != -1)
  	{
 		  nextFileHeader->Deallocate(freeMap);
  	}
```
In `void FileHeader::FetchFrom(int sector)`
* Recursively find the sector that contains file data.
```
	DEBUG(dbgFile, "FileHeader::FetchFrom sector: " << sector);
	if (nextFileHeaderSector != -1)
	{ 
			nextFileHeader = new FileHeader;
			nextFileHeader->FetchFrom(nextFileHeaderSector);
	}
```
In `void FileHeader::WriteBack(int sector)`
* Recursively write back to the file.
```
    DEBUG(dbgFile, "FileHeader::WriteBack sector: " << sector);
	if(nextFileHeaderSector != -1){
		DEBUG(dbgFile,"more than one file header");
		DEBUG(dbgFile, "FileHeader::WriteBack nextFileHeaderSector: " << nextFileHeaderSector);
		nextFileHeader->WriteBack(nextFileHeaderSector);
	}
```
In `int FileHeader::ByteToSector(int offset)`
* Check if file is in range
```
    if (offset < 0 || offset >= FileLength())
        return -1;
```
* Then check which file header would the offset be
* if the number of sectors is more than file header can contain, find the next linked file header
```
    if (offset < 0 || offset >= FileLength())
        return -1;

    // Correctly handle offsets that fall into the next file header
    if ((offset / SectorSize) < NumDirect)
        return dataSectors[offset / SectorSize];
    else
    {
        if (nextFileHeader == NULL)
            return -1;
        else
            return nextFileHeader->ByteToSector(offset - NumDirect * SectorSize);
    }
```
`int FileHeader::FileLength()`
* recursively adds the file length
```
	int lenght = 0;
	if(nextFileHeaderSector == -1)
		lenght = numBytes;
	else{
		DEBUG(dbgFile, "FileHeader::FileLength nextFileHeaderSector: " << nextFileHeaderSector);
		lenght = numBytes + nextFileHeader->FileLength();
	}
	return lenght;
```
In `int FileHeader::CountFileHeaders()`
* recursively counts the number of file fileHeaders
```
int FileHeader::CountFileHeaders()
{
	int count = 1; // Start with the current file header
	FileHeader *current = nextFileHeader;
	while (current != NULL)
	{
		count++;
		current = current->getNextFileHeader();
	}
	return count;
}
```
### `filehdr.h`
* Calculate the number of available sectors in a file header
```
    #define NumDirect ((SectorSize - 4 * sizeof(int)) / sizeof(int))
```
* added public function
```
	FileHeader* getNextFileHeader(){return nextFileHeader;}
    int CountFileHeaders(); /
```
* added private member
```
	int nextFileHeaderSector; 
    FileHeader* nextFileHeader; 
```
### `filesys.cc`
In `FileSystem::OpenAsOpenFile(char *name)`
opened a file that is in the directory
* Create a new OpenFile(sector) object by its sector number
```
 Directory *directory = new Directory(NumDirEntries);
    OpenFile *openFile = NULL;
    int sector;

    DEBUG(dbgFile, "Opening file " << name);
    directory->FetchFrom(directoryFile);
    sector = directory->Find(name);
    if (sector >= 0){
        openFile = new OpenFile(sector); // name was found in directory
        DEBUG(dbgFile, "file found " << name << " with sector " << sector);
    }

    delete directory;
    DEBUG(dbgFile, "returning openfile");
    return openFile; // return NULL if not found
```
In `OpenFileId FileSystem::Open(char *name)`
* Calls `OpenFile * FileSystem::OpenAsOpenFile(char *name)` to open a file
* `openedFile` is created here
```
OpenFileId FileSystem::Open(char *name)
{

    OpenFileId openFileId = -1;
    openFileId = 1;
    openedFile = OpenAsOpenFile(name);
    DEBUG(dbgFile,"OPEN COMPLETE");
    return openFileId; // return -1 if not found

}
```
In `FileSystem::Read(char *buf, int size, OpenFileId id)`
* Read the file using `int OpenFile::Read(char *into, int numBytes)`
```
        DEBUG(dbgFile,"size: " << size);
        for (int i = 0; i < size; i++) {
            DEBUG(dbgFile, "buf[" << i << "]: " << buf[i]);
        }
        int result = openedFile->Read(buf, size);
        for (int i = 0; i < size; i++) {
            DEBUG(dbgFile, "buf[" << i << "]: " << buf[i]);
        }
        DEBUG(dbgFile,"READ COMPLETE");
        return result;
    return 0; // return 0 if not found
```
In `int FileSystem::Write(char *buf, int size, OpenFileId id)`
Write using `int OpenFile::Write(char *from, int numBytes)`
```
    int result = openedFile->Write(buf, size);
    DEBUG(dbgFile,"WRITE COMPLETE");
    return result;
```
In `int FileSystem::Close(OpenFileId id)`
Deallocate `openedFile`
```
    DEBUG(dbgFile,"CLOSE COMPLETE");
    delete openedFile;
    return 1;
```
### `filesys.h`
Added member function
```
	OpenFile *OpenAsOpenFile(char *name); // Open a file (UNIX open)

	OpenFileId Open(char *name);

	int Read(char *buf, int size, OpenFileId id); // Read from a file.

	int Write(char *buf, int size, OpenFileId id); // Write to a file.

	int Close(OpenFileId id); // Close a file.
```
and a private member
```
	OpenFile *openedFile;
```
### `disk.h`
* For **bonus 1**
* changed `NumTracks` because the disk size was not enough
* Added from 128kb to 64mb
```
const int NumTracks = 16384;
```
### `exception.cc`
Exception handling routines, call functions at `ksyscall.h` for OS filesystem service routine.
```


		case SC_Create:
		val = kernel->machine->ReadRegister(4);
		{
			int size = kernel->machine->ReadRegister(5);
			char * filename = &(kernel->machine->mainMemory[val]);
			// create a file with the filename, 
			// implement in ksyscall.h
			status = SysCreate(filename, size);
			// write the file-creation status to register 2
			kernel->machine->WriteRegister(2, (int) status);
			
		}
		
		kernel->machine->WriteRegister(PrevPCReg, kernel->machine->ReadRegister(PCReg));
		kernel->machine->WriteRegister(PCReg, kernel->machine->ReadRegister(PCReg)+4);
		kernel->machine->WriteRegister(NextPCReg ,kernel->machine->ReadRegister(PCReg)+4);
		return;
		ASSERTNOTREACHED();
		break;

		case SC_Open:
		val = kernel->machine->ReadRegister(4);
		{
			char * filename = &(kernel->machine->mainMemory[val]);
			DEBUG(dbgSys, "Open file: " << filename << endl);
			status = SysOpen(filename);
			DEBUG(dbgSys, "exception.cc Open file id: " << status << endl);
			kernel->machine->WriteRegister(2, (int) status);
		}

		kernel->machine->WriteRegister(PrevPCReg, kernel->machine->ReadRegister(PCReg));
		kernel->machine->WriteRegister(PCReg, kernel->machine->ReadRegister(PCReg)+4);
		kernel->machine->WriteRegister(NextPCReg ,kernel->machine->ReadRegister(PCReg)+4);
		return;
		ASSERTNOTREACHED();
		break;

		case SC_Read:
		val = kernel->machine->ReadRegister(4);
		{
			char *buffer = &(kernel->machine->mainMemory[val]);
			int size = kernel->machine->ReadRegister(5);
			OpenFileId id = kernel->machine->ReadRegister(6);
			status = SysRead(buffer, size, id);

			for (int i = 0; i < size; ++i)
				DEBUG(dbgSys, buffer[i]);
			DEBUG(dbgSys, endl);
				
			kernel->machine->WriteRegister(2, (int)status);
		}

		kernel->machine->WriteRegister(PrevPCReg, kernel->machine->ReadRegister(PCReg));
		kernel->machine->WriteRegister(PCReg, kernel->machine->ReadRegister(PCReg)+4);
		kernel->machine->WriteRegister(NextPCReg ,kernel->machine->ReadRegister(PCReg)+4);
		return;
		ASSERTNOTREACHED();
		break;

		case SC_Write:
		val = kernel->machine->ReadRegister(4);
		{
			char *buffer = &(kernel->machine->mainMemory[val]);
			int size = kernel->machine->ReadRegister(5);
			OpenFileId id = kernel->machine->ReadRegister(6);
			status = SysWrite(buffer, size, id);

			DEBUG(dbgSys, "Write to file id: " << id 
				<< " with size: "  << size << "\nContent:\n");
			for (int i = 0; i < size; ++i)
				DEBUG(dbgSys, buffer[i]);
			DEBUG(dbgSys, endl);

			kernel->machine->WriteRegister(2, (int)status);
		}

		kernel->machine->WriteRegister(PrevPCReg, kernel->machine->ReadRegister(PCReg));
		kernel->machine->WriteRegister(PCReg, kernel->machine->ReadRegister(PCReg)+4);
		kernel->machine->WriteRegister(NextPCReg ,kernel->machine->ReadRegister(PCReg)+4);
		return;
		ASSERTNOTREACHED();
		break;

		case SC_Close:
		val = kernel->machine->ReadRegister(4);
		{
			OpenFileId id = val;
			status = SysClose(id);
			kernel->machine->WriteRegister(2, (int)status);
		}
		kernel->machine->WriteRegister(PrevPCReg, kernel->machine->ReadRegister(PCReg));
		kernel->machine->WriteRegister(PCReg, kernel->machine->ReadRegister(PCReg)+4);
		kernel->machine->WriteRegister(NextPCReg ,kernel->machine->ReadRegister(PCReg)+4);
		return;
		ASSERTNOTREACHED();
		break;
		
		case SC_Exit:
			DEBUG(dbgAddr, "Program exit\n");
			val = kernel->machine->ReadRegister(4);
			kernel->currentThread->Finish();
			break;
		default:
			cerr << "Unexpected system call " << type << "\n";
			break;
		}

		kernel->machine->WriteRegister(PrevPCReg, kernel->machine->ReadRegister(PCReg));
		kernel->machine->WriteRegister(PCReg, kernel->machine->ReadRegister(PCReg)+4);
		kernel->machine->WriteRegister(NextPCReg ,kernel->machine->ReadRegister(PCReg)+4);
		return;
		ASSERTNOTREACHED();
		break;
```
In `ksyscall.h`
Calls the `kernel->filesystem` to handle service routine. (functions explained before)
```
int SysCreate(char *filename, int size)
{
    // return value
    // 1: success
    // 0: failed
    return kernel->fileSystem->Create(filename, size);
}
OpenFileId SysOpen(char *filename)
{
    // return value
    // > 0: success
    // 0: failed
    return kernel->fileSystem->Open(filename);
}
int SysRead(char *buf, int size, OpenFileId id)
{
    DEBUG(dbgFile, "SysRead: " << size << " bytes from file " << id);
    return kernel->fileSystem->Read(buf, size, id);
}
int SysWrite(char *buf, int size, OpenFileId id)
{
    return kernel->fileSystem->Write(buf, size, id);
}
int SysClose(OpenFileId id)
{
    return kernel->fileSystem->Close(id);
}
```

