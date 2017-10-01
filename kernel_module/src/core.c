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


// Project 1: Rishabh Rathore, UNITY-ID: rrathor; Aasheesh Tandon, UNITY-ID: atandon;


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

extern struct miscdevice npheap_dev;

struct object_store {
	struct list_head head_of_list;  // kernel's list structure 
	//struct mutex resource_lock;
	__u64 size;
	__u64 offset;
	unsigned long virt_addr;
};

struct list_head myobjectlist;		// global list head
extern struct mutex list_lock;             // lock for list modifications

// Print nodes of linked list
void list_print(void) {
    struct list_head *pos = NULL;   
    printk("\nPrinting contents of the linked list:\n");

    list_for_each(pos, &myobjectlist) {
        printk("Size:%llu,\nOffset:%llu,\nVirtual Address:%lu\n\n\n",((struct object_store *)pos)->size,\
            ((struct object_store *)pos)->offset, ((struct object_store *)pos)->virt_addr);
    }
}

// Searches for the desired object-id
struct object_store *get_object(__u64 offset) {
    
    struct list_head *pos = NULL;
    struct object_store *res = NULL;
    printk("Search the list for offset: %llu using list_for_each()\n", offset);
    mutex_lock(&list_lock);
    list_for_each(pos, &myobjectlist){
        res = (struct object_store *) pos;
        if(res->offset == offset) {
            //mutex_unlock(&list_lock);
            printk(" offset: %llu found in list\n", offset);
            return res;
        }
    }
    //mutex_unlock(&list_lock);
    printk("%llu not found in list\n", offset);
    return NULL;
}

// Inserts a Node at tail of Doubly linked list
struct object_store *insert_object(__u64 offset) {

    
    struct object_store *new = NULL;
    printk("Inside insert_object for offset: %llu\n", offset);
    new = (struct object_store*)kmalloc(sizeof(struct object_store),GFP_KERNEL);
    memset(new, 0, sizeof(struct object_store));
    INIT_LIST_HEAD(&new->head_of_list);
    //mutex_init(&new->resource_lock);
	new->offset = offset;
    
	//mutex_lock(&list_lock);
    list_add_tail(&new->head_of_list, &myobjectlist);
    //mutex_unlock(&list_lock);
    printk("Leaving insert_object \n");
    list_print();
    return new;

}

// ---------------> How to call this function ------------>      /* delete_object (offset); */

// Deletes an entry from the list 
void delete_object(__u64 offset) {
	
	struct list_head *pos = NULL ;
    struct list_head *temp_store = NULL;
	printk("deleting the list using list_for_each_safe()\n");	
	list_for_each_safe(pos, temp_store, &myobjectlist) {

		if(((struct object_store *)pos)->offset == offset) {
			list_del(pos);
		    kfree(pos);
		    return;	
		}
	}	 
}

// Deletes the entire linked-list
void delete_list(void) {
    
    struct list_head *pos = NULL; 
    struct list_head *temp_store = NULL;
    unsigned long obj_virt_addr = 0;
    printk("deleting the whole linked-list data structure\n");
    
/*5*/    list_for_each_safe(pos, temp_store, &myobjectlist) {
            obj_virt_addr = ((struct object_store *)pos)->virt_addr;
            kfree((void *)obj_virt_addr);
            //pos->virt_addr = 0;
            //pos->size=0;
            list_del(pos);
            kfree(pos);	
    }	
    //myobjectlist->head_of_list->next = myobjectlist->head_of_list->prev;
}


int npheap_mmap(struct file *filp, struct vm_area_struct *vma)
{
    __u64 size = 0;
    __u64 offset = 0;
    struct object_store *object = NULL; 
    unsigned long start_address = 0;
    unsigned long obj_phy_addr = 0;
    printk(KERN_INFO "enter npheap_mmap\n");
    // store properties of vma 
    offset = vma->vm_pgoff;
    size = vma->vm_end - vma->vm_start;
    start_address = vma->vm_start;
    

    // check if object exists in list
    object = get_object(offset);
    if(object == NULL){
        printk("calling insert from mmap with offset : %llu \n",offset);
        //create node in link list
        object = insert_object(offset);
    }
    
    if(!object->virt_addr){
        printk("calling kmalloc with offset %llu \n", offset);
        object->virt_addr = (unsigned long) (void *)kmalloc(vma->vm_end - vma->vm_start, GFP_KERNEL);
        printk("after kmalloc with offset %llu \n", offset);
        memset((void *)object->virt_addr, 0, size);
        object->size = size;
    }
    if(!object->virt_addr){
        // error in allocation of memory
        return -EAGAIN;
    }
    //get physical address from virtual address
    obj_phy_addr = __pa(object->virt_addr) >> PAGE_SHIFT;
    // object_store = insert_object();
    // make entry in page table of process
    printk("calling remap_pfn_range with offset %llu \n", offset);
    if (remap_pfn_range(vma, start_address, __pa(object->virt_addr) >> PAGE_SHIFT,
        size,
        vma->vm_page_prot))
        return -EAGAIN;
    printk(KERN_INFO "exit npheap_mmap\n");
    return 0;
}

int npheap_init(void)
{
    int ret;
    if ((ret = misc_register(&npheap_dev)))
        printk(KERN_ERR "Unable to register \"npheap\" misc device\n");
    else
        printk(KERN_ERR "\"npheap\" misc device installed\n");
        /* MOVE TO npheap_init() ---------------------------> */ 
        mutex_init(&list_lock);
        INIT_LIST_HEAD(&myobjectlist);

    return ret;
}

void npheap_exit(void)
{   
    // free all nodes of list and memory
    list_print();
    delete_list();
    list_print();
    misc_deregister(&npheap_dev);
    
}