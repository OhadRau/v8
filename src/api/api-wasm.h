// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_API_API_WASM_H_
#define V8_API_API_WASM_H_

#include <cassert>
#include <cstdint>
#include <cstddef>
#include "include/v8.h"
#include "third_party/wasm-api/wasm.hh"

namespace v8 {
namespace wasm {

#define PAGE_SIZE 0x10000

class Memory {
  size_t pages_;
  byte_t* data_;
 public:
  Memory(size_t pages, byte_t* data) : pages_(pages), data_(data) {}
  size_t size() { return pages_ * PAGE_SIZE; }
  size_t pages() { return pages_; }
  byte_t* data() { return data_; }
};

class Context {
 public:
  Context(::wasm::Memory *memory);
  Context(::wasm::Memory *memory, v8::Isolate *isolate);
  ~Context();
  ::wasm::Memory *memory;
  v8::Isolate *isolate;
};

enum ValKind : uint8_t {
  I32,
  I64,
  F32,
  F64,
  ANYREF = 128,
  FUNCREF
};

// TODO: Should we enforce Ref ownership like the C API?
class Val {
  ValKind kind_;
  union value {
    int32_t i32;
    int64_t i64;
    float32_t f32;
    float64_t f64;
    void *ref;
  } value_;

  Val(ValKind kind, value value) : kind_(kind), value_(value) {}

 public:
  Val() : kind_(ANYREF) { value_.ref = NULL; }
  Val(int32_t i) : kind_(I32) { value_.i32 = i; }
  Val(int64_t i) : kind_(I64) { value_.i64 = i; }
  Val(float32_t i) : kind_(F32) { value_.f32 = i; }
  Val(float64_t i) : kind_(F64) { value_.f64 = i; }
  Val(void *r) : kind_(ANYREF) { value_.ref = r; }

  ValKind kind() { return kind_; }
  int32_t i32() { assert(kind_ == I32); return value_.i32; }
  int64_t i64() { assert(kind_ == I64); return value_.i64; }
  float32_t f32() { assert(kind_ == F32); return value_.f32; }
  float64_t f64() { assert(kind_ == F64); return value_.f64; }
};

class FuncType {
  std::vector<ValKind> params_, results_;
 public:
  FuncType(std::vector<ValKind> params, std::vector<ValKind> results) : params_(params), results_(results) {}

  std::vector<ValKind> params() { return params_; }
  std::vector<ValKind> results() { return results_; }
};

class Func {
 public:
  // TODO: Specify a better return value (Trap)
  using callbackType = void (*)(const Context, const Val[], Val[]);

  Func(FuncType, callbackType);

  FuncType type();
  callbackType callback();
 private:
  FuncType type_;
  callbackType callback_;
};

void PreloadNative(Isolate* isolate,
                   const char* name,
                   Func* import);

}  // namespace wasm
}  // namespace v8

#endif