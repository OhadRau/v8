// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <unordered_map>
#include "src/api/api-wasm.h"
#include "include/v8.h"
#include "include/v8-internal.h"
#include "src/common/globals.h"
#include "src/api/api-inl.h"
#include "src/handles/handles.h"
#include "src/objects/contexts.h"
#include "src/objects/lookup.h"
#include "src/execution/isolate.h"
#include "src/wasm/value-type.h"
#include "src/wasm/wasm-objects.h"

#define IGNORE(X) ((void) (X))

namespace i = v8::internal;

namespace v8 {
namespace wasm {

Context::Context(Memory *memory) {
  this->isolate = Isolate::GetCurrent();
  this->memory = memory;
}

Context::Context(Memory *memory, Isolate *isolate) {
  this->isolate = isolate;
  this->memory = memory;
}

Context::~Context() {}

Func::Func(FuncType funcType, callbackType cb) :
  type_(funcType),
  callback_(cb) {}

FuncType Func::type() {
  return type_;
}
Func::callbackType Func::callback() {
  return callback_;
}

i::wasm::ValueType wasm_valtype_to_v8(ValKind type) {
  switch (type) {
    case I32:
      return i::wasm::kWasmI32;
    case I64:
      return i::wasm::kWasmI64;
    case F32:
      return i::wasm::kWasmF32;
    case F64:
      return i::wasm::kWasmF64;
    default:
      // TODO(wasm+): support new value types
      UNREACHABLE();
  }
}

// TODO(ohadrau): Clean up so that we're not maintaining 2 copies of this
// Use an invalid type as a marker separating params and results.
static const i::wasm::ValueType kMarker = i::wasm::kWasmStmt;

static i::Handle<i::PodArray<i::wasm::ValueType>> Serialize(
    i::Isolate* isolate, FuncType type) {
  int sig_size =
      static_cast<int>(type.params().size() + type.results().size() + 1);
  i::Handle<i::PodArray<i::wasm::ValueType>> sig =
      i::PodArray<i::wasm::ValueType>::New(isolate, sig_size,
                                            i::AllocationType::kOld);
  int index = 0;
  // TODO(jkummerow): Consider making vec<> range-based for-iterable.
  for (size_t i = 0; i < type.results().size(); i++) {
    sig->set(index++,
              v8::wasm::wasm_valtype_to_v8(type.results()[i]));
  }
  // {sig->set} needs to take the address of its second parameter,
  // so we can't pass in the static const kMarker directly.
  i::wasm::ValueType marker = kMarker;
  sig->set(index++, marker);
  for (size_t i = 0; i < type.params().size(); i++) {
    sig->set(index++,
              v8::wasm::wasm_valtype_to_v8(type.params()[i]));
  }
  return sig;
}

void PreloadNative(Isolate* isolate,
                   const char* moduleName,
                   const char* name,
                   Func* import) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  i::HandleScope handle_scope(i_isolate);

  i::JSObject imports_obj = i_isolate->wasm_native_imports();
  i::Handle<i::JSObject> imports_handle(imports_obj, i_isolate);
  auto type = import->type();
  i::Handle<i::String> module_str =
      i_isolate->factory()->NewStringFromAsciiChecked(moduleName);
  i::Handle<i::String> name_str =
      i_isolate->factory()->NewStringFromAsciiChecked(name);

  i::Handle<i::JSObject> module_obj;
  i::LookupIterator module_it(i_isolate, imports_handle, module_str,
                              i::LookupIterator::OWN_SKIP_INTERCEPTOR);
  if (i::JSObject::HasProperty(&module_it).ToChecked()) {
    module_obj = i::Handle<i::JSObject>::cast(
        i::Object::GetProperty(&module_it).ToHandleChecked());
  } else {
    module_obj =
        i_isolate->factory()->NewJSObject(i_isolate->object_function());
    IGNORE(i::Object::SetProperty(i_isolate, imports_handle,
                                  module_str, module_obj));
  }
  // TODO(ohadrau): Is this the right embedder data to pass in?
  i::Handle<i::WasmPreloadFunction> callback =
    i::WasmPreloadFunction::New(
      i_isolate, reinterpret_cast<i::Address>(import->callback()),
      nullptr, Serialize(i_isolate, import->type()));
  IGNORE(i::Object::SetProperty(i_isolate, module_obj, name_str, callback));

  i_isolate->set_wasm_native_imports(*imports_handle);
}

}  // namespace wasm
}  // namespace v8

#undef IGNORE
