/* tc-riscv.h -- header file for tc-riscv.c.*/

#ifndef TC_RISCV
#define TC_RISCV

#include "write.h"

#define TARGET_BYTES_BIG_ENDIAN 0

#define TARGET_ARCH bfd_arch_riscv

#define WORKING_DOT_WORD	1
#define LOCAL_LABELS_FB 	1

/* Symbols named FAKE_LABEL_NAME are emitted when generating DWARF, so make
   sure FAKE_LABEL_NAME is printable.  It still must be distinct from any
   real label name.  So, append a space, which other labels can't contain.  */
#undef FAKE_LABEL_NAME  /* override prev definition in write.h.  */
#define FAKE_LABEL_NAME ".L0 "
/* Changing the special character in FAKE_LABEL_NAME requires changing
   FAKE_LABEL_CHAR too.  */
#undef FAKE_LABEL_CHAR
#define FAKE_LABEL_CHAR ' '

#define md_after_parse_args() riscv_after_parse_args()
extern void riscv_after_parse_args (void);

#define md_section_align(seg,size)        (size)
#define md_undefined_symbol(name)         (0)
#define md_operand(x)

extern void riscv_handle_align (fragS *);
#define HANDLE_ALIGN riscv_handle_align

#define md_cgen_record_fixup_exp riscv_record_fixup_exp

/* The ISA of the target may change based on command-line arguments.  */
#define TARGET_FORMAT riscv_target_format()
extern const char * riscv_target_format (void);

#define md_after_parse_args() riscv_after_parse_args()
extern void riscv_after_parse_args (void);

#define md_parse_long_option(arg) riscv_parse_long_option (arg)
extern int riscv_parse_long_option (const char *);

/* Let the linker resolve all the relocs due to relaxation.  */
#define tc_fix_adjustable(fixp) 0
#define md_allow_local_subtract(l,r,s) 0

/* Call md_pcrel_from_section(), not md_pcrel_from().  */
extern long md_pcrel_from_section (struct fix *, segT);
#define MD_PCREL_FROM_SECTION(FIX, SEC) md_pcrel_from_section (FIX, SEC)

/* For 12 vs 20 bit branch selection.  */
extern const struct relax_type md_relax_table[];
#define TC_GENERIC_RELAX_TABLE md_relax_table

/* By default the max length of an instruction when relaxing is equal to
   CGEN_MAX_INSN_SIZE (which is currently 4). However, the relaxed form
   of conditional branches is expanded to a branch+jump sequence, which
   takes up 8 bytes. If the default behavior is used, then it seems that
   fragments can end up without enough space allocated for the relaxed
   instruction.  */
#define TC_CGEN_MAX_RELAX(insn, len) 8

extern void riscv_pop_insert (void);
#define md_pop_insert()         riscv_pop_insert ()

#endif /* TC_RISCV */
