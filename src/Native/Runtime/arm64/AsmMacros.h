;; Licensed to the .NET Foundation under one or more agreements.
;; The .NET Foundation licenses this file to you under the MIT license.
;; See the LICENSE file in the project root for more information.

;; OS provided macros
#include <ksarm64.h>
;; generated by the build from AsmOffsets.cpp
#include "AsmOffsets.inc"

;;
;; CONSTANTS -- INTEGER
;;
TSF_Attached                    equ 0x01
TSF_SuppressGcStress            equ 0x08
TSF_DoNotTriggerGc              equ 0x10
TSF_SuppressGcStress__OR__TSF_DoNotTriggerGC equ 0x18

;; GC type flags
GC_ALLOC_FINALIZE               equ 1
GC_ALLOC_ALIGN8_BIAS            equ 4
GC_ALLOC_ALIGN8                 equ 8

;; Note: these must match the defs in PInvokeTransitionFrameFlags defined in rhbinder.h
PTFF_SAVE_X19           equ 0x00000001
PTFF_SAVE_X20           equ 0x00000002
PTFF_SAVE_X21           equ 0x00000004
PTFF_SAVE_X22           equ 0x00000008
PTFF_SAVE_X23           equ 0x00000010
PTFF_SAVE_X24           equ 0x00000020
PTFF_SAVE_X25           equ 0x00000040
PTFF_SAVE_X26           equ 0x00000080
PTFF_SAVE_X27           equ 0x00000100
PTFF_SAVE_X28           equ 0x00000200
PTFF_SAVE_SP            equ 0x00000400
PTFF_SAVE_ALL_PRESERVED equ 0x000003FF  ;; NOTE: x19-x28
PTFF_SAVE_FP            equ 0x00080000
PTFF_SAVE_LR            equ 0x00100000
PTFF_X0_IS_GCREF        equ 0x00200000  ;; iff PTFF_SAVE_X0 : set->x0 is Object, clear->x0 is scalar
PTFF_X0_IS_BYREF        equ 0x00400000  ;; iff PTFF_SAVE_X0 : set->x0 is ByRef, clear->x0 is Object or scalar
PTFF_THREAD_ABORT       equ 0x00800000  ;; indicates that ThreadAbortException should be thrown when returning from the transition

;;
;; Rename fields of nested structs
;;
OFFSETOF__Thread__m_alloc_context__alloc_ptr        equ OFFSETOF__Thread__m_rgbAllocContextBuffer + OFFSETOF__gc_alloc_context__alloc_ptr
OFFSETOF__Thread__m_alloc_context__alloc_limit      equ OFFSETOF__Thread__m_rgbAllocContextBuffer + OFFSETOF__gc_alloc_context__alloc_limit

;;
;; IMPORTS
;;
    EXTERN RhpGcAlloc
    EXTERN RhpPublishObject
    EXTERN RhExceptionHandling_FailedAllocation
    IMPORT g_lowest_address
    IMPORT g_highest_address
    IMPORT g_ephemeral_low
    IMPORT g_ephemeral_high
    IMPORT g_card_table

;; -----------------------------------------------------------------------------
;;
;; Macro to export a pointer to an address inside a stub as a 64-bit variable
;;
    MACRO
        EXPORT_POINTER_TO_ADDRESS $Name
1
        AREA | .rdata | , ALIGN = 8, DATA, READONLY
$Name
        DCQ         $Name, 0
        EXPORT      $Name
        TEXTAREA
        ROUT

    MEND

;; -----------------------------------------------------------------------------
;;
;; Macro for indicating an alternate entry point into a function.
;;

    MACRO
        LABELED_RETURN_ADDRESS $ReturnAddressName

        ; export the return address name, but do not perturb the code by forcing alignment
$ReturnAddressName
        EXPORT $ReturnAddressName

        ; flush any pending literal pool stuff
        ROUT

    MEND

;; -----------------------------------------------------------------------------
;;
;; Macro to get a pointer to the Thread* object for the currently executing thread
;; 

__tls_array     equ 0x58    ;; offsetof(TEB, ThreadLocalStoragePointer)

    EXTERN _tls_index

    GBLS __SECTIONREL_tls_CurrentThread
__SECTIONREL_tls_CurrentThread SETS "SECTIONREL_tls_CurrentThread"

    MACRO
        INLINE_GETTHREAD $destReg, $trashReg

        ldr         $trashReg, =_tls_index
        ldr         $trashReg, [$trashReg]
        ldr         $destReg, [xpr, #__tls_array]
        ldr         $destReg, [$destReg, $trashReg lsl #3]
        ldr         $trashReg, =$__SECTIONREL_tls_CurrentThread
        ldr         $trashReg, [$trashReg]
        add         $destReg, $destReg, $trashReg
    MEND

    ;; INLINE_GETTHREAD_CONSTANT_POOL macro has to be used after the last function in the .asm file that used
    ;; INLINE_GETTHREAD. Optionally, it can be also used after any function that used INLINE_GETTHREAD
    ;; to improve density, or to reduce distance betweeen the constant pool and its use.
    MACRO
        INLINE_GETTHREAD_CONSTANT_POOL
        EXTERN tls_CurrentThread

    ;; Section relocs are 32 bits. Using an extra DCD initialized to zero for 8-byte alignment.
$__SECTIONREL_tls_CurrentThread
        DCD tls_CurrentThread
        RELOC 8, tls_CurrentThread      ;; SECREL
        DCD 0

__SECTIONREL_tls_CurrentThread SETS "$__SECTIONREL_tls_CurrentThread":CC:"_"

    MEND

;; -----------------------------------------------------------------------------
;;
;; Macro used from unmanaged helpers called from managed code where the helper does not transition immediately
;; into pre-emptive mode but may cause a GC and thus requires the stack is crawlable. This is typically the
;; case for helpers that meddle in GC state (e.g. allocation helpers) where the code must remain in
;; cooperative mode since it handles object references and internal GC state directly but a garbage collection
;; may be inevitable. In these cases we need to be able to transition to pre-meptive mode deep within the
;; unmanaged code but still be able to initialize the stack iterator at the first stack frame which may hold
;; interesting GC references. In all our helper cases this corresponds to the most recent managed frame (e.g.
;; the helper's caller).
;;
;; This macro builds a frame describing the current state of managed code.
;;
;; INVARIANTS
;; - The macro assumes it defines the method prolog, it should typically be the first code in a method and
;;   certainly appear before any attempt to alter the stack pointer.
;; - This macro uses trashReg (after its initial value has been saved in the frame) and upon exit trashReg
;;   will contain the address of transition frame.
;;

DEFAULT_FRAME_SAVE_FLAGS equ PTFF_SAVE_ALL_PRESERVED + PTFF_SAVE_SP

    MACRO
        PUSH_COOP_PINVOKE_FRAME $trashReg

        ;; The following macro variables are just some assembler magic to get the name of the 32-bit version
        ;; of $trashReg. It does it by string manipulation. Replaces something like x3 with w3.
        LCLS TrashRegister32Bit
TrashRegister32Bit SETS "$trashReg"
TrashRegister32Bit SETS "w":CC:("$TrashRegister32Bit":RIGHT:((:LEN:TrashRegister32Bit) - 1))

        PROLOG_SAVE_REG_PAIR   fp, lr, #-0x80!      ;; Push down stack pointer and store FP and LR

        ;; 0x10 bytes reserved for Thread* and flags

        ;; Save callee saved registers
        PROLOG_SAVE_REG_PAIR   x19, x20, #0x20
        PROLOG_SAVE_REG_PAIR   x21, x22, #0x30
        PROLOG_SAVE_REG_PAIR   x23, x24, #0x40
        PROLOG_SAVE_REG_PAIR   x25, x26, #0x50
        PROLOG_SAVE_REG_PAIR   x27, x28, #0x60

        ;; Save the value of SP before stack allocation to the last slot in the frame (slot #15)
        add                    $trashReg, sp, #0x80
        str                    $trashReg, [sp, #0x70]

        ;; Record the bitmask of saved registers in the frame (slot #3)
        mov                    $TrashRegister32Bit, #DEFAULT_FRAME_SAVE_FLAGS
        str                    $TrashRegister32Bit, [sp, #0x18]

        mov $trashReg, sp
    MEND

;; Pop the frame and restore register state preserved by PUSH_COOP_PINVOKE_FRAME
    MACRO
        POP_COOP_PINVOKE_FRAME

        EPILOG_RESTORE_REG_PAIR   x19, x20, #0x20
        EPILOG_RESTORE_REG_PAIR   x21, x22, #0x30
        EPILOG_RESTORE_REG_PAIR   x23, x24, #0x40
        EPILOG_RESTORE_REG_PAIR   x25, x26, #0x50
        EPILOG_RESTORE_REG_PAIR   x27, x28, #0x60
        EPILOG_RESTORE_REG_PAIR   fp, lr, #0x80!
    MEND
