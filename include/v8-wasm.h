// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INCLUDE_V8_WASM_H_
#define INCLUDE_V8_WASM_H_

#include <cstdint>
#include <cstddef>
#include "v8.h"

namespace v8 {
namespace wasm {

class V8_EXPORT Memory {
 public:
  Memory(size_t pages, uint8_t* data);
  size_t size();
  size_t pages();
  uint8_t* data();
};

class V8_EXPORT Context {
 public:
  explicit Context(Memory *memory);
  Context(Memory *memory, Isolate *isolate);
  ~Context();
  Memory *memory;
  Isolate *isolate;
};

enum V8_EXPORT ValKind : uint8_t {
  I32,
  I64,
  F32,
  F64,
  ANYREF = 128,
  FUNCREF
};

class V8_EXPORT Val {
 public:
  Val();
  explicit Val(int32_t i);
  explicit Val(int64_t i);
  explicit Val(float i);
  explicit Val(double i);
  explicit Val(void *r);

  ValKind kind();
  int32_t i32();
  int64_t i64();
  float f32();
  double f64();
};

class V8_EXPORT FuncType {
 public:
  FuncType(std::vector<ValKind> params, std::vector<ValKind> results);

  std::vector<ValKind> params() const;
  std::vector<ValKind> results() const;
};

class V8_EXPORT Func {
 public:
  // TODO(ohadrau): Specify a better return value (Trap)
  using callbackType = void *(*)(const Memory*, const Val[], Val[]);

  Func(const FuncType*, callbackType);

  const FuncType* type();
  callbackType callback();
};

void PreloadNative(Isolate* isolate,
                   const char* module_name,
                   const char* name,
                   Func* import);

}  // namespace wasm
}  // namespace v8

#endif  // INCLUDE_V8_WASM_H_
