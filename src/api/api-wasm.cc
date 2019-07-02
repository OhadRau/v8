// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/api/api-wasm.h"
#include "src/common/globals.h"
#include "src/api/api-inl.h"
#include "src/handles/handles.h"
#include "src/objects/contexts.h"
#include "src/objects/lookup.h"
#include "src/execution/isolate.h"
#include "src/wasm/value-type.h"
#include "src/wasm/wasm-objects.h"

#include <cstdio>

#define IGNORE(X) ((void) (X))

namespace i = v8::internal;

namespace v8 {
namespace wasm {

Memory::Memory(size_t pages, uint8_t* data) : pages_(pages), data_(data) {}
size_t Memory::size() { return pages_ * PAGE_SIZE; }
size_t Memory::pages() { return pages_; }
uint8_t* Memory::data() { return data_; }

Context::Context(Memory *memory) {
  this->isolate = Isolate::GetCurrent();
  this->memory = memory;
}

Context::Context(Memory *memory, Isolate *isolate) {
  this->isolate = isolate;
  this->memory = memory;
}

Context::~Context() {}

Val::Val(ValKind kind, value value) : kind_(kind), value_(value) {}

Val::Val(int32_t i) : kind_(I32) { value_.i32 = i; }
Val::Val(int64_t i) : kind_(I64) { value_.i64 = i; }
Val::Val(float i) : kind_(F32) { value_.f32 = i; }
Val::Val(double i) : kind_(F64) { value_.f64 = i; }
Val::Val(void *r) : kind_(ANYREF) { value_.ref = r; }

ValKind Val::kind() { return kind_; }
int32_t Val::i32() { assert(kind_ == I32); return value_.i32; }
int64_t Val::i64() { assert(kind_ == I64); return value_.i64; }
float Val::f32() { assert(kind_ == F32); return value_.f32; }
double Val::f64() { assert(kind_ == F64); return value_.f64; }

FuncType::FuncType(
  std::vector<ValKind> params, std::vector<ValKind> results
) : params_(params), results_(results) {}

std::vector<ValKind> FuncType::params() const { return params_; }
std::vector<ValKind> FuncType::results() const { return results_; }

Func::Func(const FuncType* funcType, callbackType cb) :
  type_(funcType),
  callback_(cb) {}

const FuncType* Func::type() {
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
    i::Isolate* isolate, const FuncType* type) {
  puts("[WASM-PL] Create PodArray");
  int sig_size =
      static_cast<int>(type->params().size() + type->results().size() + 1);
  printf("[WASM-PL] Sig size: %d\n", sig_size);
  i::Handle<i::PodArray<i::wasm::ValueType>> sig =
      i::PodArray<i::wasm::ValueType>::New(isolate, sig_size,
                                           i::AllocationType::kOld);
  puts("[WASM-PL] Copy v8 types");
  int index = 0;
  // TODO(jkummerow): Consider making vec<> range-based for-iterable.
  for (size_t i = 0; i < type->results().size(); i++) {
    sig->set(index++, wasm_valtype_to_v8(type->results()[i]));
  }
  // {sig->set} needs to take the address of its second parameter,
  // so we can't pass in the static const kMarker directly.
  i::wasm::ValueType marker = kMarker;
  sig->set(index++, marker);
  for (size_t i = 0; i < type->params().size(); i++) {
    sig->set(index++, wasm_valtype_to_v8(type->params()[i]));
  }
  puts("[WASM-PL] Serialized");
  return sig;
}

void PreloadNative(Isolate* isolate,
                   const char* module_name,
                   const char* name,
                   Func* import) {
  puts("[WASM-PL] Get isolate");
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  i::HandleScope handle_scope(i_isolate);

  puts("[WASM-PL] Select imports");
  Eternal<i::JSObject> imports = i_isolate->wasm_native_imports();
  if (imports.IsEmpty()) {
    i::Handle<i::JSObject> handle =
        i_isolate->factory()->NewJSObject(i_isolate->object_function());
    Local<i::JSObject> local =
        Utils::Convert<i::JSObject, i::JSObject>(handle);
    imports.Set(isolate, local);
  }

  Local<i::JSObject> imports_local = imports.Get(isolate);
  i::Handle<i::JSObject> imports_handle =
    i::Handle<i::JSObject>(reinterpret_cast<i::Address*>(*imports_local));

  i::Handle<i::String> module_str =
      i_isolate->factory()->NewStringFromAsciiChecked(module_name);
  i::Handle<i::String> name_str =
      i_isolate->factory()->NewStringFromAsciiChecked(name);

  puts("[WASM-PL] Create object");
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
  puts("[WASM-PL] Create callback");
  // TODO(ohadrau): Is this the right embedder data to pass in?
  i::Handle<i::WasmPreloadFunction> callback =
    i::WasmPreloadFunction::New(
      i_isolate, reinterpret_cast<i::Address>(import->callback()),
      nullptr, Serialize(i_isolate, import->type()));
  IGNORE(i::Object::SetProperty(i_isolate, module_obj, name_str, callback));

  puts("[WASM-PL] Update native imports");
  i_isolate->set_wasm_native_imports(imports);
  assert(imports.Get(isolate) == i_isolate->wasm_native_imports().Get(isolate));
  puts("[WASM-PL] Assertion success: imports_handle set.");
}

}  // namespace wasm
}  // namespace v8

#undef IGNORE
