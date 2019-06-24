// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <unordered_map>
#include "api-wasm.h"
#include "include/v8.h"
#include "include/v8-internal.h"
#include "src/handles/handles.h"
#include "src/objects/contexts.h"
#include "src/execution/isolate.h"
#include "third_party/wasm-api/wasm.hh"

namespace i = v8::internal;

namespace v8 {
namespace wasm {

Context::Context(::wasm::Memory *memory) {
  this->isolate = Isolate::GetCurrent();
  this->memory = memory;
}

Context::Context(::wasm::Memory *memory, Isolate *isolate) {
  this->isolate = isolate;
  this->memory = memory;
}

Context::~Context() {}

Func::Func(FuncType funcType, callbackType cb) : type_(funcType), callback_(cb) {}

FuncType Func::type() {
  return type_;
}
Func::callbackType Func::callback() {
  return callback_;
}

// QUESTION: Should we make this return a JSFunction, so we can pass this thru the JS API?
void PreloadNative(Isolate* isolate,
                   const char* name,
                   Func* import) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  //i::HandleScope handle_scope(i_isolate);

  auto map = i_isolate->wasm_native_imports();
  if (!map) {
    map = new std::unordered_map<const char *, Func *>();
    i_isolate->set_wasm_native_imports(map);
  }
  //JSFunction func = i_isolate->factory()->
  map->insert({{ name, import }});
}

}  // namespace wasm
}  // namespace v8