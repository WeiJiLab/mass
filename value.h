#ifndef VALUE_H
#define VALUE_H
#include "prelude.h"

typedef void(*fn_type_opaque)();
typedef void (*fn_type_void_to_void)(void);
typedef f32 (*fn_type_void_to_f32)(void);
typedef f32 (*fn_type_f32_to_f32)(f32);
typedef f64 (*fn_type_f64_to_f64)(f64);
typedef s32 (*fn_type_void_to_s32)(void);
typedef s64 (*fn_type_void_to_s64)(void);
typedef const char *(*fn_type_void_to_const_charp)(void);
typedef s16 (*fn_type_s16_to_s16)(s16);
typedef s32 (*fn_type_voidp_to_s32)(void*);
typedef s64 (*fn_type_voidp_s64_to_s64)(void*, s64);
typedef s8  (*fn_type_s32_s8_to_s8)(s32, s8);
typedef void (*fn_type_s32p_to_void)(s32*);
typedef s8 (*fn_type_s32_to_s8)(s32);
typedef s32 (*fn_type_s32_to_s32)(s32);
typedef s64 (*fn_type_s32_to_s64)(s32);
typedef s32 (*fn_type_s32_s32_to_s32)(s32, s32);
typedef s64 (*fn_type_s64_to_s64)(s64);
typedef s64 (*fn_type_s64_s64_to_s64)(s64, s64);
typedef s64 (*fn_type_s64_s64_s64_to_s64)(s64, s64, s64);
typedef s64 (*fn_type_s64_s64_s64_s64_s64_to_s64)(s64, s64, s64, s64, s64);
typedef s64 (*fn_type_s64_s64_s64_s64_s64_s64_to_s64)(s64, s64, s64, s64, s64, s64);
typedef s32 (*fn_type__void_to_s32__to_s32)(fn_type_void_to_s32);


typedef enum {
  Operand_Type_None,
  Operand_Type_Register,
  Operand_Type_Xmm,
  Operand_Type_Immediate_8,
  Operand_Type_Immediate_16,
  Operand_Type_Immediate_32,
  Operand_Type_Immediate_64,
  Operand_Type_Memory_Indirect,
  Operand_Type_Sib,
  Operand_Type_RIP_Relative,
  Operand_Type_RIP_Relative_Import,
  Operand_Type_Label_32,
} Operand_Type;

typedef enum {
  Register_A   = 0b0000,
  Register_C   = 0b0001,
  Register_D   = 0b0010,
  Register_B   = 0b0011,

  Register_AH  = 0b0100,
  Register_SP  = 0b0100,
  R_M_SIB      = 0b0100,

  Register_CH  = 0b0100,
  Register_BP  = 0b0101,

  Register_DH  = 0b0100,
  Register_SI  = 0b0110,

  Register_BH  = 0b0100,
  Register_DI  = 0b0111,

  Register_R8  = 0b1000,
  Register_R9  = 0b1001,
  Register_R10 = 0b1010,
  Register_R11 = 0b1011,
  Register_R12 = 0b1100,
  Register_R13 = 0b1101,
  Register_R14 = 0b1110,
  Register_R15 = 0b1111,

  Register_Xmm0 = 0b000,
  Register_Xmm1 = 0b001,
  Register_Xmm2 = 0b010,
  Register_Xmm3 = 0b011,
  Register_Xmm4 = 0b100,
  Register_Xmm5 = 0b101,
  Register_Xmm6 = 0b110,
  Register_Xmm7 = 0b111,
} Register;

typedef struct {
  Register reg;
  s32 displacement;
} Operand_Memory_Indirect;

typedef enum {
  SIB_Scale_1 = 0b00,
  SIB_Scale_2 = 0b01,
  SIB_Scale_4 = 0b10,
  SIB_Scale_8 = 0b11,
} SIB_Scale;

typedef struct {
  SIB_Scale scale;
  Register index;
  Register base;
  s32 displacement;
} Operand_Sib;

typedef struct {
  s32 *patch_target;
  u8 *from_offset;
} Label_Location;
typedef dyn_array_type(Label_Location) Array_Label_Location;

typedef struct {
  u8 *target;
  Array_Label_Location locations;
} Label;

typedef struct {
  Slice name;
  u32 name_rva;
  u32 offset_in_data;
} Import_Symbol;
typedef dyn_array_type(Import_Symbol) Array_Import_Symbol;

typedef struct {
  Slice name;
  u32 name_rva;
  u32 rva;
  Array_Import_Symbol symbols;
  u32 image_thunk_rva;
} Import_Library;
typedef dyn_array_type(Import_Library) Array_Import_Library;

typedef struct {
  Slice library_name;
  Slice symbol_name;
} Operand_RIP_Relative_Import;

typedef struct {
  Operand_Type type;
  u32 byte_size;
  union {
    Register reg;
    s8 imm8;
    s16 imm16;
    s32 imm32;
    s64 imm64;
    Label *label32;
    Operand_Memory_Indirect indirect;
    Operand_Sib sib;
    s64 rip_offset_in_data;
    Operand_RIP_Relative_Import import;
  };
} Operand;

const char *
operand_type_string(
  Operand_Type type
);

struct Descriptor;

typedef struct Value {
  struct Descriptor *descriptor;
  Operand operand;
} Value;

typedef dyn_array_type(Value) Array_Value;
typedef dyn_array_type(Value *) Array_Value_Ptr;

typedef enum {
  Descriptor_Type_Void,
  Descriptor_Type_Integer,
  Descriptor_Type_Float,
  Descriptor_Type_Pointer,
  Descriptor_Type_Fixed_Size_Array,
  Descriptor_Type_Function,
  Descriptor_Type_Struct,
  Descriptor_Type_Tagged_Union,
  Descriptor_Type_Type
} Descriptor_Type;

typedef struct Descriptor_Function {
  Array_Value_Ptr arguments;

  Value *returns;
  bool frozen;
  Value *next_overload;
} Descriptor_Function;

typedef struct {
  Slice name;
  struct Descriptor *descriptor;
  s32 offset;
} Descriptor_Struct_Field;
typedef dyn_array_type(Descriptor_Struct_Field) Array_Descriptor_Struct_Field;

typedef struct {
  Slice name;
  Array_Descriptor_Struct_Field fields;
} Descriptor_Struct;

typedef struct {
  Descriptor_Struct *struct_list;
  s32 struct_count;
} Descriptor_Tagged_Union;

typedef struct {
  struct Descriptor *item;
  u32 length;
} Descriptor_Fixed_Size_Array;

typedef struct {
  u32 byte_size;
} Descriptor_Integer;

typedef struct {
  u32 byte_size;
} Descriptor_Float;

typedef struct Descriptor {
  Descriptor_Type type;
  union {
    Descriptor_Integer integer;
    Descriptor_Float float_;
    Descriptor_Function function;
    Descriptor_Fixed_Size_Array array;
    Descriptor_Struct struct_;
    Descriptor_Tagged_Union tagged_union;
    struct Descriptor *pointer_to;
    struct Descriptor *type_descriptor;
  };
} Descriptor;


Descriptor descriptor_s8 = {
  .type = { Descriptor_Type_Integer },
  .integer = { .byte_size = 1 },
};
Descriptor descriptor_s16 = {
  .type = { Descriptor_Type_Integer },
  .integer = { .byte_size = 2 },
};
Descriptor descriptor_s32 = {
  .type = { Descriptor_Type_Integer },
  .integer = { .byte_size = 4 },
};
Descriptor descriptor_s64 = {
  .type = { Descriptor_Type_Integer },
  .integer = { .byte_size = 8 },
};
Descriptor descriptor_void = {
  .type = Descriptor_Type_Void,
};
Descriptor descriptor_f32 = {
  .type = { Descriptor_Type_Float },
  .integer = { .byte_size = 4 },
};
Descriptor descriptor_f64 = {
  .type = { Descriptor_Type_Float },
  .integer = { .byte_size = 8 },
};

Value void_value = {
  .descriptor = &descriptor_void,
  .operand = { .type = Operand_Type_None },
};

Value *type_s64_value = &(Value) {
  .descriptor = &(Descriptor) {
    .type = Descriptor_Type_Type,
    .type_descriptor = &descriptor_s64,
  },
  .operand = {.type = Operand_Type_None },
};
Value *type_s32_value = &(Value) {
  .descriptor = &(Descriptor) {
    .type = Descriptor_Type_Type,
    .type_descriptor = &descriptor_s32,
  },
  .operand = {.type = Operand_Type_None },
};
Value *type_s16_value = &(Value) {
  .descriptor = &(Descriptor) {
    .type = Descriptor_Type_Type,
    .type_descriptor = &descriptor_s16,
  },
  .operand = {.type = Operand_Type_None },
};
Value *type_s8_value = &(Value) {
  .descriptor = &(Descriptor) {
    .type = Descriptor_Type_Type,
    .type_descriptor = &descriptor_s8,
  },
  .operand = {.type = Operand_Type_None },
};

Value *type_f64_value = &(Value) {
  .descriptor = &(Descriptor) {
    .type = Descriptor_Type_Type,
    .type_descriptor = &descriptor_f64,
  },
  .operand = {.type = Operand_Type_None },
};
Value *type_f32_value = &(Value) {
  .descriptor = &(Descriptor) {
    .type = Descriptor_Type_Type,
    .type_descriptor = &descriptor_f32,
  },
  .operand = {.type = Operand_Type_None },
};

u32
descriptor_byte_size(
  const Descriptor *descriptor
);

bool
same_type(
  Descriptor *a,
  Descriptor *b
);

bool
same_value_type(
  Value *a,
  Value *b
);

typedef enum {
  Instruction_Extension_Type_None,
  Instruction_Extension_Type_Register,
  Instruction_Extension_Type_Op_Code,
  Instruction_Extension_Type_Plus_Register,
} Instruction_Extension_Type;

typedef enum {
  Operand_Encoding_Type_None,
  Operand_Encoding_Type_Register,
  Operand_Encoding_Type_Register_A,
  Operand_Encoding_Type_Register_Memory,
  Operand_Encoding_Type_Xmm,
  Operand_Encoding_Type_Xmm_Memory,
  Operand_Encoding_Type_Memory,
  Operand_Encoding_Type_Immediate,
} Operand_Encoding_Type;

typedef enum {
  Operand_Size_Any = 0,
  Operand_Size_8 = 1,
  Operand_Size_16 = 2,
  Operand_Size_32 = 4,
  Operand_Size_64 = 8,
} Operand_Size;

typedef struct {
  Operand_Encoding_Type type;
  Operand_Size size;
} Operand_Encoding;

typedef struct {
  u8 op_code[4];
  Instruction_Extension_Type extension_type;
  u8 op_code_extension;
  Operand_Encoding operands[3];
} Instruction_Encoding;

typedef struct {
  const char *name;
  const Instruction_Encoding *encoding_list;
  u32 encoding_count;
} X64_Mnemonic;

typedef struct {
  X64_Mnemonic mnemonic;
  Operand operands[3];
  Label *maybe_label;
  const char *filename;
  u32 line_number;
} Instruction;
typedef dyn_array_type(Instruction) Array_Instruction;

typedef struct _Program Program;

// https://docs.microsoft.com/en-us/cpp/build/exception-handling-x64?view=vs-2019#unwind-data-definitions-in-c
typedef enum _UNWIND_OP_CODES {
  UWOP_PUSH_NONVOL = 0, /* info == register number */
  UWOP_ALLOC_LARGE,     /* no info, alloc size in next 2 slots */
  UWOP_ALLOC_SMALL,     /* info == size of allocation / 8 - 1 */
  UWOP_SET_FPREG,       /* no info, FP = RSP + UNWIND_INFO.FPRegOffset*16 */
  UWOP_SAVE_NONVOL,     /* info == register number, offset in next slot */
  UWOP_SAVE_NONVOL_FAR, /* info == register number, offset in next 2 slots */
  UWOP_SAVE_XMM128 = 8, /* info == XMM reg number, offset in next slot */
  UWOP_SAVE_XMM128_FAR, /* info == XMM reg number, offset in next 2 slots */
  UWOP_PUSH_MACHFRAME   /* info == 0: no error-code, 1: error-code */
} UNWIND_CODE_OPS;

typedef union {
  struct {
    u8 CodeOffset;
    u8 UnwindOp : 4;
    u8 OpInfo   : 4;
  };
  u16 FrameOffset;
  u16 DataForPreviousCode;
} UNWIND_CODE;

typedef struct {
  u8 Version       : 3;
  u8 Flags         : 5;
  u8 SizeOfProlog;
  u8 CountOfCodes;
  u8 FrameRegister : 4;
  u8 FrameOffset   : 4;
  // :RegisterAllocation need to add more reserved space for UnwindCode
  UNWIND_CODE UnwindCode[2];
/*  union {
*       OPTIONAL u32 ExceptionHandler;
*       OPTIONAL u32 FunctionEntry;
*   };
*   OPTIONAL u32 ExceptionData[]; */
} UNWIND_INFO;

typedef struct {
  s32 stack_reserve;
  u32 max_call_parameters_stack_size;

  Label *prolog_label;
  Label *epilog_label;

  Array_Instruction instructions;

  Descriptor *descriptor;
  Program *program;

  Value *value;
} Function_Builder;
typedef dyn_array_type(Function_Builder) Array_Function_Builder;

typedef struct Scope Scope;


typedef struct {
  Slice filename;
  u64 line;
  u64 column;
} Source_Location;

typedef struct {
  Slice message;
  Source_Location location;
} Parse_Error;
typedef dyn_array_type(Parse_Error) Array_Parse_Error;

typedef enum {
  Tokenizer_Result_Type_Error,
  Tokenizer_Result_Type_Success,
} Tokenizer_Result_Type;

typedef struct Token Token;

typedef struct {
  Tokenizer_Result_Type type;
  union {
    Token *root;
    Array_Parse_Error errors;
  };
} Tokenizer_Result;

typedef enum {
  Parse_Result_Type_Success,
  Parse_Result_Type_Error,
} Parse_Result_Type;

typedef struct {
  Parse_Result_Type type;
  Array_Parse_Error errors;
} Parse_Result;

typedef struct _Program {
  Fixed_Buffer *data_buffer;
  Array_Import_Library import_libraries;
  Value *entry_point;
  Array_Function_Builder functions;
  s64 code_base_rva;
  s64 data_base_rva;
  Scope *global_scope;
  Array_Parse_Error errors;
} Program;

typedef struct {
  Fixed_Buffer *code_buffer;
  Fixed_Buffer *data_buffer;
} Jit_Program;

Bucket_Buffer temp_buffer;
Allocator *temp_allocator;

Value *
value_global(
  Program *program,
  Descriptor *descriptor
);

Value *
value_global_c_string(
  Program *program,
  const char *string
);

void *
rip_value_pointer(
  Program *program,
  Value *value
);

#define temp_allocate(_type_)\
  bucket_buffer_allocate(temp_buffer, _type_)

#define temp_allocate_array(_type_, _count_)\
  bucket_buffer_allocate_array(temp_buffer, _type_, _count_)

u64
estimate_max_code_size_in_bytes(
  Program *program
);

Fixed_Buffer *
program_end(
  Program *program
);

void
program_push_error_from_bucket_buffer(
  Program *program,
  Source_Location location,
  Bucket_Buffer buffer
);

void
program_push_error_from_slice(
  Program *program,
  Source_Location location,
  Slice message
);

#define program_error_builder(_program_, _location_)\
  for(\
    Bucket_Buffer _buffer = bucket_buffer_make(.allocator = allocator_system);\
    _buffer.internal;\
    program_push_error_from_bucket_buffer((_program_), (_location_), _buffer),\
    _buffer.internal = 0\
  )
#define program_error_append_slice(_slice_)\
  bucket_buffer_append_slice(_buffer, (_slice_))

#define program_error_append_literal(_message_)\
  program_error_append_slice(slice_literal(_message_))

#endif VALUE_H
