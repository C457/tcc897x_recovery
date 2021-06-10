#ifndef __QB_FUNCTION_H__
#define __QB_FUNCTION_H__

#include <cutils/qb_manager.h>		// For QuickBoot Definitions. ( Properties... )

#define QB_OPT_DATA     0x1
#define QB_OPT_CACHE    0x2
#define QB_OPT_SNAPSHOT 0x4

int do_qb_prebuilt(Device* device);
int extract_qb_data_auto(const char* path, int extract_qb_data_only);
int quickboot_factory_reset_opt(int sel); // USERDATA : 0x1, CACHE : 0x2, SNAPSHOT, 0x4
int write_sparse_image_no_zip(const char *src_path, const char *dest_path);

#endif //__QB_FUNCTION_H__
