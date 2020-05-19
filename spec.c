#include "bdd-for-c.h"
#include "windows.h"
#include <stdio.h>

#include "value.c"
#include "prelude.c"
#include "instruction.c"
#include "encoding.c"

typedef struct {
  // TODO make it s32
  u8 stack_reserve;
  u8 next_argument_index;
  Buffer buffer;

  Descriptor_Function descriptor;
} Function_Builder;

Operand
declare_variable(
  Function_Builder *fn,
  u32 byte_size
) {
  Operand result = stack(fn->stack_reserve, byte_size);
  fn->stack_reserve += 0x10;
  return result;
}

void
assign(
  Function_Builder *fn,
  Operand a,
  Operand b
) {
  encode(&fn->buffer, (Instruction) {mov, {a, b}});
}

Operand
mutating_plus(
  Function_Builder *fn,
  Operand a,
  Operand b
) {
  // TODO Create a temp value for result
  encode(&fn->buffer, (Instruction) {add, {a, b}});
  return a;
}

Function_Builder
fn_begin() {
  Function_Builder fn = {
    .stack_reserve = 0x0,
    .buffer = make_buffer(1024, PAGE_EXECUTE_READWRITE),
    .descriptor = (const Descriptor_Function) {0}
  };

  // @Volatile @ArgumentCount
  fn.descriptor.argument_list = malloc(sizeof(Value) * 4);
  fn.descriptor.returns = malloc(sizeof(Value));

  // @Volatile @ReserveStack
  encode(&fn.buffer, (Instruction) {sub, {rsp, imm8(0xcc)}});
  return fn;
}

Value
fn_end(
  Function_Builder *builder
) {
  u8 alignment = 0x8;
  u8 stack_size = builder->stack_reserve + alignment;

  { // Override stack reservation
    u64 save_occupied = builder->buffer.occupied;
    builder->buffer.occupied = 0;

    // @Volatile @ReserveStack
    encode(&builder->buffer, (Instruction) {sub, {rsp, imm8(stack_size)}});
    builder->buffer.occupied = save_occupied;
  }

  encode(&builder->buffer, (Instruction) {add, {rsp, imm8(stack_size)}});
  encode(&builder->buffer, (Instruction) {ret, {0}});

  builder->descriptor.argument_count = builder->next_argument_index;
  return (const Value) {
    .descriptor = {
      .type = Descriptor_Type_Function,
      .function = builder->descriptor,
    },
    .operand = imm64((s64) builder->buffer.memory)
  };
}

Value
fn_arg(
  Function_Builder *fn,
  Descriptor descriptor
) {
  //assert(byte_size == 8);
  switch (fn->next_argument_index++) {
    case 0: {
      Value arg = {
        .descriptor = descriptor,
        .operand = rcx,
      };
      fn->descriptor.argument_list[0] = arg;
      return arg;
    }
    case 1: {
      Value arg = {
        .descriptor = descriptor,
        .operand = rdx,
      };
      fn->descriptor.argument_list[1] = arg;
      return arg;
    }
    case 2: {
      Value arg = {
        .descriptor = descriptor,
        .operand = r8,
      };
      fn->descriptor.argument_list[2] = arg;
      return arg;
    }
    case 3: {
      Value arg = {
        .descriptor = descriptor,
        .operand = r9,
      };
      fn->descriptor.argument_list[3] = arg;
      return arg;
    }
  }
  // @Volatile @ArgumentCount
  assert(!"More than 4 arguments are not supported at the moment.");
  return (const Value){0};
}

void
fn_return(
  Function_Builder *fn,
  Value to_return
) {
  // FIXME check that all return paths return the same type
  *fn->descriptor.returns = to_return;

  if (to_return.descriptor.type != Descriptor_Type_Void) {
    if (memcmp(&rax, &to_return, sizeof(rax)) != 0) {
      encode(&fn->buffer, (Instruction) {mov, {rax, to_return.operand}});
    }
  }
}

Value
make_constant_s32(
  s32 integer
) {
  Function_Builder builder = fn_begin();
  {
    fn_return(&builder, value_from_s32(integer));
  }
  return fn_end(&builder);
}

Value
make_identity_s64() {
  Function_Builder builder = fn_begin();
  {
    Value arg0 = fn_arg(&builder, descriptor_integer);
    fn_return(&builder, arg0);
  }
  return fn_end(&builder);
}

Value
make_increment_s64() {
  Function_Builder builder = fn_begin();
  {
    Operand x = declare_variable(&builder, sizeof(s64));
    assign(&builder, x, imm32(1));
    Operand y = declare_variable(&builder, sizeof(s64));
    assign(&builder, y, imm32(2));
    Value arg0 = fn_arg(&builder, descriptor_integer);
    mutating_plus(&builder, arg0.operand, x);
    mutating_plus(&builder, arg0.operand, y);
    fn_return(&builder, arg0);
  }
  return fn_end(&builder);
}

Value
call_function_value(
  Function_Builder *builder,
  Value *to_call,
  Value *argument_list,
  s64 argument_count
) {
  assert(to_call->descriptor.type == Descriptor_Type_Function);
  Descriptor_Function *descriptor = &to_call->descriptor.function;
  assert(descriptor->argument_count == argument_count);

  for (s64 i = 0; i < argument_count; ++i) {
    // FIXME add proper type checks for arguments
    assert(descriptor->argument_list[i].descriptor.type == argument_list[i].descriptor.type);
    encode(
      &builder->buffer,
      (Instruction) {mov, {descriptor->argument_list[i].operand, argument_list[i].operand}}
     );
  }

  encode(&builder->buffer, (Instruction) {mov, {rax, to_call->operand}});
  encode(&builder->buffer, (Instruction) {call, {rax, 0}});

  return *descriptor->returns;
}

Value
make_call_no_arg_return_s32() {
  Function_Builder builder = fn_begin();
  {
    Value *returns = malloc(sizeof(Value));
    *returns = (const Value){0};
    returns->descriptor = descriptor_integer;
    returns->operand = rax;

    Value arg0 = fn_arg(&builder, (const Descriptor){
      .type = Descriptor_Type_Function,
      .function = {
        .argument_list = 0,
        .argument_count = 0,
        .returns = returns,
      }
    });

    Value result = call_function_value(&builder, &arg0, 0, 0);
    fn_return(&builder, result);
  }
  return fn_end(&builder);
}

Value
make_partial_application_s64(
  Value *original_fn,
  s64 arg
) {
  Function_Builder builder = fn_begin();
  {
    Value applied_arg0 = value_from_s64(arg);
    Value result = call_function_value(&builder, original_fn, &applied_arg0, 1);
    fn_return(&builder, result);
  }
  return fn_end(&builder);
}

typedef struct {
  s32 *location;
  u64 ip;
} Patch_32;

Patch_32
make_jnz(
  Function_Builder *fn
) {
  encode(&fn->buffer, (Instruction) {jnz, {imm32(0xcc), 0}});
  u64 ip = fn->buffer.occupied;
  s32 *location = (s32 *)(fn->buffer.memory + fn->buffer.occupied - sizeof(s32));
  return (const Patch_32) { .location = location, .ip = ip };
}

Patch_32
make_jmp(
  Function_Builder *fn
) {
  encode(&fn->buffer, (Instruction) {jmp, {imm32(0xcc), 0}});
  u64 ip = fn->buffer.occupied;
  s32 *location = (s32 *)(fn->buffer.memory + fn->buffer.occupied - sizeof(s32));
  return (const Patch_32) { .location = location, .ip = ip };
}

void
patch_jump_to_here(
  Function_Builder *fn,
  Patch_32 patch
) {
  *patch.location = (s32) (fn->buffer.occupied - patch.ip);
}

Value
make_is_non_zero() {
  Function_Builder builder = fn_begin();
  {
    encode(&builder.buffer, (Instruction) {cmp, {ecx, imm32(0)}});
    Patch_32 patch = make_jnz(&builder);

    fn_return(&builder, value_from_s64(0));
    Patch_32 return_patch = make_jmp(&builder);
    patch_jump_to_here(&builder, patch);

    fn_return(&builder, value_from_s64(1));
    patch_jump_to_here(&builder, return_patch);
  }
  return fn_end(&builder);
}

spec("mass") {
  it("should create function that will return 42") {
    Value the_answer = make_constant_s32(42);
    s32 result = value_as_function(&the_answer, fn_type_void_to_s32)();
    check(result == 42);
  }

  it("should create function that returns s64 value that was passed") {
    Value id_value = make_identity_s64();
    fn_type_s64_to_s64 id_s64 = (fn_type_s64_to_s64)id_value.operand.imm64;
    s64 result = id_s64(42);
    check(result == 42);
  }

  it("should create function increments s64 value passed to it") {
    Value add_3_s64 = make_increment_s64();
    s64 result = value_as_function(&add_3_s64, fn_type_s64_to_s64)(42);
    check(result == 45);
  }

  it("should create a function to call a no argument fn") {
    Value the_answer = make_constant_s32(42);
    Value caller= make_call_no_arg_return_s32();
    s32 result = value_as_function(&caller, fn_type__void_to_s32__to_s32)(
      value_as_function(&the_answer, fn_type_void_to_s32)
    );
    check(result == 42);
  }

  it("should create a partially applied function") {
    Value id_value = make_identity_s64();
    Value partial_fn_value = make_partial_application_s64(&id_value, 42);
    fn_type_void_to_s64 the_answer = value_as_function(&partial_fn_value, fn_type_void_to_s64);
    s64 result = the_answer();
    check(result == 42);
  }

  it("should have a function that returns 0 if arg is zero, 1 otherwise") {
    Value is_non_zero_value = make_is_non_zero();
    fn_type_s32_to_s32 is_non_zero = value_as_function(&is_non_zero_value, fn_type_s32_to_s32);
    s64 result = is_non_zero(0);
    check(result == 0);
    result = is_non_zero(42);
    check(result == 1);
  }

  it("should return 3rd argument") {
    Function_Builder builder = fn_begin();
    {
      (void)fn_arg(&builder, descriptor_integer);
      (void)fn_arg(&builder, descriptor_integer);
      Value arg2 = fn_arg(&builder, descriptor_integer);
      fn_return(&builder, arg2);
    }
    Value value = fn_end(&builder);
    fn_type_s64_s64_s64_to_s64 third = value_as_function(&value, fn_type_s64_s64_s64_to_s64);

    s64 result = third(1, 2, 3);
    check(result == 3);
  }

  it("should parse c function forward declarations") {
    c_function_value("void fn_void()", 0);
    c_function_value("void fn_int(int)", 0);
    Value explicit_void_arg = c_function_value("void fn_void(void)", 0);
    check(explicit_void_arg.descriptor.function.argument_count == 0);
  }

  it("should say 'Hello, world!'") {
    const char *message = "Hello, world!";
    Descriptor message_descriptor = {
      .type = Descriptor_Type_Fixed_Size_Array,
      .array = {
        .item = &descriptor_integer,
        .length = strlen(message + 1/* null terminator */),
      },
    };

    Value message_value = {
      .descriptor = {
        .type = Descriptor_Type_Pointer,
        .pointer_to = &message_descriptor,
      },
      .operand = imm64((s64) message),
    };

    Value puts_value = c_function_value("int puts(const char*)", (fn_type_opaque) puts);

    Function_Builder builder = fn_begin();
    {
      call_function_value(&builder, &puts_value, &message_value, 1);
      fn_return(&builder, void_value);
    }
    Value value = fn_end(&builder);

    value_as_function(&value, fn_type_void_to_void)();
  }
}
