typedef enum {
  Instruction_Extension_Type_Register = 1,
  Instruction_Extension_Type_Op_Code,
  Instruction_Extension_Type_Plus_Register,
} Instruction_Extension_Type;

typedef enum {
  Operand_Encoding_Type_None,
  Operand_Encoding_Type_Register,
  Operand_Encoding_Type_Register_Memory,
  Operand_Encoding_Type_Immediate_8,
  Operand_Encoding_Type_Immediate_32,
} Operand_Encoding_Type;

typedef struct {
  u16 op_code;
  Instruction_Extension_Type extension_type;
  u8 op_code_extension;
  Operand_Encoding_Type operand_encoding_types[2];
} Instruction_Encoding;

typedef struct {
  const Instruction_Encoding *encoding_list;
  u32 encoding_count;
} X64_Mnemonic;

////////////////////////////////////////////////////////////////////////////////
// mov
////////////////////////////////////////////////////////////////////////////////
const Instruction_Encoding mov_encoding_list[] = {
  {
    .op_code = 0x89,
    .extension_type = Instruction_Extension_Type_Register,
    .operand_encoding_types = {
      Operand_Encoding_Type_Register_Memory,
      Operand_Encoding_Type_Register
    },
  },
  {
    .op_code = 0xc7,
    .extension_type = Instruction_Extension_Type_Op_Code,
    .op_code_extension = 0,
    .operand_encoding_types = {
      Operand_Encoding_Type_Register_Memory,
      Operand_Encoding_Type_Immediate_32
    },
  },
};

const X64_Mnemonic mov = {
  .encoding_list = (const Instruction_Encoding *)mov_encoding_list,
  .encoding_count = static_array_size(mov_encoding_list),
};

////////////////////////////////////////////////////////////////////////////////
// ret
////////////////////////////////////////////////////////////////////////////////
const Instruction_Encoding ret_encoding_list[] = {
  {
    .op_code = 0xc3,
    .extension_type = Instruction_Extension_Type_Register,
    .operand_encoding_types = {
      Operand_Encoding_Type_None,
      Operand_Encoding_Type_None
    },
  },
};
const X64_Mnemonic ret = {
  .encoding_list = (const Instruction_Encoding *)ret_encoding_list,
  .encoding_count = static_array_size(ret_encoding_list),
};

////////////////////////////////////////////////////////////////////////////////
// add
////////////////////////////////////////////////////////////////////////////////
const Instruction_Encoding add_encoding_list[] = {
  {
    .op_code = 0x03,
    .extension_type = Instruction_Extension_Type_Register,
    .operand_encoding_types = {
      Operand_Encoding_Type_Register,
      Operand_Encoding_Type_Register_Memory
    },
  },
  {
    .op_code = 0x83,
    .extension_type = Instruction_Extension_Type_Op_Code,
    .op_code_extension = 0,
    .operand_encoding_types = {
      Operand_Encoding_Type_Register_Memory,
      Operand_Encoding_Type_Immediate_8
    },
  },
};
const X64_Mnemonic add = {
  .encoding_list = (const Instruction_Encoding *)add_encoding_list,
  .encoding_count = static_array_size(add_encoding_list),
};

////////////////////////////////////////////////////////////////////////////////
// sub
////////////////////////////////////////////////////////////////////////////////
const Instruction_Encoding sub_encoding_list[] = {
  {
    .op_code = 0x83,
    .extension_type = Instruction_Extension_Type_Op_Code,
    .op_code_extension = 5,
    .operand_encoding_types = {
      Operand_Encoding_Type_Register_Memory,
      Operand_Encoding_Type_Immediate_8
    },
  },
};
const X64_Mnemonic sub = {
  .encoding_list = (const Instruction_Encoding *)sub_encoding_list,
  .encoding_count = static_array_size(sub_encoding_list),
};

