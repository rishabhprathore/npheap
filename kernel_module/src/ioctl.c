//////////////////////////////////////////////////////////////////////
//                             North Carolina State University
//
//
//
//                             Copyright 2016
//
////////////////////////////////////////////////////////////////////////
//
// This program is free software; you can redistribute it and/or modify it
// under the terms and conditions of the GNU General Public License,
// version 2, as published by the Free Software Foundation.
//
// This program is distributed in the hope it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
// more details.
//
// You should have received a copy of the GNU General Public License along with
// this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
//
////////////////////////////////////////////////////////////////////////
//
//   Author:  Hung-Wei Tseng
//
//   Description:
//     Skeleton of NPHeap Pseudo Device
//
////////////////////////////////////////////////////////////////////////

#include "npheap.h"

#include <asm/uaccess.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/poll.h>
#include <linux/mutex.h>
#include <linux/list.h>


struct object_store {
	struct list_head head_of_list;  // kernel's list structure 
	struct mutex resource_lock;
	__u64 size;
	__u64 offset;
	unsigned long virt_addr;
};

struct object_store *insert_object(__u64 offset);
void delete_object(__u64 offset);
struct object_store *get_object(__u64 offset);

// If exist, return the data.
long npheap_lock(struct npheap_cmd __user *user_cmd){
    struct npheap_cmd k_cmd;
    struct object_store *object = NULL;
    if (copy_from_user(&k_cmd, (void __user *) user_cmd, sizeof(struct npheap_cmd))){
        return -EFAULT;
    }
    __u64 offset = k_cmd.offset/PAGE_SIZE;
    object = get_object(offset);
    if (!object)
    {
        // create a new object if does not exist
        object = insert_object(offset);
        if (!object)
        // object creation failed
            return -EAGAIN;
    }

    mutex_lock(&object->resource_lock);
    return 0;
}     

long npheap_unlock(struct npheap_cmd __user *user_cmd)
{
    struct npheap_cmd  k_cmd;
    struct object_store *object = NULL;
    __u64 offset = 0;

    if (copy_from_user(&k_cmd, (void __user *) user_cmd, sizeof(struct npheap_cmd)))
        return -EFAULT;

    __u64 offset = k_cmd.offset/PAGE_SIZE;  

    object = get_object(offset);
    if (!object)
    {   
        // object should exist
        return -EFAULT;
    }

    mutex_unlock(&object->resource_lock);
    return 0;
}

long npheap_getsize(struct npheap_cmd __user *user_cmd)
{
    struct npheap_cmd k_cmd;
    struct object_store *object = NULL;
    __u64 offset = 0;

    if (copy_from_user(&k_cmd, (void __user *) user_cmd, sizeof(struct npheap_cmd)))
        return -EFAULT;

    __u64 offset = k_cmd.offset/PAGE_SIZE;

    object = get_object(offset);
    if (!object)
    {   
        // object should exist
        return -EFAULT;
    }
    k_cmd.size = object->size;
    if (copy_to_user((void __user *) user_cmd, &k_cmd, sizeof(struct npheap_cmd)))
        return -EFAULT;
    return object->size;
}
long npheap_delete(struct npheap_cmd __user *user_cmd)
{
    struct npheap_cmd  k_cmd;
    struct object_store *object = NULL;
    __u64 offset = 0;

    if (copy_from_user(&k_cmd, (void __user *) user_cmd, sizeof(struct npheap_cmd)))
        return -EFAULT;

    __u64 offset = k_cmd.offset/PAGE_SIZE;

    object = get_object(offset);
    if (!object)
    {   
        // object should exist
        return -EFAULT;
    }
    if(object->virt_addr !=0){
        kfree((void *) object->virt_addr);
        object->virt_addr = 0;
        object->size = 0;
    }
    return 0;
}

long npheap_ioctl(struct file *filp, unsigned int cmd,
                                unsigned long arg)
{
    switch (cmd) {
    case NPHEAP_IOCTL_LOCK:
        return npheap_lock((void __user *) arg);
    case NPHEAP_IOCTL_UNLOCK:
        return npheap_unlock((void __user *) arg);
    case NPHEAP_IOCTL_GETSIZE:
        return npheap_getsize((void __user *) arg);
    case NPHEAP_IOCTL_DELETE:
        return npheap_delete((void __user *) arg);
    default:
        return -ENOTTY;
    }
}
