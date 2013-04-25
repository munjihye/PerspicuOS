/*===- mmu.h - SVA Execution Engine  =-------------------------------------===
 * 
 *                        Secure Virtual Architecture
 *
 * This file was developed by the LLVM research group and is distributed under
 * the University of Illinois Open Source License. See LICENSE.TXT for details.
 * 
 *===----------------------------------------------------------------------===
 *
 * Copyright (c) 2003 Peter Wemm.
 * Copyright (c) 1991 Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department and William Jolitz of UUNET Technologies Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Derived from hp300 version by Mike Hibler, this version by William
 * Jolitz uses a recursive map [a pde points to the page directory] to
 * map the page tables using the pagetables themselves. This is done to
 * reduce the impact on kernel virtual memory for lots of sparse address
 * space, and to reduce the cost of memory to each process.
 *
 *  from: hp300: @(#)pmap.h 7.2 (Berkeley) 12/16/90
 *  from: @(#)pmap.h    7.4 (Berkeley) 5/12/91
 * $FreeBSD: release/9.0.0/sys/amd64/include/pmap.h 222813 2011-06-07 08:46:13Z attilio $
 *
 *===----------------------------------------------------------------------===
 */


#ifndef SVA_MMU_H
#define SVA_MMU_H

#include <sys/types.h>
#include "mmu_types.h"

/* Size of the smallest page frame in bytes */
static const uintptr_t X86_PAGE_SIZE = 4096u;

/* Start and end addresses of the secure memory */
#define SECMEMSTART 0xffffff0000000000u
#define SECMEMEND   0xffffff8000000000u

/* Mask to get the proper number of bits from the virtual address */
static const uintptr_t vmask = 0x0000000000000fffu;

/*
 * Offset into the PML4E at which the mapping for the secure memory region can
 * be found.
 */
static const uintptr_t secmemOffset = ((SECMEMSTART >> 39) << 3) & vmask;

/*
 * Assert macro for SVA
 */
#define SVA_ASSERT(res,string) if(!res) panic(string)

/*
 *****************************************************************************
 * Define structures used in the SVA MMU interface.
 *****************************************************************************
 */

/*
 * Frame usage constants
 */
/* Enum representing the four page types */
enum page_type_t {
    PG_UNUSED,
    PG_L1,          /* Defines a page being used as an L1 PTP */
    PG_L2,          /* Defines a page being used as an L2 PTP */
    PG_L3,          /* Defines a page being used as an L3 PTP */
    PG_L4,          /* Defines a page being used as an L4 PTP */
    PG_LEAF,    /* TODO: only a temporary listing */
    PG_TKDATA,  /* TODO don't care about */    /* Defines a kernel data page */
    PG_TUDATA,  /* TODO don't care about */    /* Defines a user data page */
    PG_CODE,        /* Defines a code page */
    PG_STACK,   /* TODO don't care about */
    PG_IO,      /* TODO don't care about */
    PG_SVA,         /* Defines an SVA system page */
    PG_SECMEM,      /* Defines a secure page */
};

/* Mask to get the address bits out of a PTE, PDE, etc. */
static const uintptr_t addrmask = 0x000ffffffffff000u;

/*
 * Struct: page_desc_t
 *
 * Description:
 *  There is one element of this structure for each physical page of memory
 *  in the system.  It records information about the physical memory (and the
 *  data stored within it) that SVA needs to perform its MMU safety checks.
 */
typedef struct page_desc_t {
    /* Type of frame */
    enum page_type_t type;

    /* State of page: value of != 0 is active and 0 is inactive */
    unsigned active : 1;

    /* Number of times a page is mapped */
    unsigned count : 12;

    /* Number of times a page is used as a l1 page */
    unsigned l1_count : 5;

    /* Number of times a page is used as a l2 page (unused in non-PAE) */
    unsigned l2_count : 2;

    /* Is this page a L1 in user-space? */
    unsigned l1_user : 1;

    /* Is this page a L1 in kernel-space? */
    unsigned l1_kernel : 1;

    /* Is this page a user page? */
    unsigned user : 1;
} page_desc_t;


/*
 * ===========================================================================
 * BEGIN FreeBSD CODE BLOCK
 *
 * $FreeBSD: release/9.0.0/sys/amd64/include/pmap.h 222813 2011-06-07 08:46:13Z attilio $
 * ===========================================================================
 */

/* MMU Flags ---- Intel Nomenclature ---- */
#define PG_V        0x001   /* P    Valid               */
#define PG_RW       0x002   /* R/W  Read/Write          */
#define PG_U        0x004   /* U/S  User/Supervisor     */
#define PG_NC_PWT   0x008   /* PWT  Write through       */
#define PG_NC_PCD   0x010   /* PCD  Cache disable       */
#define PG_A        0x020   /* A    Accessed            */
#define PG_M        0x040   /* D    Dirty               */
#define PG_PS       0x080   /* PS   Page size (0=4k,1=2M)   */
#define PG_PTE_PAT  0x080   /* PAT  PAT index           */
#define PG_G        0x100   /* G    Global              */
#define PG_AVAIL1   0x200   /*    / Available for system    */
#define PG_AVAIL2   0x400   /*   <  programmers use     */
#define PG_AVAIL3   0x800   /*    \                     */
#define PG_PDE_PAT  0x1000  /* PAT  PAT index           */
#define PG_NX       (1ul<<63) /* No-execute             */

/* Various interpretations of the above */
#define PG_W        PG_AVAIL1   /* "Wired" pseudoflag */
#define PG_MANAGED  PG_AVAIL2
#define PG_FRAME    (0x000ffffffffff000ul)
#define PG_PS_FRAME (0x000fffffffe00000ul)
#define PG_PROT     (PG_RW|PG_U)    /* all protection bits . */
#define PG_N        (PG_NC_PWT|PG_NC_PCD)   /* Non-cacheable */

/*
 * ===========================================================================
 * END FreeBSD CODE BLOCK
 * ===========================================================================
 */

extern uintptr_t getPhysicalAddr (void * v);
extern pml4e_t mapSecurePage (unsigned char * v, uintptr_t paddr);
extern void unmapSecurePage (unsigned char * v);

/*
 *****************************************************************************
 * SVA Implementation Function Prototypes
 *****************************************************************************
 */
void init_mmu(void);
void init_leaf_page_from_mapping(page_entry_t val);

/*
 *****************************************************************************
 * SVA utility functions needed by multiple compilation units
 *****************************************************************************
 */

/*
 * Description:
 *  Given a page table entry value, return the page description associate with
 *  the frame being addressed in the mapping.
 *
 * Inputs:
 *  mapping: the mapping with the physical address of the referenced frame
 *
 * Return:
 *  Pointer to the page_desc for this frame
 */
page_desc_t * getPageDescPtr(unsigned long mapping);

/*
 * Function: getVirtual()
 *
 * Description:
 *  This function takes a physical address and converts it into a virtual
 *  address that the SVA VM can access.
 *
 *  In a real system, this is done by having the SVA VM create its own
 *  virtual-to-physical mapping of all of physical memory within its own
 *  reserved portion of the virtual address space.  However, for now, we'll
 *  take advantage of FreeBSD's direct map of physical memory so that we don't
 *  have to set one up.
 */
static inline unsigned char *
getVirtual (uintptr_t physical) {
  return (unsigned char *)(physical | 0xfffffe0000000000u);
}

/*
 * Function: get_pagetable()
 *
 * Description:
 *  Return a physical address that can be used to access the current page table.
 */
static inline unsigned char *
get_pagetable (void) {
  /* Value of the CR3 register */
  uintptr_t cr3;

  /* Get the page table value out of CR3 */
  __asm__ __volatile__ ("movq %%cr3, %0\n" : "=r" (cr3));

  /*
   * Shift the value over 12 bits.  The lower-order 12 bits of the page table
   * pointer are assumed to be zero, and so they are reserved or used by the
   * hardware.
   */
  return (unsigned char *)((((uintptr_t)cr3) & 0x000ffffffffff000u));
}

/*
 *****************************************************************************
 * MMU declare, update, and verification helper routines
 *****************************************************************************
 */

#if 0
/*
 * Function: isInvalidMappingOrder
 *
 * Description: This function verifies that the mapping being inserted is
 *      correct by checking that the va address being mapped into this
 *      particular page table entry should be in that address location.
 *
 *      To prove that we have a valid ordering we can just verify that the
 *      update is being applied to the correct page table (at any level) and
 *      that the particuler page table entry (at any level) is at the index
 *      defined by the virtual address.
 *
 * Inputs:
 *  
 */
static inline int 
isInvalidMappingOrder (page_desc_t pgDesc) {
    /* 
     * Check that we have the correct page table for the given VA and teh
     * active set of MMU mappings.
     */
    if (updatePageFrame != expectedPageFrameForVA(va)) {
        return false;
    }

    /*
     * Check that we have the correct index into the given page table from the
     * virtual address
     */
    else if (pteIndex != expectedPTEIndex(va)) {
        return false;
    } else {
        return true;
    }
}
#endif

#if 0
static inline uintptr_t
pageVA(page_desc_t pg){
    return getVirtual(pg.physAddress);    
}
#endif

/*
 * Function: readOnlyPage
 *
 * Description: 
 *  This function determines whether or not the given page descriptor
 *  references a page that should be marked as read only. We set this for pages
 *  of type: l4,l3,l2,l1, code, and TODO: is this all of them?
 *
 * Inputs:
 *  pg  - page descriptor to check
 *
 * Return:
 *  - 0 denotes not a read only page
 *  - 1 denotes a read only page
 */
static inline int
readOnlyPage(page_desc_t pg){
    return  pg.type == PG_L4    ||
            pg.type == PG_L3    ||
            pg.type == PG_L2    ||
            pg.type == PG_L1    ||
            pg.type == PG_CODE  ||
            pg.type == PG_SVA 
            ;
}

/*
 *****************************************************************************
 * Page descriptor query functions
 *****************************************************************************
 */

/* State whether this kernel virtual address is in the secure memory range */
static inline int isSecureMemVA(uintptr_t va)
    { return va > SECMEMSTART && va < SECMEMEND; }

/* Description: Return whether the page is active or not */
static inline int pgIsActive (page_desc_t page) { return page.type != PG_UNUSED ; } 

/* 
 * The following functions query the given page descriptor for type attributes.
 */
static inline int isFramePg (page_desc_t page) { 
    return page.type == PG_TKDATA   ||       /* Defines a kernel data page */
           page.type == PG_TUDATA   ||       /* Defines a user data page */
           page.type == PG_FRAME             /* Defines a code page */
        ;
}

static inline int isL1Pg (page_desc_t page) { return page.type == PG_L1; }
static inline int isL2Pg (page_desc_t page) { return page.type == PG_L2; }
static inline int isL3Pg (page_desc_t page) { return page.type == PG_L3; }
static inline int isL4Pg (page_desc_t page) { return page.type == PG_L4; }
static inline int isSVAPg (page_desc_t page) { return page.type == PG_SVA; }
static inline int isSecureMemPG(page_desc_t page)
    { return page.type == PG_SECMEM; }

#endif

