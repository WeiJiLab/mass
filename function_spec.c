#include "bdd-for-c.h"

#include "value.c"
#include "instruction.c"
#include "encoding.c"
#include "function.c"
#include "source.c"

spec("function") {
  static Source_File test_source_file = {
    .path = slice_literal_fields(__FILE__),
    .text = slice_literal_fields(""),
  };
  static Source_Range test_range = {
    .file = &test_source_file,
    .offsets = {0},
  };
  static Function_Builder *builder = 0;
  static Bucket_Buffer *temp_buffer = 0;
  static Allocator *temp_allocator = 0;
  static Execution_Context *temp_context = 0;

  before() {
    builder = malloc(sizeof(Function_Builder));
    *builder = (Function_Builder){
      .code_block.instructions = dyn_array_make(Array_Instruction),
    };
  }

  before_each() {
    temp_buffer = bucket_buffer_make(.allocator = allocator_system);
    temp_allocator = bucket_buffer_allocator_make(temp_buffer);
    temp_context = allocator_allocate(temp_allocator, Execution_Context);
    dyn_array_clear(builder->code_block.instructions);
    *builder = (Function_Builder){
      .code_block.instructions = builder->code_block.instructions,
    };
    Mass_Result *result = allocator_allocate(temp_allocator, Mass_Result);
    *result = (Mass_Result){0};
    *temp_context = (Execution_Context) {
      .allocator = temp_allocator,
      .builder = builder,
      .result = result,
    };
  }

  after_each() {
    bucket_buffer_destroy(temp_buffer);
  }

  describe("stack") {
    it("should correctly align allocated values") {
      Value *a = reserve_stack(temp_allocator, builder, &descriptor_s8);
      check(a->storage.tag == Storage_Tag_Memory);
      check(a->storage.Memory.location.tag == Memory_Location_Tag_Indirect);
      check(a->storage.Memory.location.Indirect.offset == -1);
      Value *b = reserve_stack(temp_allocator, builder, &descriptor_s32);
      check(b->storage.tag == Storage_Tag_Memory);
      check(b->storage.Memory.location.tag == Memory_Location_Tag_Indirect);
      check(b->storage.Memory.location.Indirect.offset == -8);
    }
  }

  describe("move_value") {
    it("should not add any instructions when moving value to itself (pointer equality)") {
      Value *reg_a = value_register_for_descriptor(temp_context, Register_A, &descriptor_s8);
      move_value(temp_allocator, builder, &test_range, &reg_a->storage, &reg_a->storage);
      check(dyn_array_length(builder->code_block.instructions) == 0);
    }
    it("should not add any instructions when moving value to itself (value equality)") {
      Value *reg_a1 = value_register_for_descriptor(temp_context, Register_A, &descriptor_s8);
      Value *reg_a2 = value_register_for_descriptor(temp_context, Register_A, &descriptor_s8);
      move_value(temp_allocator, builder, &test_range, &reg_a1->storage, &reg_a2->storage);
      check(dyn_array_length(builder->code_block.instructions) == 0);
    }
    it("should be use `xor` to clear the register instead of move of 0") {
      Descriptor *descriptors[] = {
        &descriptor_s8, &descriptor_s16, &descriptor_s32, &descriptor_s64,
      };
      Value *immediates[] = {
        value_from_s8(temp_context, 0),
        value_from_s16(temp_context, 0),
        value_from_s32(temp_context, 0),
        value_from_s64(temp_context, 0),
      };
      for (u64 i = 0; i < countof(descriptors); ++i) {
        Value *reg_a = value_register_for_descriptor(temp_context, Register_A, descriptors[i]);
        // We end at the current descriptor index to make sure we do not
        // try to move larger immediate into a smaller register
        for (u64 j = 0; j <= u64_min(i, countof(immediates)); ++j) {
          dyn_array_clear(builder->code_block.instructions);
          move_value(temp_allocator, builder, &test_range, &reg_a->storage, &immediates[j]->storage);
          check(dyn_array_length(builder->code_block.instructions) == 1);
          Instruction *instruction = dyn_array_get(builder->code_block.instructions, 0);
          check(instruction_equal(
            instruction, &(Instruction){.assembly = {xor, reg_a->storage, reg_a->storage}}
          ));
        }
      }
    }
    it("should be able to do a single instruction move for imm to a register") {
      Descriptor *descriptors[] = {
        &descriptor_s8, &descriptor_s16, &descriptor_s32, &descriptor_s64,
      };
      Value *immediates[] = {
        value_from_s8(temp_context, 42),
        value_from_s16(temp_context, 4200),
        value_from_s32(temp_context, 42000),
        value_from_s64(temp_context, 42ll << 32ll),
      };
      for (u64 i = 0; i < countof(descriptors); ++i) {
        Value *reg_a = value_register_for_descriptor(temp_context, Register_A, descriptors[i]);
        // We end at the current descriptor index to make sure we do not
        // try to move larger immediate into a smaller register
        for (u64 j = 0; j <= u64_min(i, countof(immediates) - 1); ++j) {
          dyn_array_clear(builder->code_block.instructions);
          move_value(temp_allocator, builder, &test_range, &reg_a->storage, &immediates[j]->storage);
          check(dyn_array_length(builder->code_block.instructions) == 1);
          Instruction *instruction = dyn_array_get(builder->code_block.instructions, 0);
          check(instruction->assembly.mnemonic == mov);
          check(storage_equal(&instruction->assembly.operands[0], &reg_a->storage));
          s64 actual_immediate = storage_immediate_value_up_to_s64(&instruction->assembly.operands[1]);
          s64 expected_immediate = storage_immediate_value_up_to_s64(&immediates[j]->storage);
          check(actual_immediate == expected_immediate);
        }
      }
    }
    it("should be able to do a single instruction move for non-64bit imm to memory") {
      Descriptor *descriptors[] = {
        &descriptor_s8, &descriptor_s16, &descriptor_s32, &descriptor_s64,
      };
      Value *immediates[] = {
        value_from_s8(temp_context, 42),
        value_from_s16(temp_context, 4200),
        value_from_s32(temp_context, 42000),
      };
      for (u64 i = 0; i < countof(descriptors); ++i) {
        Value *memory = &(Value){descriptors[i], stack(0, descriptor_byte_size(descriptors[i]))};
        // We end at the current descriptor index to make sure we do not
        // try to move larger immediate into a smaller register
        for (u64 j = 0; j <= u64_min(i, countof(immediates) - 1); ++j) {
          dyn_array_clear(builder->code_block.instructions);
          move_value(temp_allocator, builder, &test_range, &memory->storage, &immediates[j]->storage);
          check(dyn_array_length(builder->code_block.instructions) == 1);
          Instruction *instruction = dyn_array_get(builder->code_block.instructions, 0);
          check(instruction->assembly.mnemonic == mov);
          check(storage_equal(&instruction->assembly.operands[0], &memory->storage));
          s64 actual_immediate = storage_immediate_value_up_to_s64(&instruction->assembly.operands[1]);
          s64 expected_immediate = storage_immediate_value_up_to_s64(&immediates[j]->storage);
          check(actual_immediate == expected_immediate);
        }
      }
    }
    it("should use a 32bit immediate when the value fits for a move to a 64bit value") {
      Value *memory = &(Value){&descriptor_s64, stack(0, 8)};
      Storage immediate = imm64(temp_allocator, 42000);
      move_value(temp_allocator, builder, &test_range, &memory->storage, &immediate);
      check(dyn_array_length(builder->code_block.instructions) == 1);
      Instruction *instruction = dyn_array_get(builder->code_block.instructions, 0);
      check(instruction->assembly.mnemonic == mov);
      check(storage_equal(&instruction->assembly.operands[0], &memory->storage));
      check(instruction->assembly.operands[1].byte_size == 4);
    }
    it("should use a temp register for a imm64 to memory move") {
      Value *memory = &(Value){&descriptor_s64, stack(0, 8)};
      Value *immediate = value_from_s64(temp_context, 42ll << 32);
      move_value(temp_allocator, builder, &test_range, &memory->storage, &immediate->storage);
      check(dyn_array_length(builder->code_block.instructions) == 2);
      Value *temp_reg = value_register_for_descriptor(temp_context, Register_C, &descriptor_s64);
      check(instruction_equal(
        dyn_array_get(builder->code_block.instructions, 0),
        &(Instruction){.assembly = {mov, temp_reg->storage, immediate->storage}}
      ));
      check(instruction_equal(
        dyn_array_get(builder->code_block.instructions, 1),
        &(Instruction){.assembly = {mov, memory->storage, temp_reg->storage}}
      ));
    }
    it("should use appropriate setCC instruction when moving from eflags") {
      struct { Compare_Type compare_type; const X64_Mnemonic *mnemonic; } tests[] = {
        { Compare_Type_Equal, sete },
        { Compare_Type_Signed_Less, setl },
        { Compare_Type_Signed_Greater_Equal, setge },
      };
      for (u64 i = 0; i < countof(tests); ++i) {
        dyn_array_clear(builder->code_block.instructions);
        Value *memory = &(Value){&descriptor_s8, stack(0, 1)};
        Value *eflags = value_from_compare(temp_context, tests[i].compare_type);
        move_value(temp_allocator, builder, &test_range, &memory->storage, &eflags->storage);
        check(dyn_array_length(builder->code_block.instructions) == 1);
        Instruction *instruction = dyn_array_get(builder->code_block.instructions, 0);
        check(instruction->assembly.mnemonic == tests[i].mnemonic);
        check(storage_equal(&instruction->assembly.operands[0], &memory->storage));
        check(storage_equal(&instruction->assembly.operands[1], &eflags->storage));
      }
    }
    it("should use a temporary register for setCC to more than a byte") {
      struct { Descriptor *descriptor; } tests[] = {
        { &descriptor_s16 },
        { &descriptor_s32 },
        { &descriptor_s64 },
      };
      for (u64 i = 0; i < countof(tests); ++i) {
        dyn_array_clear(builder->code_block.instructions);
        Value *memory = &(Value){
          tests[i].descriptor,
          stack(0, descriptor_byte_size(tests[i].descriptor))
        };
        Value *temp_reg = value_register_for_descriptor(temp_context, Register_C, &descriptor_s8);
        Value *resized_temp_reg = value_register_for_descriptor(temp_context, Register_C, tests[i].descriptor);
        Value *eflags = value_from_compare(temp_context, Compare_Type_Equal);
        move_value(temp_allocator, builder, &test_range, &memory->storage, &eflags->storage);
        check(dyn_array_length(builder->code_block.instructions) == 3);
        check(instruction_equal(
          dyn_array_get(builder->code_block.instructions, 0),
          &(Instruction){.assembly = {sete, temp_reg->storage, eflags->storage}}
        ));
        check(instruction_equal(
          dyn_array_get(builder->code_block.instructions, 1),
          &(Instruction){.assembly = {movsx, resized_temp_reg->storage, temp_reg->storage}}
        ));
        check(instruction_equal(
          dyn_array_get(builder->code_block.instructions, 2),
          &(Instruction){.assembly = {mov, memory->storage, resized_temp_reg->storage}}
        ));
      }
    }
  }
  describe("plus") {
    it("should fold s8 immediates and move them to the result value") {
      Value *reg_a = value_register_for_descriptor(temp_context, Register_A, &descriptor_s8);
      plus(temp_context, &test_range, reg_a, value_from_s8(temp_context, 30), value_from_s8(temp_context, 12));
      check(dyn_array_length(builder->code_block.instructions) == 1);
      Instruction *instruction = dyn_array_get(builder->code_block.instructions, 0);
      check(instruction_equal(
        instruction, &(Instruction){.assembly = {mov, reg_a->storage, imm8(temp_allocator, 42)}}
      ));
    }
    it("should move `a` to result and add `b` to it when result is neither `a` or `b`") {
      Value *reg_a = value_register_for_descriptor(temp_context, Register_A, &descriptor_s32);
      Value *reg_b = value_register_for_descriptor(temp_context, Register_B, &descriptor_s32);
      Value *reg_c = value_register_for_descriptor(temp_context, Register_C, &descriptor_s32);

      plus(temp_context, &test_range, reg_a, reg_b, reg_c);
      check(dyn_array_length(builder->code_block.instructions) == 2);
      check(instruction_equal(
        dyn_array_get(builder->code_block.instructions, 0),
        &(Instruction){.assembly = {mov, reg_a->storage, reg_b->storage}}
      ));
      check(instruction_equal(
        dyn_array_get(builder->code_block.instructions, 1),
        &(Instruction){.assembly = {add, reg_a->storage, reg_c->storage}}
      ));
    }
    it("should avoid moving `a` to result when result is also `a`") {
      Value *reg_a = value_register_for_descriptor(temp_context, Register_A, &descriptor_s32);
      Value *reg_b = value_register_for_descriptor(temp_context, Register_B, &descriptor_s32);

      plus(temp_context, &test_range, reg_a, reg_a, reg_b);
      check(dyn_array_length(builder->code_block.instructions) == 1);
      check(instruction_equal(
        dyn_array_get(builder->code_block.instructions, 0),
        &(Instruction){.assembly = {add, reg_a->storage, reg_b->storage}}
      ));
    }
    it("should flip operands and avoid moving `b` to result when result is also `b`") {
      Value *reg_a = value_register_for_descriptor(temp_context, Register_A, &descriptor_s32);
      Value *reg_b = value_register_for_descriptor(temp_context, Register_B, &descriptor_s32);

      plus(temp_context, &test_range, reg_b, reg_a, reg_b);
      check(dyn_array_length(builder->code_block.instructions) == 1);
      check(instruction_equal(
        dyn_array_get(builder->code_block.instructions, 0),
        &(Instruction){.assembly = {add, reg_b->storage, reg_a->storage}}
      ));
    }
    it("should use the larger operand's size for the result") {
      Value *r32 = value_register_for_descriptor(temp_context, register_acquire_temp(builder), &descriptor_s32);
      Value *result_m32 = &(Value){&descriptor_s32, stack(0, 4)};
      Value *m8 = &(Value){&descriptor_s8, stack(0, 1)};
      plus(temp_context, &test_range, result_m32, r32, m8);
      check(dyn_array_length(builder->code_block.instructions) == 3);
      Value *temp = value_register_for_descriptor(temp_context, register_acquire_temp(builder), &descriptor_s32);
      check(instruction_equal(
        dyn_array_get(builder->code_block.instructions, 0),
        &(Instruction){.assembly = {movsx, temp->storage, m8->storage}}
      ));
      check(instruction_equal(
        dyn_array_get(builder->code_block.instructions, 1),
        &(Instruction){.assembly = {add, temp->storage, r32->storage}}
      ));
      check(instruction_equal(
        dyn_array_get(builder->code_block.instructions, 2),
        &(Instruction){.assembly = {mov, result_m32->storage, temp->storage}}
      ));
    }
    it("should use a temp register when result is also `a`, but both operands are memory") {
      Value *temp_reg = value_register_for_descriptor(temp_context, Register_C, &descriptor_s32);
      Value *m_a = &(Value){&descriptor_s32, stack(0, 4)};
      Value *m_b = &(Value){&descriptor_s32, stack(4, 4)};

      plus(temp_context, &test_range, m_a, m_a, m_b);
      check(dyn_array_length(builder->code_block.instructions) == 3);
      check(instruction_equal(
        dyn_array_get(builder->code_block.instructions, 0),
        &(Instruction){.assembly = {mov, temp_reg->storage, m_a->storage}}
      ));
      check(instruction_equal(
        dyn_array_get(builder->code_block.instructions, 1),
        &(Instruction){.assembly = {add, temp_reg->storage, m_b->storage}}
      ));
      check(instruction_equal(
        dyn_array_get(builder->code_block.instructions, 2),
        &(Instruction){.assembly = {mov, m_a->storage, temp_reg->storage}}
      ));
    }
  }
  describe("minus") {
    it("should fold s8 immediates and move them to the result value") {
      Value *temp_reg = value_register_for_descriptor(temp_context, Register_C, &descriptor_s8);
      minus(temp_context, &test_range, temp_reg, value_from_s8(temp_context, 52), value_from_s8(temp_context, 10));
      check(dyn_array_length(builder->code_block.instructions) == 1);
      Instruction *instruction = dyn_array_get(builder->code_block.instructions, 0);
      check(instruction_equal(
        instruction, &(Instruction){.assembly = {mov, temp_reg->storage, imm8(temp_allocator, 42)}}
      ));
    }
    it("should move `a` to result and sub `b` from it when result is neither `a` or `b`") {
      Value *reg_a = value_register_for_descriptor(temp_context, Register_A, &descriptor_s32);
      Value *reg_b = value_register_for_descriptor(temp_context, Register_B, &descriptor_s32);
      Value *reg_c = value_register_for_descriptor(temp_context, Register_C, &descriptor_s32);

      minus(temp_context, &test_range, reg_a, reg_b, reg_c);
      check(dyn_array_length(builder->code_block.instructions) == 2);
      check(instruction_equal(
        dyn_array_get(builder->code_block.instructions, 0),
        &(Instruction){.assembly = {mov, reg_a->storage, reg_b->storage}}
      ));
      check(instruction_equal(
        dyn_array_get(builder->code_block.instructions, 1),
        &(Instruction){.assembly = {sub, reg_a->storage, reg_c->storage}}
      ));
    }
    it("should avoid moving `a` to result when result is also `a`") {
      Value *reg_a = value_register_for_descriptor(temp_context, Register_A, &descriptor_s32);
      Value *reg_b = value_register_for_descriptor(temp_context, Register_B, &descriptor_s32);

      minus(temp_context, &test_range, reg_a, reg_a, reg_b);
      check(dyn_array_length(builder->code_block.instructions) == 1);
      check(instruction_equal(
        dyn_array_get(builder->code_block.instructions, 0),
        &(Instruction){.assembly = {sub, reg_a->storage, reg_b->storage}}
      ));
    }
    it("should use a temp A register when result is also `b`") {
      Value *temp_reg = value_register_for_descriptor(temp_context, Register_D, &descriptor_s32);
      Value *reg_b = value_register_for_descriptor(temp_context, Register_B, &descriptor_s32);
      register_acquire(builder, Register_B);
      register_acquire(builder, Register_C);
      Value *reg_c = value_register_for_descriptor(temp_context, Register_C, &descriptor_s32);

      minus(temp_context, &test_range, reg_c, reg_b, reg_c);
      check(dyn_array_length(builder->code_block.instructions) == 3);
      check(instruction_equal(
        dyn_array_get(builder->code_block.instructions, 0),
        &(Instruction){.assembly = {mov, temp_reg->storage, reg_b->storage}}
      ));
      check(instruction_equal(
        dyn_array_get(builder->code_block.instructions, 1),
        &(Instruction){.assembly = {sub, temp_reg->storage, reg_c->storage}}
      ));
      check(instruction_equal(
        dyn_array_get(builder->code_block.instructions, 2),
        &(Instruction){.assembly = {mov, reg_c->storage, temp_reg->storage}}
      ));
    }
  }
}
