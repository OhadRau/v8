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

Context::Context(Memory* memory) {
  this->isolate = Isolate::GetCurrent();
  this->memory = memory;
}

Context::Context(Memory* memory, Isolate* isolate) {
  this->isolate = isolate;
  this->memory = memory;
}

Val::Val(ValKind kind, value value) : kind_(kind), value_(value) {}

Val::Val(int32_t i) : kind_(I32) { value_.i32 = i; }
Val::Val(int64_t i) : kind_(I64) { value_.i64 = i; }
Val::Val(float i) : kind_(F32) { value_.f32 = i; }
Val::Val(double i) : kind_(F64) { value_.f64 = i; }
Val::Val(void* r) : kind_(ANYREF) { value_.ref = r; }

ValKind Val::kind() const { return kind_; }
int32_t Val::i32() const { assert(kind_ == I32); return value_.i32; }
int64_t Val::i64() const { assert(kind_ == I64); return value_.i64; }
float Val::f32() const { assert(kind_ == F32); return value_.f32; }
double Val::f64() const { assert(kind_ == F64); return value_.f64; }
void* Val::ref() const {
  assert(kind_ == ANYREF || kind_ == FUNCREF);
  return value_.ref;
}

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

i::Address FuncData::v8_callback(
  void* data, i::Address argv,
  size_t memoryPages, uint8_t* memoryBase
) {
  FuncData* self = reinterpret_cast<FuncData*>(data);
  i::Isolate* isolate = self->isolate;
  puts("[WASM-PL] Got self + isolate");

  const std::vector<ValKind>& param_types = self->type->params();
  const std::vector<ValKind>& result_types = self->type->results();
  puts("[WASM-PL] Got param/result types");

  int num_param_types = static_cast<int>(param_types.size());
  int num_result_types = static_cast<int>(result_types.size());

  std::unique_ptr<Val[]> params(new Val[num_param_types]);
  std::unique_ptr<Val[]> results(new Val[num_result_types]);
  i::Address p = argv;
  for (int i = 0; i < num_param_types; ++i) {
    switch (param_types[i]) {
      case I32:
        params[i] = Val(i::ReadUnalignedValue<int32_t>(p));
        p += 4;
        break;
      case I64:
        params[i] = Val(i::ReadUnalignedValue<int64_t>(p));
        p += 8;
        break;
      case F32:
        params[i] = Val(i::ReadUnalignedValue<float>(p));
        p += 4;
        break;
      case F64:
        params[i] = Val(i::ReadUnalignedValue<double>(p));
        p += 8;
        break;
      case ANYREF:
      case FUNCREF: {
        i::Address raw = i::ReadUnalignedValue<i::Address>(p);
        p += sizeof(raw);
        if (raw == i::kNullAddress) {
          params[i] = Val(nullptr);
        } else {
          i::JSReceiver raw_obj = i::JSReceiver::cast(i::Object(raw));
          i::Handle<i::JSReceiver> obj(raw_obj, raw_obj.GetIsolate());
          params[i] = Val(reinterpret_cast<void*>(obj->address()));
        }
        break;
      }
    }
  }
  puts("[WASM-PL] Got params");

  const Memory* memory = new Memory(memoryPages, memoryBase);
  self->callback(memory, params.get(), results.get());
  puts("[WASM-PL] Called callback");

  if (isolate->has_scheduled_exception()) {
    isolate->PromoteScheduledException();
  }
  if (isolate->has_pending_exception()) {
    i::Object ex = isolate->pending_exception();
    isolate->clear_pending_exception();
    return ex.ptr();
  }
  puts("[WASM-PL] Checked pending exceptions");

  p = argv;
  for (int i = 0; i < num_result_types; ++i) {
    switch (result_types[i]) {
      case I32:
        i::WriteUnalignedValue(p, results[i].i32());
        p += 4;
        break;
      case I64:
        i::WriteUnalignedValue(p, results[i].i64());
        p += 8;
        break;
      case F32:
        i::WriteUnalignedValue(p, results[i].f32());
        p += 4;
        break;
      case F64:
        i::WriteUnalignedValue(p, results[i].f64());
        p += 8;
        break;
      case ANYREF:
      case FUNCREF: {
        if (results[i].ref() == nullptr) {
          i::WriteUnalignedValue(p, i::kNullAddress);
        } else {
          i::WriteUnalignedValue(p, results[i].ref());
        }
        p += sizeof(i::Address);
        break;
      }
    }
  }
  puts("[WASM-PL] Got results");
  return i::kNullAddress;
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
  FuncData* data = new FuncData(i_isolate, import->type());;
  i::Handle<i::WasmPreloadFunction> callback =
      i::WasmPreloadFunction::New(
        i_isolate, reinterpret_cast<i::Address>(&FuncData::v8_callback),
        data, Serialize(i_isolate, import->type()));
  /*i::Handle<i::WasmPreloadFunction> callback =
    i::WasmPreloadFunction::New(
      i_isolate, reinterpret_cast<i::Address>(import->callback()),
      nullptr, Serialize(i_isolate, import->type()));
  i::Handle<i::WasmCapiFunction> function = i::WasmCapiFunction::New(
      isolate, reinterpret_cast<i::Address>(&FuncData::v8_callback), data,
      SignatureHelper::Serialize(isolate, data->type.get()));*/
  IGNORE(i::Object::SetProperty(i_isolate, module_obj, name_str, callback));

  puts("[WASM-PL] Update native imports");
  i_isolate->set_wasm_native_imports(imports);
  assert(imports.Get(isolate) == i_isolate->wasm_native_imports().Get(isolate));
  puts("[WASM-PL] Assertion success: imports_handle set.");
}

}  // namespace wasm
}  // namespace v8

#undef IGNORE
