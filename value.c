#include "value.h"

bool
same_type(
  Descriptor *a,
  Descriptor *b
) {
  if (a->type != b->type) return false;
  switch(a->type) {
    case Descriptor_Type_Pointer: {
      return same_type(a->pointer_to, b->pointer_to);
    }
    case Descriptor_Type_Fixed_Size_Array: {
      return same_type(a->array.item, b->array.item) && a->array.length == b->array.length;
    }
    case Descriptor_Type_Struct: {
      return a == b;
    }
    case Descriptor_Type_Function: {
      if (!same_type(a->function.returns->descriptor, b->function.returns->descriptor)) {
        return false;
      }
      if (a->function.argument_count != b->function.argument_count) {
        return false;
      }
      for (s64 i = 0; i < a->function.argument_count; ++i) {
        if (!same_type(
          a->function.argument_list[i].descriptor,
          b->function.argument_list[i].descriptor)
        ) {
          return false;
        }
      }
      return true;
    }
    default: {
      return descriptor_byte_size(a) == descriptor_byte_size(b);
    }
  }
}

Value_Overload *
maybe_get_if_single_overload(
  Value *value
) {
  if (value->overload_count == 1) {
    return value->overload_list[0];
  }
  return 0;
}
inline bool
same_overload_type(
  Value_Overload *a_overload,
  Value_Overload *b_overload
) {
  return same_type(a_overload->descriptor, b_overload->descriptor);
}

inline bool
same_value_type(
  Value *a,
  Value *b
) {
  // FIXME
  Value_Overload *a_overload = maybe_get_if_single_overload(a);
  Value_Overload *b_overload = maybe_get_if_single_overload(b);
  assert(a_overload);
  assert(b_overload);
  return same_type(a_overload->descriptor, b_overload->descriptor);
}

typedef struct {
  Value_Overload *a;
  Value_Overload *b;
} Value_Overload_Pair;

Value_Overload_Pair *
get_matching_values(
  Value *a_value,
  Value *b_value
) {
  for (u32 a_index = 0; a_index < a_value->overload_count; ++a_index) {
    Value_Overload *a = a_value->overload_list[a_index];
    for (u32 b_index = 0; b_index < b_value->overload_count; ++b_index) {
      Value_Overload *b = b_value->overload_list[b_index];
      if (same_overload_type(a, b)) {
        Value_Overload_Pair *result = temp_allocate(Value_Overload_Pair);
        *result = (const Value_Overload_Pair) { a, b };
        return result;
      }
    }
  }
  return 0;
}

u32
descriptor_byte_size(
  const Descriptor *descriptor
) {
  assert(descriptor);
  switch(descriptor->type) {
    case Descriptor_Type_Void: {
      return 0;
    }
    case Descriptor_Type_Struct: {
      s64 count = descriptor->struct_.field_count;
      assert(count);
      u32 alignment = 0;
      u32 raw_size = 0;
      for (s32 i = 0; i < count; ++i) {
        Descriptor_Struct_Field *field = &descriptor->struct_.field_list[i];
        u32 field_size = descriptor_byte_size(field->descriptor);
        alignment = max(alignment, field_size);
        bool is_last_field = i == count - 1;
        if (is_last_field) {
          raw_size = field->offset + field_size;
        }
      }
      return align(raw_size, alignment);
    }
    case Descriptor_Type_Integer: {
      return descriptor->integer.byte_size;
    }
    case Descriptor_Type_Fixed_Size_Array: {
      return descriptor_byte_size(descriptor->array.item) * descriptor->array.length;
    }
    case Descriptor_Type_Pointer:
    case Descriptor_Type_Function: {
      return 8;
    }
    default: {
      assert(!"Unknown Descriptor Type");
    }
  }
  return 0;
}

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


// @Volatile @Reflection
typedef struct {
  s32 field_count;
} Descriptor_Struct_Reflection;

// @Volatile @Reflection
Descriptor_Struct_Field struct_reflection_fields[] = {
  {
    .name = "field_count",
    .offset = 0,
    .descriptor = &descriptor_s32,
  }
};

Descriptor descriptor_struct_reflection = {
  .type = Descriptor_Type_Struct,
  .struct_ = {
    .field_list = struct_reflection_fields,
    .field_count = static_array_size(struct_reflection_fields),
  },
};

Value_Overload void_value_overload = {
  .descriptor = &descriptor_void,
  .operand = { .type = Operand_Type_None },
};
Value_Overload *vode_value_overload_pointer = &void_value_overload;
Value void_value = {
  .overload_list = &vode_value_overload_pointer,
  .overload_count = 1,
};


const char *
operand_type_string(
  Operand_Type type
) {
  switch (type) {
    case Operand_Type_None: {
      return "_";
    }
    case Operand_Type_Register: {
      return "r8";
    }
    case Operand_Type_Immediate_8: {
      return "imm8";
    }
    case Operand_Type_Immediate_32: {
      return "imm32";
    }
    case Operand_Type_Immediate_64: {
      return "imm64";
    }
    case Operand_Type_Memory_Indirect: {
      return "m";
    }
  }
  return "Unknown";
}

#define define_register(reg_name, reg_index, reg_byte_size) \
const Operand reg_name = { \
  .type = Operand_Type_Register, \
  .byte_size = (reg_byte_size), \
  .reg = (reg_index), \
};

define_register(rax, 0, 8); // 0b0000
define_register(rcx, 1, 8);
define_register(rdx, 2, 8);
define_register(rbx, 3, 8);
define_register(rsp, 4, 8);
define_register(rbp, 5, 8);
define_register(rsi, 6, 8);
define_register(rdi, 7, 8);

define_register(eax, 0, 4);
define_register(ecx, 1, 4);
define_register(edx, 2, 4);
define_register(ebx, 3, 4);
define_register(esp, 4, 4);
define_register(ebp, 5, 4);
define_register(esi, 6, 4);
define_register(edi, 7, 4);

define_register(r8,  8, 8);
define_register(r9,  9, 8);
define_register(r10, 10, 8);
define_register(r11, 11, 8);
define_register(r12, 12, 8);
define_register(r13, 13, 8);
define_register(r14, 14, 8);
define_register(r15, 15, 8);

define_register(r8d,  8, 4);
define_register(r9d,  9, 4);
define_register(r10d, 10, 4);
define_register(r11d, 11, 4);
define_register(r12d, 12, 4);
define_register(r13d, 13, 4);
define_register(r14d, 14, 4);
define_register(r15d, 15, 4);
#undef define_register

inline Operand
imm8(
  s8 value
) {
  return (const Operand) {
    .type = Operand_Type_Immediate_8,
    .byte_size = 1,
    .imm8 = value
  };
}

inline Operand
imm32(
  s32 value
) {
  return (const Operand) {
    .type = Operand_Type_Immediate_32,
    .byte_size = 4,
    .imm32 = value
  };
}

inline Operand
imm64(
  s64 value
) {
  return (const Operand) {
    .type = Operand_Type_Immediate_64,
    .byte_size = 8,
    .imm64 = value
  };
}

inline Operand
stack(
  s32 offset,
  u32 byte_size
) {
  return (const Operand) {
    .type = Operand_Type_Memory_Indirect,
    .byte_size = byte_size,
    .indirect = (const Operand_Memory_Indirect) {
      .reg = rsp.reg,
      .displacement = offset,
    }
  };
}

Value *
single_overload_value(
  Value_Overload *overload
) {
  Value *result = temp_allocate(Value);
  Value_Overload **overload_list = temp_allocate_array(Value_Overload *, 1);
  overload_list[0] = overload;
  *result = (const Value) {
    .overload_list = overload_list,
    .overload_count = 1,
  };
  return result;
}

Value *
value_from_s64(
  s64 integer
) {
  Value_Overload *result = temp_allocate(Value_Overload);
  *result = (const Value_Overload) {
    .descriptor = &descriptor_s64,
    .operand = imm64(integer),
  };
  return single_overload_value(result);
}

Value *
value_from_s32(
  s32 integer
) {
  Value_Overload *result = temp_allocate(Value_Overload);
  *result = (const Value_Overload) {
    .descriptor = &descriptor_s32,
    .operand = imm32(integer),
  };
  return single_overload_value(result);
}

Value *
value_byte_size(
  Value *value
) {
  s32 byte_size = 0;
  for (s32 i = 0; i < value->overload_count; ++i){
    Value_Overload *overload = value->overload_list[i];
    s32 overload_byte_size = descriptor_byte_size(overload->descriptor);
    if (i == 0) {
      byte_size = overload_byte_size;
    } else {
      assert(byte_size == overload_byte_size);
    }
  }
  assert(byte_size);
  return value_from_s32(byte_size);
}


Value_Overload *
value_register_for_descriptor(
  Register reg,
  Descriptor *descriptor
) {
  u32 byte_size = descriptor_byte_size(descriptor);
  assert(byte_size == 1 || byte_size == 2 || byte_size == 4 || byte_size == 8);

  Value_Overload *result = temp_allocate(Value_Overload);
  *result = (const Value_Overload) {
    .descriptor = descriptor,
    .operand = {
      .type = Operand_Type_Register,
      .reg = reg,
      .byte_size = byte_size,
    },
  };
  return result;
}

Descriptor *
descriptor_pointer_to(
  Descriptor *descriptor
) {
  Descriptor *result = temp_allocate(Descriptor);
  *result = (const Descriptor) {
    .type = Descriptor_Type_Pointer,
    .pointer_to = descriptor,
  };
  return result;
}

Descriptor *
descriptor_array_of(
  Descriptor *descriptor,
  u32 length
) {
  Descriptor *result = temp_allocate(Descriptor);
  *result = (const Descriptor) {
    .type = Descriptor_Type_Fixed_Size_Array,
    .array = {
      .item = descriptor,
      .length = length,
    },
  };
  return result;
}

s64
helper_value_as_function(
  Value *value
) {
  Value_Overload *overload = maybe_get_if_single_overload(value);
  assert(overload);
  assert(overload->operand.type == Operand_Type_Immediate_64);
  return overload->operand.imm64;
}

#define value_as_function(_value_, _type_) \
  ((_type_)helper_value_as_function(_value_))


bool
memory_range_equal_to_c_string(
  const void *memory_range_start,
  const void *memory_range_end,
  const char *string
) {
  s64 length = ((char *)memory_range_end) - ((char *)memory_range_start);
  s64 string_length = strlen(string);
  if (string_length != length) return false;
  return memcmp(memory_range_start, string, string_length) == 0;
}

Descriptor *
parse_c_type(
  const char *range_start,
  const char *range_end
) {
  Descriptor *descriptor = 0;

  const char *start = range_start;
  for(const char *ch = range_start; ch <= range_end; ++ch) {
    if (!(*ch == ' ' || *ch == '*' || ch == range_end)) continue;
    if (start != ch) {
      if (memory_range_equal_to_c_string(start, ch, "char")) {
        descriptor = &descriptor_s8;
      } else if (memory_range_equal_to_c_string(start, ch, "int")) {
        descriptor = &descriptor_s32;
      } else if (memory_range_equal_to_c_string(start, ch, "void")) {
        descriptor = &descriptor_void;
      } else if (memory_range_equal_to_c_string(start, ch, "const")) {
        // TODO support const values?
      } else {
        assert(!"Unsupported argument type");
      }
    }
    if (*ch == '*') {
      assert(descriptor);
      Descriptor *previous_descriptor = descriptor;
      descriptor = temp_allocate(Descriptor);
      *descriptor = (const Descriptor) {
        .type = Descriptor_Type_Pointer,
        .pointer_to = previous_descriptor,
      };
    }
    start = ch + 1;
  }
  return descriptor;
}

Value_Overload *
c_function_return_value(
  const char *forward_declaration
) {
  char *ch = strchr(forward_declaration, '(');
  assert(ch);
  --ch;

  // skip whitespace before (
  for(; *ch == ' '; --ch);
  for(;
    (*ch >= 'a' && *ch <= 'z') ||
    (*ch >= 'A' && *ch <= 'Z') ||
    (*ch >= '0' && *ch <= '9') ||
    *ch == '_';
    --ch
  )
  // skip whitespace before function name
  for(; *ch == ' '; --ch);
  ++ch;
  Descriptor *descriptor = parse_c_type(forward_declaration, ch);
  assert(descriptor);
  switch(descriptor->type) {
    case Descriptor_Type_Void: {
      return &void_value_overload;
    }
    case Descriptor_Type_Function:
    case Descriptor_Type_Integer:
    case Descriptor_Type_Pointer: {
      Value_Overload *return_value = temp_allocate(Value_Overload);
      *return_value = (const Value_Overload) {
        .descriptor = descriptor,
        .operand = eax,
      };
      return return_value;
    }
    default: {
      assert(!"Unsupported return type");
    }
  }
  return 0;
}

Value *
c_function_value(
  const char *forward_declaration,
  fn_type_opaque fn
) {
  Descriptor *descriptor = temp_allocate(Descriptor);
  *descriptor = (const Descriptor) {
    .type = Descriptor_Type_Function,
    .function = {0},
  };
  Value_Overload *result = temp_allocate(Value_Overload);
  *result = (const Value_Overload) {
    .descriptor = descriptor,
    .operand = imm64((s64) fn),
  };

  result->descriptor->function.returns = c_function_return_value(forward_declaration);
  char *ch = strchr(forward_declaration, '(');
  assert(ch);
  ch++;

  char *start = ch;
  Descriptor *argument_descriptor = 0;
  for (; *ch; ++ch) {
    if (*ch == ',' || *ch == ')') {
      if (start != ch) {
        argument_descriptor = parse_c_type(start, ch);
        assert(argument_descriptor);
      }
      start = ch + 1;
    }
  }

  if (argument_descriptor && argument_descriptor->type != Descriptor_Type_Void) {
    Value_Overload *arg = temp_allocate(Value_Overload);
    arg->descriptor = argument_descriptor;
    // FIXME should not use a hardcoded register here
    arg->operand = rcx;

    result->descriptor->function.argument_list = arg;
    result->descriptor->function.argument_count = 1;
  }

  return single_overload_value(result);
}