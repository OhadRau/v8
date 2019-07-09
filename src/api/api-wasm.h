// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_API_API_WASM_H_
#define V8_API_API_WASM_H_

#include <cassert>
#include <cstdint>
#include <cstddef>
#include "include/v8.h"

namespace v8 {
namespace wasm {

#define PAGE_SIZE 0x10000

class Memory {
 private:
  size_t pages_;
  uint8_t* data_;
 public:
  Memory(size_t pages, uint8_t* data);
  size_t size();
  size_t pages();
  uint8_t* data();
};

class Context {
 public:
  explicit Context(Memory* memory);
  Context(Memory* memory, v8::Isolate* isolate);
  Memory* memory;
  v8::Isolate* isolate;
};

enum ValKind : uint8_t {
  I32,
  I64,
  F32,
  F64,
  ANYREF = 128,
  FUNCREF
};

// TODO(ohadrau): Should we enforce Ref ownership like the C API?
class Val {
 private:
  ValKind kind_;
  union value {
    int32_t i32;
    int64_t i64;
    float f32;
    double f64;
    void* ref;
  } value_;

  Val(ValKind kind, value value);

 public:
  Val() : kind_(ANYREF) { value_.ref = NULL; }
  explicit Val(int32_t i);
  explicit Val(int64_t i);
  explicit Val(float i);
  explicit Val(double i);
  explicit Val(void* r);

  ValKind kind() const;
  int32_t i32() const;
  int64_t i64() const;
  float f32() const;
  double f64() const;
  void* ref() const;
};

class FuncType {
 private:
  std::vector<ValKind> params_, results_;
 public:
  FuncType(std::vector<ValKind> params, std::vector<ValKind> results);

  std::vector<ValKind> params() const;
  std::vector<ValKind> results() const;
};

class Func {
  friend struct FuncData;
  // TODO(ohadrau): Specify a better return value (Trap)
  using callbackType = void (*)(const Memory*, const Val[], Val[]);
 private:
  const FuncType* type_;
  callbackType callback_;
 public:
  Func(const FuncType*, callbackType);

  const FuncType* type();
  callbackType callback();
};

struct FuncData {
  v8::internal::Isolate* isolate;
  const FuncType* type;
  Func::callbackType callback;

  FuncData(v8::internal::Isolate* isolate, const FuncType* type)
      : isolate(isolate),
        type(type) {}

  static v8::internal::Address v8_callback(
      void* data,
      v8::internal::Address argv,
      size_t memoryPages,
      uint8_t* memoryBase);
};

void PreloadNative(Isolate* isolate,
                   const char* module_name,
                   const char* name,
                   Func* import);

}  // namespace wasm
}  // namespace v8

#endif  // V8_API_API_WASM_H_
