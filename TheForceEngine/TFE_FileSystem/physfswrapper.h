#ifndef _PHYSFSWRAPPER_H_
#define _PHYSFSWRAPPER_H_

#include <vector>
#include <string>
#include <physfs.h>
#include <SDL_endian.h>

#ifndef TFE_MAX_PATH
#define TFE_MAX_PATH 1024
#endif

enum TFE_VPATH {
	VPATH_NONE = 0,		// absolute vpath
	VPATH_TFE  = 1,		// TFE Files
	VPATH_GAME = 2,		// all game-related data
	VPATH_TMP  = 3,		// temporary path
	VPATH_TMP2 = 4,		// another temp path
	VPATH_EDPRJ= 5		// current editor project space
};

enum TFE_WMODE {
	WMODE_WRITE,
	WMODE_APPEND
};

typedef PHYSFS_File* TFEFile;
typedef std::vector<std::string> TFEFileList;
typedef std::vector<const char *> TFEExtList;	// {"txt","ttf"}

static TFEExtList TFEAllExts = {};

struct SDL_RWops;
struct vpMount;

typedef vpMount* TFEMount;

int vpInit(const char * const argv0, const char * const userdata);
void vpDeinit(void);

/* create an absolute virtual path string.
 * returns an allocated buffer which must be free()d by the caller.
 */
char* vpToVpath(TFE_VPATH v, const char *fn);

TFEFile    vpFileOpen(const char* filename, TFE_VPATH vpathid = VPATH_NONE, bool matchcase = true);
SDL_RWops* vpFileOpenRW(const char* filename, TFE_VPATH vpathid = VPATH_NONE, bool matchcase = true);

bool vpMkdir(const char* dirname);
TFEFile    vpOpenWrite(const char* fn, TFE_WMODE wmode = WMODE_WRITE);
SDL_RWops* vpOpenWriteRW(const char* fn, TFE_WMODE wmode = WMODE_WRITE);

void vpCloseRW(SDL_RWops *rw);
void vpClose(TFEFile f);

bool vpGetFileList(TFE_VPATH vpathid, TFEFileList& inout, TFEExtList& te = TFEAllExts);
bool vpGetFileList(TFE_VPATH vpathid, const char* subpath, TFEFileList& inout, TFEExtList& te = TFEAllExts, bool matchcase = true);

bool vpFileExists(const char* filename, bool matchcase = true);
bool vpFileExists(TFEMount mnt, const char* filename, bool matchcase = true);
bool vpFileExists(TFE_VPATH vpathid, const char* filename, bool matchcase = true);

TFEMount vpMountReal(const char* srcname, TFE_VPATH vpdst, bool front = true);
TFEMount vpMountVirt(TFE_VPATH vpsrc, const char *srcname, TFE_VPATH vpdst, bool front = true, bool matchcase = true);

bool vpUnmount(TFEMount mnt);
void vpUnmountTree(TFE_VPATH vpid);

bool vpDirExists(const char* dirname, TFE_VPATH vpsrc = VPATH_NONE);

void vpDeleteFile(const char* filename);
bool vpSetWriteDir(const char *realpath);

const char* vpGetFileContainer(TFE_VPATH vpathid, const char *fn, bool matchcase = true);

/******************************************************************************/

class vpFile {
public:
	vpFile();
	vpFile(TFE_VPATH vpathid, const char* name, char** buf, unsigned int* size, bool matchcase = true);
	vpFile(TFEMount mnt, const char* name, char** buf, unsigned int* size, bool matchcase = true);
	vpFile(TFE_VPATH vpathid, const char* name, bool matchcase = true);
	vpFile(TFEMount mnt, const char* name, bool matchcase = true);
	~vpFile();

	/* openread: open a file for reading, relative to either the vpathid root
	 * or the full tree root, by default matching the case.
	 */
	bool openread(TFE_VPATH vpathid, const char* name, bool matchcase = true);
	bool openread(const char *filepath, bool matchcase = true);

	/* openwrite: open for writing/appending. no path since its always at
	 * the last set write directory, which is the user documents path
	 */
	bool openwrite(const char *fn, TFE_WMODE mode = WMODE_WRITE);
	/* size: filesize */
	PHYSFS_sint64 size(void);
	/* tell: where we're at */
	PHYSFS_sint64 tell(void);
	/* seek: absolute seek */
	bool seek(PHYSFS_uint64 pos);
	/* eof: enf of file reached or not */
	bool eof(void);
	/* read a whole file into memory, allocate a buffer for it too */
	bool readallocbuffer(char **buf, unsigned int *size);
	/* close file */
	void close(void);

	/*
	 * Stream reimplementation, BUT, assumes backing store is little endian
	 * and does conversion accordingly.
	 */
	// read: supply nullptr buffer to just skip over 'size' bytes.
	int read(void *buffer, unsigned int size);
	bool read(uint8_t  *d);
	bool read(uint16_t *d);
	bool read(uint32_t *d);
	bool read(uint64_t *d);
	bool read(int8_t   *d);
	bool read(int16_t  *d);
	bool read(int32_t  *d);
	bool read(int64_t  *d);
	bool read(float    *d);
	bool read(double   *d);

	bool write(const void *buffer, unsigned int size);
	bool write(uint8_t  d);
	bool write(uint16_t d);
	bool write(uint32_t d);
	bool write(uint64_t d);
	bool write(int8_t   d);
	bool write(int16_t  d);
	bool write(int32_t  d);
	bool write(int64_t  d);
	bool write(float    d);
	bool write(double   d);

	bool ok(void);
	explicit operator bool() const { return (!error) && (m_handle != nullptr); }
private:
	PHYSFS_File *m_handle;
	bool wmode;
	bool error;
};

#endif /* _PHYSFSWRAPPER_H_ */
