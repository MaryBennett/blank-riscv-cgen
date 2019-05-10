/* tc-riscv.c -- RISC-V assembler  */

#include "as.h"
#include "safe-ctype.h"
#include "subsegs.h"
#include "symcat.h"
#include "opcodes/riscv-desc.h"
#include "opcodes/riscv-opc.h"
#include "cgen.h"
#include "struc-symbol.h"
#include "elf/riscv.h"

#include "dw2gencfi.h"


/* Is the given value a sign-extended 32-bit value?  */
#define IS_SEXT_32BIT_NUM(x)                                            \
  (((x) &~ (offsetT) 0x7fffffff) == 0                                   \
  || (((x) &~ (offsetT) 0x7fffffff) == ~ (offsetT) 0x7fffffff))

#define IS_ZEXT_32BIT_NUM(x)                                            \
  (((x) &~ (offsetT) 0xffffffff) == 0                                   \
  || (((x) &~ (offsetT) 0xffffffff) == ~ (offsetT) 0xffffffff))

/* Structure to hold all of the different components describing
   an individual instruction.  */

typedef struct
{
  const CGEN_INSN *     insn;
  const CGEN_INSN *     orig_insn;
  CGEN_FIELDS           fields;
#if CGEN_INT_INSN_P
  CGEN_INSN_INT         buffer [1];
#define INSN_VALUE(buf) (*(buf))
#else
  unsigned char         buffer [CGEN_MAX_INSN_SIZE];
#define INSN_VALUE(buf) (buf)
#endif
  char *                addr;
  fragS *               frag;
  int                   num_fixups;
  fixS *                fixups [GAS_CGEN_MAX_FIXUPS];
  int                   indices [MAX_OPERAND_INSTANCES];
} riscv_insn;

#ifndef DEFAULT_ARCH
#define DEFAULT_ARCH "riscv64"
#endif

static const char default_arch[] = DEFAULT_ARCH;

static unsigned xlen = 0; /* width of an x-register */
static unsigned abi_xlen = 0; /* width of a pointer in the ABI */

static unsigned elf_flags = 0;

/* This is the set of options which the .option pseudo-op may modify.  */

struct riscv_set_options
{
  int pic; /* Generate position-independent code.  */
  int relax; /* Emit relocs the linker is allowed to relax.  */
};

static struct riscv_set_options riscv_opts =
{
  0,	/* pic */
  1,	/* relax */
};

/* The set of ISAs which are supported.  */
CGEN_BITSET *riscv_isas = NULL;

/* This array holds the chars that always start a comment.  If the
    pre-processor is disabled, these aren't very useful */
const char comment_chars[] = "#";

/* This array holds the chars that only start a comment at the beginning of
   a line.  If the line seems to have the form '# 123 filename'
   .line and .file directives will appear in the pre-processed output */
/* Note that input_file.c hand checks for '#' at the beginning of the
   first line of the input file.  This is because the compiler outputs
   #NO_APP at the beginning of its output.  */
/* Also note that C style comments are always supported.  */
const char line_comment_chars[] = "#";

/* This array holds machine specific line separator characters.  */
const char line_separator_chars[] = ";";

/* Chars that can be used to separate mant from exp in floating point nums */
const char EXP_CHARS[] = "eE";

/* Chars that mean this number is a floating point constant */
/* As in 0f12.456 */
/* or    0d1.2345e12 */
const char FLT_CHARS[] = "rRsSfFdDxXpP";

/* The default target format to use.  */

const char *
riscv_target_format (void)
{
  return xlen == 64 ? "elf64-littleriscv" : "elf32-littleriscv";
}

void
md_begin (void)
{
  /* Initialize the `cgen' interface.  */

  /* Set the machine number and endian.  */

  /* NOTE: Even though we provide the CGEN_CPU_OPEN_ISAS option here,
     the created cgen assembler will include instructions from every
     ISA. It is only by defining the hooks CGEN_VALIDATE_INSN_SUPPORTED
     and riscv_cgen_insn_supported() that unsupported instructions
     are rejected */
  gas_cgen_cpu_desc = riscv_cgen_cpu_open (CGEN_CPU_OPEN_ISAS, riscv_isas,
                                           CGEN_CPU_OPEN_MACHS, 0,
                                           CGEN_CPU_OPEN_ENDIAN,
                                           CGEN_ENDIAN_LITTLE,
                                           CGEN_CPU_OPEN_END);
  /* riscv_cgen_cpu_open creates a copy of the isa bitset. Copy it back
     so we can update the supported isa at runtime.

     NOTE: This makes the dangerous assumption that
     gas_cgen_cpu_desc->isas will remain valid for the lifetime of
     the assembler. This is not guaranteed to be the case.  */
  riscv_isas = gas_cgen_cpu_desc->isas;

  riscv_cgen_init_asm (gas_cgen_cpu_desc);

  /* This is a callback from cgen to gas to parse operands.  */
  cgen_set_parse_operand_fn (gas_cgen_cpu_desc, gas_cgen_parse_operand);

  /* Set the default alignment for the text section.  */
  record_alignment (text_section, riscv_subset_supports ("c") ? 1 : 2);
}


/* Fixup creation */
/* These functions are wrappers around standard functions, with the aim
   of tagging instructions with the current state of the 'relax' flag
   which can be altered by an .option directive. The state of the
   'relax' flag is stored in the fx_tcbit which is a CPU dependent bit */

static fixS *
riscv_fix_new (fragS *frag, int where, int size, symbolS *add_symbol,
               offsetT offset, int pcrel, bfd_reloc_code_real_type r_type)
{
  fixS *fixP = fix_new (frag, where, size, add_symbol, offset, pcrel, r_type);
  fixP->fx_tcbit = riscv_opts.relax;
  return fixP;
}

static fixS *
riscv_fix_new_exp (fragS *frag, int where, int size, expressionS *exp,
                   int pcrel, bfd_reloc_code_real_type r_type)
{
  fixS *fixP = fix_new_exp (frag, where, size, exp, pcrel, r_type);
  fixP->fx_tcbit = riscv_opts.relax;
  return fixP;
}

/* Wrapper around standard gas_cgen_record_fixup_exp */
fixS *
riscv_record_fixup_exp (fragS *frag, int where, const CGEN_INSN *insn,
                        int length, const CGEN_OPERAND *operand, int opinfo,
                        expressionS *exp)
{
  fixS *fixP = gas_cgen_record_fixup_exp (frag, where, insn, length, operand,
                                          opinfo, exp);
  fixP->fx_tcbit = riscv_opts.relax;
  return fixP;
}



void
md_assemble (char * str)
{
  const char * errmsg;

  !!!
}

const char *md_shortopts = "O::g::G:";

enum options
{
  OPTION_MARCH = OPTION_MD_BASE,
  OPTION_PIC,
  OPTION_NO_PIC,
  OPTION_MABI,
  OPTION_RELAX,
  OPTION_NO_RELAX,
  OPTION_END_OF_ENUM
};

struct option md_longopts[] =
{
  {"march", required_argument, NULL, OPTION_MARCH},
  {"fPIC", no_argument, NULL, OPTION_PIC},
  {"fpic", no_argument, NULL, OPTION_PIC},
  {"fno-pic", no_argument, NULL, OPTION_NO_PIC},
  {"mabi", required_argument, NULL, OPTION_MABI},
  {"mrelax", no_argument, NULL, OPTION_RELAX},
  {"mno-relax", no_argument, NULL, OPTION_NO_RELAX},

  {NULL, no_argument, NULL, 0}
};
size_t md_longopts_size = sizeof (md_longopts);

enum float_abi {
  FLOAT_ABI_DEFAULT = -1,
  FLOAT_ABI_SOFT,
  FLOAT_ABI_SINGLE,
  FLOAT_ABI_DOUBLE,
  FLOAT_ABI_QUAD
};
static enum float_abi float_abi = FLOAT_ABI_DEFAULT;

static void
riscv_set_abi (unsigned new_xlen, enum float_abi new_float_abi)
{
  abi_xlen = new_xlen;
  float_abi = new_float_abi;
}

int
md_parse_option (int c, const char *arg)
{
  switch (c)
    {
    case OPTION_MARCH:
      riscv_set_arch (arg);
      break;

    case OPTION_NO_PIC:
      riscv_opts.pic = FALSE;
      break;

    case OPTION_PIC:
      riscv_opts.pic = TRUE;
      break;

    case OPTION_MABI:
      if (strcmp (arg, "lp64") == 0)
	riscv_set_abi (64, FLOAT_ABI_SOFT, FALSE);
      else
	return 0;
      break;

    case OPTION_RELAX:
      riscv_opts.relax = TRUE;
      break;

    case OPTION_NO_RELAX:
      riscv_opts.relax = FALSE;
      break;

    default:
      return 0;
    }

  return 1;
}

void
riscv_after_parse_args (void)
{
  abort();
}

void
md_show_usage (FILE *stream)
{
  fprintf (stream, _("\
RISC-V options:\n\
  -fpic          generate position-independent code\n\
  -fno-pic       don't generate position-independent code (default)\n\
  -march=ISA     set the RISC-V architecture\n\
  -mabi=ABI      set the RISC-V ABI\n\
"));
}

/* Interface to relax_segment.  */

const relax_typeS md_relax_table[] =
{
/* The fields are:
   1) most positive reach of this state,
   2) most negative reach of this state,
   3) how many bytes this mode will add to the size of the current frag
   4) which index into the table to try if we can't fit into this one.  */

  /* The first entry must be unused because an `rlx_more' value of zero ends
     each list.  */
  {0, 0, 0, 0},

  /* When relaxing a fragment, the state machine is always started in this
     state. However depending on the type of instructions that is at the end
     of this fragment we will need to advance to a number of different
     states. This is handled in md_estimate_size_before_relax so in this
     table this is just a dummy entry.  */
  {0, 0, 0, 0},
};

int
md_estimate_size_before_relax (fragS * fragP, segT segment)
{
  abort();
}

/* Relax a machine dependent frag.  */

void
md_convert_frag (bfd *   abfd ATTRIBUTE_UNUSED,
                 segT    sec  ATTRIBUTE_UNUSED,
                 fragS * fragP)
{
  abort();
}

/* Functions concerning relocs.  */

/* The location from which a PC relative jump should be calculated,
   given a PC relative reloc.  */

long
md_pcrel_from_section (fixS * fixP, segT sec)
{
  if (fixP->fx_addsy != (symbolS *) NULL
      && (! S_IS_DEFINED (fixP->fx_addsy)
          || (S_GET_SEGMENT (fixP->fx_addsy) != sec)
          || S_IS_EXTERNAL (fixP->fx_addsy)
          || S_IS_WEAK (fixP->fx_addsy)))
    {
      /* The symbol is undefined (or is defined but not in this section).
         Let the linker figure it out.  */
      return 0;
    }

  return fixP->fx_frag->fr_address + fixP->fx_where;
}


/* Return the bfd reloc type for OPERAND of INSN at fixup FIXP.
   Returns BFD_RELOC_NONE if no reloc type can be found.
   *FIXP may be modified if desired.  */

bfd_reloc_code_real_type
md_cgen_lookup_reloc (const CGEN_INSN *    insn ATTRIBUTE_UNUSED,
                      const CGEN_OPERAND * operand,
                      fixS *               fixP)
{
  if (fixP->fx_cgen.opinfo)
    return fixP->fx_cgen.opinfo;

  switch (operand->type)
    {
    default: /* avoid -Wall warning */
      return BFD_RELOC_NONE;
    }
}

/* Write a value out to the object file, using the appropriate endianness.  */

void
md_number_to_chars (char * buf, valueT val, int n)
{
  number_to_chars_littleendian (buf, val, n);
}

/* Turn a string in input_line_pointer into a floating point constant of type
   type, and store the appropriate bytes in *litP.  The number of LITTLENUMS
   emitted is stored in *sizeP .  An error message is returned, or NULL on OK.  */

/* Equal to MAX_PRECISION in atof-ieee.c.  */
#define MAX_LITTLENUMS 6

const char *
md_atof (int type, char * litP, int *  sizeP)
{
  return ieee_md_atof (type, litP, sizeP, TRUE);
}

arelent *
tc_gen_reloc (asection *section ATTRIBUTE_UNUSED, fixS *fixp)
{
  arelent *reloc = (arelent *) xmalloc (sizeof (arelent));

  reloc->sym_ptr_ptr = (asymbol **) xmalloc (sizeof (asymbol *));
  *reloc->sym_ptr_ptr = symbol_get_bfdsym (fixp->fx_addsy);
  reloc->address = fixp->fx_frag->fr_address + fixp->fx_where;
  reloc->addend = fixp->fx_addnumber;

  reloc->howto = bfd_reloc_type_lookup (stdoutput, fixp->fx_r_type);
  if (reloc->howto == NULL)
    {
      if ((fixp->fx_r_type == BFD_RELOC_16 || fixp->fx_r_type == BFD_RELOC_8)
          && fixp->fx_addsy != NULL && fixp->fx_subsy != NULL)
	{
	  /* We don't have R_RISCV_8/16, but for this special case,
	     we can use R_RISCV_ADD8/16 with R_RISCV_SUB8/16.  */
	  return reloc;
	}

      as_bad_where (fixp->fx_file, fixp->fx_line,
                    _("cannot represent %s relocation in object file"),
                    bfd_get_reloc_code_name (fixp->fx_r_type));
      return NULL;
    }

  return reloc;
}

void
md_apply_fix (struct fix *f, valueT *t, segT s ATTRIBUTE_UNUSED)
{
  abort();
}

struct riscv_option_stack
{
  struct riscv_option_stack *next;
  struct riscv_set_options options;
};

static struct riscv_option_stack *riscv_opts_stack;

/* Handle the .option pseudo-op.  */

static void
s_riscv_option (int x ATTRIBUTE_UNUSED)
{
  abort();
}

/* Handle the .bss pseudo-op.  */

static void
s_bss (int ignore ATTRIBUTE_UNUSED)
{
  subseg_set (bss_section, 0);
  demand_empty_rest_of_line ();
}

/* Called from md_do_align.  Used to create an alignment frag in a
   code section by emitting a worst-case NOP sequence that the linker
   will later relax to the correct number of NOPs.  We can't compute
   the correct alignment now because of other linker relaxations.  */

bfd_boolean
riscv_frag_align_code (int n)
{
  bfd_vma bytes = (bfd_vma) 1 << n;
  bfd_vma insn_alignment = riscv_subset_supports ("c") ? 2 : 4;
  bfd_vma worst_case_bytes = bytes - insn_alignment;
  char *nops;
  expressionS ex;

  /* If we are moving to a smaller alignment than the instruction size, then no
     alignment is required. */
  if (bytes <= insn_alignment)
    return TRUE;

  /* When not relaxing, riscv_handle_align handles code alignment.  */
  if (!riscv_opts.relax)
    return FALSE;

  nops = frag_more (worst_case_bytes);

  ex.X_op = O_constant;
  ex.X_add_number = worst_case_bytes;

  riscv_make_nops (nops, worst_case_bytes);

  fix_new_exp (frag_now, nops - frag_now->fr_literal, 0,
	       &ex, FALSE, BFD_RELOC_RISCV_ALIGN);

  return TRUE;
}

/* Implement HANDLE_ALIGN.  */

void
riscv_handle_align (fragS *fragP)
{
  switch (fragP->fr_type)
    {
    case rs_align_code:
      /* When relaxing, riscv_frag_align_code handles code alignment.  */
      if (!riscv_opts.relax)
	{
	  bfd_signed_vma bytes = (fragP->fr_next->fr_address
				  - fragP->fr_address - fragP->fr_fix);
	  /* We have 4 byte uncompressed nops.  */
	  bfd_signed_vma size = 4;
	  bfd_signed_vma excess = bytes % size;
	  char *p = fragP->fr_literal + fragP->fr_fix;

	  if (bytes <= 0)
	    break;

	  /* Insert zeros or compressed nops to get 4 byte alignment.  */
	  if (excess)
	    {
	      riscv_make_nops (p, excess);
	      fragP->fr_fix += excess;
	      p += excess;
	    }

	  /* Insert variable number of 4 byte uncompressed nops.  */
	  riscv_make_nops (p, size);
	  fragP->fr_var = size;
	}
      break;

    default:
      break;
    }
}

/* Standard calling conventions leave the CFA at SP on entry.  */
void
riscv_cfi_frame_initial_instructions (void)
{
  cfi_add_CFA_def_cfa_register (/*SP*/2);
}

void
riscv_elf_final_processing (void)
{
  elf_elfheader (stdoutput)->e_flags |= elf_flags;
}

static const pseudo_typeS riscv_pseudo_table[] =
{
  /* RISC-V-specific pseudo-ops.  */
  {"option", s_riscv_option, 0},
  {"half", cons, 2},
  {"word", cons, 4},
  {"dword", cons, 8},
  {"bss", s_bss, 0},
  { NULL, NULL, 0 },
};

void
riscv_pop_insert (void)
{
  extern void pop_insert (const pseudo_typeS *);

  pop_insert (riscv_pseudo_table);
}
