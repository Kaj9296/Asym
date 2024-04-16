#include "ramfs.h"

#include <string.h>

#include "vfs/vfs.h"
#include "vfs/utils/utils.h"
#include "tty/tty.h"
#include "heap/heap.h"
#include "sched/sched.h"
#include "utils/utils.h"

static Filesystem ramfs;
static RamDir* root;

static RamFile* ram_dir_find_file(RamDir* dir, const char* filename)
{
    RamFile* file = dir->firstFile;
    while (file != NULL)
    {
        if (vfs_compare_names(file->name, filename))
        {
            return file;
        }
        else
        {
            file = file->next;
        }
    }

    return NULL;
}

static RamDir* ram_dir_find_dir(RamDir* dir, const char* dirname)
{
    RamDir* child = dir->firstChild;
    while (child != NULL)
    {
        if (vfs_compare_names(child->name, dirname))
        {
            return child;
        }
        else
        {
            child = child->next;
        }
    }

    return NULL;
}

static RamDir* ramfs_traverse(const char* path)
{
    RamDir* dir = root;
    const char* dirname = vfs_first_dir(path);
    while (dirname != NULL)
    {
        dir = ram_dir_find_dir(dir, dirname);
        if (dir == NULL)
        {
            return NULL;
        }

        dirname = vfs_next_dir(dirname);
    }

    return dir;
}

static RamFile* ramfs_find_file(const char* path)
{
    RamDir* parent = ramfs_traverse(path);
    if (parent == NULL)
    {
        return NULL;
    }

    const char* filename = vfs_basename(path);
    if (filename == NULL)
    {
        return NULL;
    }

    return ram_dir_find_file(parent, filename);
}

/*static RamFile* ramfs_find_dir(const char* path)
{
    RamDir* parent = ramfs_traverse(path);
    if (parent == NULL)
    {
        return NULL;
    }

    const char* dirname = vfs_basename(path);
    if (dirname == NULL)
    {
        return NULL;
    }

    return ram_dir_find_dir(parent, dirname);
}*/

uint64_t ramfs_read(RamfsFile* file, void* buffer, uint64_t count)
{
    uint64_t pos = file->base.position;
    uint64_t readCount = pos <= file->ramFile->size ? MIN(count, file->ramFile->size - pos) : 0;
    file->base.position += readCount;

    memcpy(buffer, file->ramFile->data + pos, readCount);

    return readCount;
}

uint64_t ramfs_seek(RamfsFile* file, int64_t offset, uint8_t origin)
{    
    uint64_t position;
    switch (origin)
    {
    case SEEK_SET:
    {
        position = offset;
    }
    break;
    case SEEK_CUR:
    {
        position = file->base.position + offset;
    }
    break;
    case SEEK_END:
    {
        position = file->ramFile->size - offset;
    }
    break;
    default:
    {
        position = 0;
    }
    break;
    }

    file->base.position = MIN(position, file->ramFile->size);
    return position;
}

File* ramfs_open(RamfsVolume* volume, const char* path)
{
    RamFile* ramFile = ramfs_find_file(path);
    if (ramFile == NULL)
    {
        return NULLPTR(EPATH);
    }

    RamfsFile* file = kmalloc(sizeof(RamfsFile));
    file_init(&file->base, (Volume*)volume);
    file->base.read = (void*)ramfs_read;
    file->base.seek = (void*)ramfs_seek;
    file->ramFile = ramFile;

    return &file->base;
}

Volume* ramfs_mount(Filesystem* fs)
{
    RamfsVolume* volume = kmalloc(sizeof(RamfsVolume));
    volume_init(&volume->base, fs);
    volume->base.open = (void*)ramfs_open;

    return &volume->base;
}

void ramfs_init(RamDir* ramRoot)
{
    tty_start_message("Ramfs initializing");

    root = ramRoot;

    memset(&ramfs, 0, sizeof(Filesystem));
    ramfs.name = "ramfs";
    ramfs.mount = ramfs_mount;

    if (vfs_mount('B', &ramfs) == ERR)
    {
        tty_print("Failed to mount ramfs");
        tty_end_message(TTY_MESSAGE_ER);
    }

    tty_end_message(TTY_MESSAGE_OK);
}