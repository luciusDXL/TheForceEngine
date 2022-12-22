#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>
#include <TFE_System/system.h>
#include "fileutil.h"
#include "filestream.h"

// implement TFE FileUtil for Linux and compatibles.
namespace FileUtil
{
	void readDirectory(const char* dir, const char* ext, FileList& fileList)
	{
		struct dirent *de;
		int el, dl;
		char *dn;
		DIR *d;

		d = opendir(dir);
		if (!d) {
			TFE_System::logWrite(LOG_ERROR, "readDirectory", "opendir(%s) failed with %d\n", dir, errno);
			return;
		}

		el = strlen(ext);
		while (NULL != (de = readdir(d))) {
			dn = de->d_name;
			dl = strlen(dn);
			// only regular files and symlinks accepted
			if ((de->d_type != DT_REG) && (de->d_type != DT_LNK))
				continue;
			// skip dotfiles, dotdirs, too short and files without extensions.
			if ((dl < (el + 2)) || (dn[0] == '.'))
				continue;
			if (dn[dl - el - 1] != '.')
				continue;
			// does the extension match (unix: case sensitive!)
			if (0 != strncasecmp(dn + dl - el, ext, el))
				continue;
			// we have a winner
			fileList.push_back(string(dn));
		}
		closedir(d);
	}

	void readSubdirectories(const char* dir, FileList& dirList)
	{
		char *dn, fp[PATH_MAX];
		struct dirent *de;
		int l1, l2;
		DIR *d;

		d = opendir(dir);
		if (!d) {
			TFE_System::logWrite(LOG_ERROR, "readSubdirctories", "opendir(%s) failed with %d\n", dir, errno);
			return;
		}
		l1 = strlen(dir);
		while (NULL != (de = readdir(d))) {
			if (de->d_type != DT_DIR)
				continue;

			dn = de->d_name;
			if (!strcmp(dn, ".") || !strcmp(dn, ".."))
				continue;

			l2 = l1 + strlen(dn) + 1;
			memset(fp, 0, PATH_MAX);
			snprintf(fp, l2, "%s%s/", dir, dn);
			dirList.push_back(string(fp));
		}
		closedir(d);
	}

	bool makeDirectory(const char* dir)
	{
		if (!mkdir(dir, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) || errno == EEXIST)
			return true;
		return false;
	}

	void getCurrentDirectory(char* dir)
	{
		char *err = getcwd(dir, TFE_MAX_PATH);
		/* should not happen, but just to be sure */
		if (err == NULL) {
			TFE_System::logWrite(LOG_ERROR, "getCurrentDirectory", "getcwd() error %d", errno);
			memset(dir, 0, TFE_MAX_PATH);
			/* substitute home */
			strcpy(dir, "~");
		}
	}

	void getExecutionDirectory(char* dir)
	{
		char exe[PATH_MAX], *c;
		ssize_t r;

		memset(dir, 0, TFE_MAX_PATH);
		memset(exe, 0, PATH_MAX);
		sprintf(exe, "/proc/%d/exe", getpid());
		r = readlink(exe, dir, PATH_MAX);
		if (r < 0) {
			TFE_System::logWrite(LOG_CRITICAL, "getExecutionDirectory", "readlink %s failed with %d", exe, errno);
			return;
		}

		// kill trailing slash
		c = strrchr(dir, '/');
		if (c)
			*c = 0;
	}

	void setCurrentDirectory(const char* dir)
	{
		int ret = chdir(dir);
		if (ret) {
			TFE_System::logWrite(LOG_ERROR, "chdir(%s) failed with %d", dir, errno);
		}
	}

	void getFilePath(const char* filename, char* path)
	{
		char *tmpbuf, *dn;
		int l;

		memset(path, 0, TFE_MAX_PATH);
		if (!filename)
			return;
		l = strlen(filename);
		if (!l)
			return;

		// dirname() messes with the input buffer - make a copy.
		tmpbuf = (char *)malloc(l + 1);
		if (!tmpbuf)
			return;			
		strcpy(tmpbuf, filename);
		dn = dirname(tmpbuf);
		sprintf(path, "%s/", dn);
		free(tmpbuf);
	}

	void getFileExtension(const char* filename, char* extension)
	{
		const char *c;

		memset(extension, 0, TFE_MAX_PATH);
		c = strrchr(filename, '.');
		if (c) {
			strcpy(extension, c + 1);
		}
	}

	void getFileNameFromPath(const char* path, char* name, bool includeExt)
	{
		char *tmpbuf, *fn, *c;

		// basename() messes with the input buffer - make a copy.
		memset(name, 0, TFE_MAX_PATH);
		tmpbuf = (char *)malloc(TFE_MAX_PATH + 1);
		if (!tmpbuf)
			return;
		strcpy(tmpbuf, path);
		fn = basename(tmpbuf);
		strcpy(name, fn);
		if (!includeExt) {
			// find the last dot and null it.
			c = strrchr(name, '.');
			if (c)
				*c = 0;
		}
		free(tmpbuf);
	}

	void copyFile(const char* srcFile, const char* dstFile)
	{
		ssize_t rd, wr;
		char buf[1024];
		int src, dst;

		src = open(srcFile, O_RDONLY);
		if (!src)
			return;
		dst = open(dstFile, O_WRONLY | O_CREAT, 00644);
		if (!dst) {
			close(src);
			return;
		}

		do {
			rd = read(src, buf, 1024);
			if (rd <= 0)
				break;
			wr = write(dst, buf, rd);
			if (wr < 0 || wr != rd)
				break;
		} while (rd > 0);
		close(dst);
		close(src);
	}

	void deleteFile(const char* srcFile)
	{
		int ret = unlink(srcFile);
		if (ret) {
			TFE_System::logWrite(LOG_WARNING, "deleteFile", "unlink(%s) failed with %d\n", srcFile, errno);
		}
	}

	bool directoryExits(const char* path)
	{
		struct stat st;
		int ret = stat(path, &st);
		if (ret < 0)
			return false;

		return (st.st_mode & S_IFDIR) != 0;
	}

	bool exists(const char *path)
	{
		struct stat st;
		int ret = stat(path, &st);

		return ret ? (errno != ENOENT) : true;
	}

	u64 getModifiedTime( const char* path )
	{
		struct timespec tslw;
		struct stat st;
		u64 mtim;

		int ret = stat(path, &st);
		if (ret) {
			TFE_System::logWrite(LOG_WARNING, "getModifiedTime", "stat(%s) failed with %d\n", path, errno);
			return (u64)-1;  // revisit
		}
		tslw = st.st_mtim;
		mtim = (u64)tslw.tv_sec * 10000 + (u64) ((double)tslw.tv_nsec / 100.0);

		return mtim;
	}

	void fixupPath(char* path)
	{
		int len = strlen(path);
		for (int i = 0; i < len; i++)
		{
			if (path[i] == '\\')
			{
				path[i] = '/';
			}
		}
	}

	void convertToOSPath(const char* path, char* pathOS)
	{
		int len = strlen(path);
		for (int i = 0; i < len; i++)
		{
			if (path[i] == '\\')
			{
				pathOS[i] = '/';
			}
			else
			{
				pathOS[i] = path[i];
			}
		}
		pathOS[len] = 0;
	}
}
