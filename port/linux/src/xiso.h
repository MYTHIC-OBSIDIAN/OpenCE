/*
XISO.H

The maps folder out of an Xbox disc image (xiso.c).
*/

#ifndef __HALO_LINUX_XISO_H
#define __HALO_LINUX_XISO_H

/* called as the copy goes: the file being copied, and the bytes copied of
all the files' */
typedef void (*xiso_progress_proc)(void *context, const char *file, unsigned long long done,
	unsigned long long total);

/* copies the image's maps folder to <destination>/maps; returns nonzero on
success, or 0 with the reason (for the player) in error. Called from any
thread */
int xiso_extract_maps(const char *image_path, const char *destination, xiso_progress_proc progress, void *context,
	char *error, int error_size);

/* copies the image's videos (its bink folder's .bik files: the opening
titles, intro.bik, the credits, credits.bik, the attract movies) to
<destination>/bink, keeping those already copied whole; returns nonzero on
success, or 0 with the reason in error */
int xiso_extract_videos(const char *image_path, const char *destination, char *error, int error_size);

/* writes the loading screen's picture, <maps_folder>/loading.tga, from
source_path: the disc's default.xbe itself, or a disc image (its root's
default.xbe); returns nonzero if it was written, or 0 with the reason in
error */
int xiso_extract_loading_picture(const char *source_path, const char *maps_folder, char *error, int error_size);

#endif
