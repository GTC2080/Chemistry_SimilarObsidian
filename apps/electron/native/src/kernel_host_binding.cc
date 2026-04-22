#include <napi.h>

#include "kernel/c_api.h"
#include "kernel/types.h"

#ifdef _WIN32
#include <Windows.h>
#endif

#include <new>
#include <string>

namespace {

struct KernelSessionBox {
  kernel_handle* handle = nullptr;
};

template <typename TResult, void (*TFreeFn)(TResult*)>
struct OwnedKernelResult {
  TResult value{};

  ~OwnedKernelResult() {
    TFreeFn(&value);
  }

  TResult* out() {
    return &value;
  }
};

const char* KernelErrorCodeName(const kernel_error_code code) {
  switch (code) {
    case KERNEL_OK:
      return "KERNEL_OK";
    case KERNEL_ERROR_INVALID_ARGUMENT:
      return "KERNEL_ERROR_INVALID_ARGUMENT";
    case KERNEL_ERROR_NOT_FOUND:
      return "KERNEL_ERROR_NOT_FOUND";
    case KERNEL_ERROR_CONFLICT:
      return "KERNEL_ERROR_CONFLICT";
    case KERNEL_ERROR_IO:
      return "KERNEL_ERROR_IO";
    case KERNEL_ERROR_INTERNAL:
      return "KERNEL_ERROR_INTERNAL";
    case KERNEL_ERROR_TIMEOUT:
      return "KERNEL_ERROR_TIMEOUT";
  }

  return "KERNEL_ERROR_UNKNOWN";
}

std::string ActiveCodePageToUtf8(const std::string& value) {
#ifdef _WIN32
  if (value.empty()) {
    return {};
  }

  const int wide_size = MultiByteToWideChar(
      CP_ACP,
      0,
      value.c_str(),
      static_cast<int>(value.size()),
      nullptr,
      0);
  if (wide_size <= 0) {
    return {};
  }

  std::wstring wide_value(static_cast<std::size_t>(wide_size), L'\0');
  MultiByteToWideChar(
      CP_ACP,
      0,
      value.c_str(),
      static_cast<int>(value.size()),
      wide_value.data(),
      wide_size);

  const int utf8_size = WideCharToMultiByte(
      CP_UTF8,
      0,
      wide_value.c_str(),
      wide_size,
      nullptr,
      0,
      nullptr,
      nullptr);
  if (utf8_size <= 0) {
    return {};
  }

  std::string utf8_value(static_cast<std::size_t>(utf8_size), '\0');
  WideCharToMultiByte(
      CP_UTF8,
      0,
      wide_value.c_str(),
      wide_size,
      utf8_value.data(),
      utf8_size,
      nullptr,
      nullptr);
  return utf8_value;
#else
  return value;
#endif
}

std::string Utf16ToActiveCodePage(const std::u16string& value) {
#ifdef _WIN32
  if (value.empty()) {
    return {};
  }

  const std::wstring wide_value(value.begin(), value.end());
  const int required_size = WideCharToMultiByte(
      CP_ACP,
      0,
      wide_value.c_str(),
      -1,
      nullptr,
      0,
      nullptr,
      nullptr);
  if (required_size <= 0) {
    return {};
  }

  std::string encoded_path(static_cast<std::size_t>(required_size), '\0');
  WideCharToMultiByte(
      CP_ACP,
      0,
      wide_value.c_str(),
      -1,
      encoded_path.data(),
      required_size,
      nullptr,
      nullptr);
  if (!encoded_path.empty() && encoded_path.back() == '\0') {
    encoded_path.pop_back();
  }
  return encoded_path;
#else
  return std::string(value.begin(), value.end());
#endif
}

void ThrowBindingError(
    Napi::Env env,
    const std::string& code,
    const std::string& message,
    const std::string& operation,
    const int32_t kernel_code = -1) {
  Napi::Error error = Napi::Error::New(env, message);
  error.Value().Set("code", Napi::String::New(env, code));
  error.Value().Set("operation", Napi::String::New(env, operation));
  if (kernel_code >= 0) {
    error.Value().Set("kernelCode", Napi::Number::New(env, kernel_code));
  }
  error.ThrowAsJavaScriptException();
}

void ThrowKernelStatusError(
    Napi::Env env,
    const kernel_status status,
    const std::string& operation,
    const std::string& message_prefix) {
  const std::string kernel_code_name = KernelErrorCodeName(status.code);
  ThrowBindingError(
      env,
      kernel_code_name,
      message_prefix + " (" + kernel_code_name + ").",
      operation,
      static_cast<int32_t>(status.code));
}

void FinalizeKernelSession(Napi::Env /* env */, KernelSessionBox* session) {
  if (session == nullptr) {
    return;
  }

  if (session->handle != nullptr) {
    kernel_close(session->handle);
    session->handle = nullptr;
  }

  delete session;
}

KernelSessionBox* RequireSessionBox(
    const Napi::CallbackInfo& info,
    const size_t index,
    const std::string& operation) {
  Napi::Env env = info.Env();
  if (info.Length() <= index || !info[index].IsExternal()) {
    ThrowBindingError(
        env,
        "BINDING_INVALID_SESSION_HANDLE",
        "Kernel session handle is missing or invalid.",
        operation);
    return nullptr;
  }

  auto* session = info[index].As<Napi::External<KernelSessionBox>>().Data();
  if (session == nullptr || session->handle == nullptr) {
    ThrowBindingError(
        env,
        "BINDING_SESSION_CLOSED",
        "Kernel session handle is already closed.",
        operation);
    return nullptr;
  }

  return session;
}

Napi::Object RequireObjectArgument(
    const Napi::CallbackInfo& info,
    const size_t index,
    const std::string& operation,
    const std::string& argument_name) {
  Napi::Env env = info.Env();
  if (info.Length() <= index || !info[index].IsObject()) {
    ThrowBindingError(
        env,
        "BINDING_INVALID_ARGUMENT",
        operation + " expects an object argument for " + argument_name + ".",
        operation);
    return Napi::Object::New(env);
  }

  return info[index].As<Napi::Object>();
}

std::string ExtractHostTextArgument(const Napi::Value& value) {
  return value.As<Napi::String>().Utf8Value();
}

std::string ExtractHostPathArgument(const Napi::Value& value) {
#ifdef _WIN32
  return Utf16ToActiveCodePage(value.As<Napi::String>().Utf16Value());
#else
  return value.As<Napi::String>().Utf8Value();
#endif
}

std::string RequireStringField(
    Napi::Env env,
    const Napi::Object& object,
    const char* field_name,
    const std::string& operation,
    const bool as_path,
    const bool allow_empty = false) {
  const Napi::Value value = object.Get(field_name);
  if (!value.IsString()) {
    ThrowBindingError(
        env,
        "BINDING_INVALID_ARGUMENT",
        operation + " expects a string field named " + field_name + ".",
        operation);
    return {};
  }

  const std::string result =
      as_path ? ExtractHostPathArgument(value) : ExtractHostTextArgument(value);
  if (!allow_empty && result.empty()) {
    ThrowBindingError(
        env,
        "BINDING_INVALID_ARGUMENT",
        operation + " received an empty field named " + field_name + ".",
        operation);
    return {};
  }

  return result;
}

std::string ReadOptionalStringField(
    const Napi::Object& object,
    const char* field_name,
    const bool as_path) {
  const Napi::Value value = object.Get(field_name);
  if (!value.IsString()) {
    return {};
  }

  return as_path ? ExtractHostPathArgument(value) : ExtractHostTextArgument(value);
}

std::size_t RequireSizeField(
    Napi::Env env,
    const Napi::Object& object,
    const char* field_name,
    const std::string& operation) {
  const Napi::Value value = object.Get(field_name);
  if (!value.IsNumber()) {
    ThrowBindingError(
        env,
        "BINDING_INVALID_ARGUMENT",
        operation + " expects a numeric field named " + field_name + ".",
        operation);
    return 0;
  }

  const double numeric_value = value.As<Napi::Number>().DoubleValue();
  if (numeric_value < 0 || numeric_value != static_cast<std::size_t>(numeric_value)) {
    ThrowBindingError(
        env,
        "BINDING_INVALID_ARGUMENT",
        operation + " received a non-integer numeric field named " + field_name + ".",
        operation);
    return 0;
  }

  return static_cast<std::size_t>(numeric_value);
}

bool ReadOptionalBoolField(
    const Napi::Object& object,
    const char* field_name,
    const bool default_value = false) {
  const Napi::Value value = object.Get(field_name);
  return value.IsBoolean() ? value.As<Napi::Boolean>().Value() : default_value;
}

kernel_search_kind ParseSearchKind(
    Napi::Env env,
    const std::string& kind,
    const std::string& operation) {
  if (kind == "note") {
    return KERNEL_SEARCH_KIND_NOTE;
  }
  if (kind == "attachment") {
    return KERNEL_SEARCH_KIND_ATTACHMENT;
  }
  if (kind == "all") {
    return KERNEL_SEARCH_KIND_ALL;
  }

  ThrowBindingError(
      env,
      "BINDING_INVALID_ARGUMENT",
      operation + " received an unsupported search kind.",
      operation);
  return KERNEL_SEARCH_KIND_ALL;
}

kernel_search_sort_mode ParseSearchSortMode(
    Napi::Env env,
    const std::string& sort_mode,
    const std::string& operation) {
  if (sort_mode == "rel_path_asc") {
    return KERNEL_SEARCH_SORT_REL_PATH_ASC;
  }
  if (sort_mode == "rank_v1") {
    return KERNEL_SEARCH_SORT_RANK_V1;
  }

  ThrowBindingError(
      env,
      "BINDING_INVALID_ARGUMENT",
      operation + " received an unsupported search sortMode.",
      operation);
  return KERNEL_SEARCH_SORT_REL_PATH_ASC;
}

Napi::Value MakeKernelPathValue(Napi::Env env, const char* value) {
  if (value == nullptr) {
    return env.Null();
  }

  return Napi::String::New(env, ActiveCodePageToUtf8(value));
}

Napi::Value MakeKernelTextValue(Napi::Env env, const char* value) {
  if (value == nullptr) {
    return env.Null();
  }

  return Napi::String::New(env, value);
}

Napi::Object MakeStateSnapshotObject(
    Napi::Env env,
    const kernel_state_snapshot& snapshot) {
  Napi::Object result = Napi::Object::New(env);
  result.Set("sessionStateCode", Napi::Number::New(env, snapshot.session_state));
  result.Set("indexStateCode", Napi::Number::New(env, snapshot.index_state));
  result.Set("indexedNoteCount", Napi::Number::New(env, snapshot.indexed_note_count));
  result.Set("pendingRecoveryOps", Napi::Number::New(env, snapshot.pending_recovery_ops));
  return result;
}

Napi::Object MakeRebuildStatusObject(
    Napi::Env env,
    const kernel_rebuild_status_snapshot& snapshot) {
  Napi::Object result = Napi::Object::New(env);
  result.Set("inFlight", Napi::Boolean::New(env, snapshot.in_flight != 0));
  result.Set("hasLastResult", Napi::Boolean::New(env, snapshot.has_last_result != 0));
  result.Set("currentGeneration", Napi::Number::New(env, snapshot.current_generation));
  result.Set(
      "lastCompletedGeneration",
      Napi::Number::New(env, snapshot.last_completed_generation));
  result.Set(
      "currentStartedAtNs",
      Napi::Number::New(env, snapshot.current_started_at_ns));
  result.Set(
      "lastResultCode",
      Napi::String::New(env, KernelErrorCodeName(snapshot.last_result_code)));
  result.Set("lastResultCodeValue", Napi::Number::New(env, snapshot.last_result_code));
  result.Set("lastResultAtNs", Napi::Number::New(env, snapshot.last_result_at_ns));
  return result;
}

Napi::Object MakeSearchHitObject(
    Napi::Env env,
    const kernel_search_page_hit& hit) {
  Napi::Object result = Napi::Object::New(env);
  result.Set("rel_path", MakeKernelPathValue(env, hit.rel_path));
  result.Set("title", MakeKernelTextValue(env, hit.title));
  result.Set("snippet", MakeKernelTextValue(env, hit.snippet));
  result.Set("match_flags", Napi::Number::New(env, hit.match_flags));
  result.Set("snippet_status", Napi::Number::New(env, hit.snippet_status));
  result.Set("result_kind", Napi::Number::New(env, hit.result_kind));
  result.Set("result_flags", Napi::Number::New(env, hit.result_flags));
  result.Set("score", Napi::Number::New(env, hit.score));
  return result;
}

Napi::Object MakeSearchPageObject(
    Napi::Env env,
    const kernel_search_page& page) {
  Napi::Array hits = Napi::Array::New(env, page.count);
  for (std::size_t index = 0; index < page.count; ++index) {
    hits.Set(static_cast<uint32_t>(index), MakeSearchHitObject(env, page.hits[index]));
  }

  Napi::Object result = Napi::Object::New(env);
  result.Set("hits", hits);
  result.Set("count", Napi::Number::New(env, page.count));
  result.Set("total_hits", Napi::Number::New(env, page.total_hits));
  result.Set("has_more", Napi::Boolean::New(env, page.has_more != 0));
  return result;
}

Napi::Object MakeAttachmentRecordObject(
    Napi::Env env,
    const kernel_attachment_record& attachment) {
  Napi::Object result = Napi::Object::New(env);
  result.Set("rel_path", MakeKernelPathValue(env, attachment.rel_path));
  result.Set("basename", MakeKernelPathValue(env, attachment.basename));
  result.Set("extension", MakeKernelTextValue(env, attachment.extension));
  result.Set("file_size", Napi::Number::New(env, attachment.file_size));
  result.Set("mtime_ns", Napi::Number::New(env, attachment.mtime_ns));
  result.Set("ref_count", Napi::Number::New(env, attachment.ref_count));
  result.Set("kind", Napi::Number::New(env, attachment.kind));
  result.Set("flags", Napi::Number::New(env, attachment.flags));
  result.Set("presence", Napi::Number::New(env, attachment.presence));
  return result;
}

Napi::Object MakeAttachmentListObject(
    Napi::Env env,
    const kernel_attachment_list& attachments) {
  Napi::Array items = Napi::Array::New(env, attachments.count);
  for (std::size_t index = 0; index < attachments.count; ++index) {
    items.Set(
        static_cast<uint32_t>(index),
        MakeAttachmentRecordObject(env, attachments.attachments[index]));
  }

  Napi::Object result = Napi::Object::New(env);
  result.Set("attachments", items);
  result.Set("count", Napi::Number::New(env, attachments.count));
  return result;
}

Napi::Object MakeAttachmentReferrersObject(
    Napi::Env env,
    const kernel_attachment_referrers& referrers) {
  Napi::Array items = Napi::Array::New(env, referrers.count);
  for (std::size_t index = 0; index < referrers.count; ++index) {
    Napi::Object referrer = Napi::Object::New(env);
    referrer.Set(
        "note_rel_path",
        MakeKernelPathValue(env, referrers.referrers[index].note_rel_path));
    referrer.Set(
        "note_title",
        MakeKernelTextValue(env, referrers.referrers[index].note_title));
    items.Set(static_cast<uint32_t>(index), referrer);
  }

  Napi::Object result = Napi::Object::New(env);
  result.Set("referrers", items);
  result.Set("count", Napi::Number::New(env, referrers.count));
  return result;
}

Napi::Object MakePdfMetadataObject(
    Napi::Env env,
    const kernel_pdf_metadata_record& metadata) {
  Napi::Object result = Napi::Object::New(env);
  result.Set("rel_path", MakeKernelPathValue(env, metadata.rel_path));
  result.Set("doc_title", MakeKernelTextValue(env, metadata.doc_title));
  result.Set(
      "pdf_metadata_revision",
      MakeKernelTextValue(env, metadata.pdf_metadata_revision));
  result.Set("page_count", Napi::Number::New(env, metadata.page_count));
  result.Set("has_outline", Napi::Boolean::New(env, metadata.has_outline != 0));
  result.Set("presence", Napi::Number::New(env, metadata.presence));
  result.Set("metadata_state", Napi::Number::New(env, metadata.metadata_state));
  result.Set("doc_title_state", Napi::Number::New(env, metadata.doc_title_state));
  result.Set("text_layer_state", Napi::Number::New(env, metadata.text_layer_state));
  return result;
}

Napi::Object MakePdfSourceRefObject(
    Napi::Env env,
    const kernel_pdf_source_ref& ref) {
  Napi::Object result = Napi::Object::New(env);
  result.Set("pdf_rel_path", MakeKernelPathValue(env, ref.pdf_rel_path));
  result.Set("anchor_serialized", MakeKernelTextValue(env, ref.anchor_serialized));
  result.Set("excerpt_text", MakeKernelTextValue(env, ref.excerpt_text));
  result.Set("page", Napi::Number::New(env, ref.page));
  result.Set("state", Napi::Number::New(env, ref.state));
  return result;
}

Napi::Object MakePdfSourceRefsObject(
    Napi::Env env,
    const kernel_pdf_source_refs& refs) {
  Napi::Array items = Napi::Array::New(env, refs.count);
  for (std::size_t index = 0; index < refs.count; ++index) {
    items.Set(static_cast<uint32_t>(index), MakePdfSourceRefObject(env, refs.refs[index]));
  }

  Napi::Object result = Napi::Object::New(env);
  result.Set("refs", items);
  result.Set("count", Napi::Number::New(env, refs.count));
  return result;
}

Napi::Object MakePdfReferrersObject(
    Napi::Env env,
    const kernel_pdf_referrers& referrers) {
  Napi::Array items = Napi::Array::New(env, referrers.count);
  for (std::size_t index = 0; index < referrers.count; ++index) {
    Napi::Object referrer = Napi::Object::New(env);
    referrer.Set(
        "note_rel_path",
        MakeKernelPathValue(env, referrers.referrers[index].note_rel_path));
    referrer.Set(
        "note_title",
        MakeKernelTextValue(env, referrers.referrers[index].note_title));
    referrer.Set(
        "anchor_serialized",
        MakeKernelTextValue(env, referrers.referrers[index].anchor_serialized));
    referrer.Set(
        "excerpt_text",
        MakeKernelTextValue(env, referrers.referrers[index].excerpt_text));
    referrer.Set("page", Napi::Number::New(env, referrers.referrers[index].page));
    referrer.Set("state", Napi::Number::New(env, referrers.referrers[index].state));
    items.Set(static_cast<uint32_t>(index), referrer);
  }

  Napi::Object result = Napi::Object::New(env);
  result.Set("referrers", items);
  result.Set("count", Napi::Number::New(env, referrers.count));
  return result;
}

Napi::Object MakeDomainMetadataEntryObject(
    Napi::Env env,
    const kernel_domain_metadata_entry& entry) {
  Napi::Object result = Napi::Object::New(env);
  result.Set("carrier_kind", Napi::Number::New(env, entry.carrier_kind));
  result.Set("carrier_key", MakeKernelPathValue(env, entry.carrier_key));
  result.Set("namespace_name", MakeKernelTextValue(env, entry.namespace_name));
  result.Set(
      "public_schema_revision",
      Napi::Number::New(env, entry.public_schema_revision));
  result.Set("key_name", MakeKernelTextValue(env, entry.key_name));
  result.Set("value_kind", Napi::Number::New(env, entry.value_kind));
  result.Set("bool_value", Napi::Boolean::New(env, entry.bool_value != 0));
  result.Set("uint64_value", Napi::Number::New(env, entry.uint64_value));
  result.Set("string_value", MakeKernelTextValue(env, entry.string_value));
  result.Set("flags", Napi::Number::New(env, entry.flags));
  return result;
}

Napi::Object MakeDomainMetadataListObject(
    Napi::Env env,
    const kernel_domain_metadata_list& entries) {
  Napi::Array items = Napi::Array::New(env, entries.count);
  for (std::size_t index = 0; index < entries.count; ++index) {
    items.Set(
        static_cast<uint32_t>(index),
        MakeDomainMetadataEntryObject(env, entries.entries[index]));
  }

  Napi::Object result = Napi::Object::New(env);
  result.Set("entries", items);
  result.Set("count", Napi::Number::New(env, entries.count));
  return result;
}

Napi::Object MakeChemSpectrumRecordObject(
    Napi::Env env,
    const kernel_chem_spectrum_record& spectrum) {
  Napi::Object result = Napi::Object::New(env);
  result.Set(
      "attachment_rel_path",
      MakeKernelPathValue(env, spectrum.attachment_rel_path));
  result.Set(
      "domain_object_key",
      MakeKernelTextValue(env, spectrum.domain_object_key));
  result.Set("subtype_revision", Napi::Number::New(env, spectrum.subtype_revision));
  result.Set("source_format", Napi::Number::New(env, spectrum.source_format));
  result.Set("coarse_kind", Napi::Number::New(env, spectrum.coarse_kind));
  result.Set("presence", Napi::Number::New(env, spectrum.presence));
  result.Set("state", Napi::Number::New(env, spectrum.state));
  result.Set("flags", Napi::Number::New(env, spectrum.flags));
  return result;
}

Napi::Object MakeChemSpectrumListObject(
    Napi::Env env,
    const kernel_chem_spectrum_list& spectra) {
  Napi::Array items = Napi::Array::New(env, spectra.count);
  for (std::size_t index = 0; index < spectra.count; ++index) {
    items.Set(
        static_cast<uint32_t>(index),
        MakeChemSpectrumRecordObject(env, spectra.spectra[index]));
  }

  Napi::Object result = Napi::Object::New(env);
  result.Set("spectra", items);
  result.Set("count", Napi::Number::New(env, spectra.count));
  return result;
}

Napi::Object MakeChemSpectrumSourceRefObject(
    Napi::Env env,
    const kernel_chem_spectrum_source_ref& ref) {
  Napi::Object result = Napi::Object::New(env);
  result.Set(
      "attachment_rel_path",
      MakeKernelPathValue(env, ref.attachment_rel_path));
  result.Set(
      "domain_object_key",
      MakeKernelTextValue(env, ref.domain_object_key));
  result.Set("selector_kind", Napi::Number::New(env, ref.selector_kind));
  result.Set(
      "selector_serialized",
      MakeKernelTextValue(env, ref.selector_serialized));
  result.Set("preview_text", MakeKernelTextValue(env, ref.preview_text));
  result.Set(
      "target_basis_revision",
      MakeKernelTextValue(env, ref.target_basis_revision));
  result.Set("state", Napi::Number::New(env, ref.state));
  result.Set("flags", Napi::Number::New(env, ref.flags));
  return result;
}

Napi::Object MakeChemSpectrumSourceRefsObject(
    Napi::Env env,
    const kernel_chem_spectrum_source_refs& refs) {
  Napi::Array items = Napi::Array::New(env, refs.count);
  for (std::size_t index = 0; index < refs.count; ++index) {
    items.Set(
        static_cast<uint32_t>(index),
        MakeChemSpectrumSourceRefObject(env, refs.refs[index]));
  }

  Napi::Object result = Napi::Object::New(env);
  result.Set("refs", items);
  result.Set("count", Napi::Number::New(env, refs.count));
  return result;
}

Napi::Object MakeChemSpectrumReferrersObject(
    Napi::Env env,
    const kernel_chem_spectrum_referrers& referrers) {
  Napi::Array items = Napi::Array::New(env, referrers.count);
  for (std::size_t index = 0; index < referrers.count; ++index) {
    Napi::Object referrer = Napi::Object::New(env);
    referrer.Set(
        "note_rel_path",
        MakeKernelPathValue(env, referrers.referrers[index].note_rel_path));
    referrer.Set(
        "note_title",
        MakeKernelTextValue(env, referrers.referrers[index].note_title));
    referrer.Set(
        "attachment_rel_path",
        MakeKernelPathValue(env, referrers.referrers[index].attachment_rel_path));
    referrer.Set(
        "domain_object_key",
        MakeKernelTextValue(env, referrers.referrers[index].domain_object_key));
    referrer.Set(
        "selector_kind",
        Napi::Number::New(env, referrers.referrers[index].selector_kind));
    referrer.Set(
        "selector_serialized",
        MakeKernelTextValue(env, referrers.referrers[index].selector_serialized));
    referrer.Set(
        "preview_text",
        MakeKernelTextValue(env, referrers.referrers[index].preview_text));
    referrer.Set(
        "target_basis_revision",
        MakeKernelTextValue(env, referrers.referrers[index].target_basis_revision));
    referrer.Set("state", Napi::Number::New(env, referrers.referrers[index].state));
    referrer.Set("flags", Napi::Number::New(env, referrers.referrers[index].flags));
    items.Set(static_cast<uint32_t>(index), referrer);
  }

  Napi::Object result = Napi::Object::New(env);
  result.Set("referrers", items);
  result.Set("count", Napi::Number::New(env, referrers.count));
  return result;
}

Napi::Object GetBindingInfo(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  Napi::Object result = Napi::Object::New(env);
  result.Set("bindingMechanism", "node_api_addon");
  result.Set("bindingName", "kernel_host_binding");
  result.Set("abiSurface", "sealed_kernel_c_abi");
  result.Set("napiVersion", Napi::Number::New(env, NAPI_VERSION));
  result.Set("kernelRevisionMax", Napi::Number::New(env, KERNEL_REVISION_MAX));
  return result;
}

Napi::Value OpenVault(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  if (info.Length() < 1 || !info[0].IsString()) {
    ThrowBindingError(
        env,
        "BINDING_INVALID_ARGUMENT",
        "openVault expects a vault path string.",
        "kernel_open_vault");
    return env.Null();
  }

  const std::string vault_path = ExtractHostPathArgument(info[0]);
  if (vault_path.empty()) {
    ThrowBindingError(
        env,
        "BINDING_INVALID_ARGUMENT",
        "openVault received an empty or unencodable vault path.",
        "kernel_open_vault");
    return env.Null();
  }

  kernel_handle* handle = nullptr;
  const kernel_status status = kernel_open_vault(vault_path.c_str(), &handle);
  if (status.code != KERNEL_OK) {
    if (handle != nullptr) {
      kernel_close(handle);
    }
    ThrowKernelStatusError(
        env,
        status,
        "kernel_open_vault",
        "kernel_open_vault failed");
    return env.Null();
  }

  auto* session = new (std::nothrow) KernelSessionBox();
  if (session == nullptr) {
    kernel_close(handle);
    ThrowBindingError(
        env,
        "BINDING_ALLOCATION_FAILED",
        "Failed to allocate a kernel session wrapper.",
        "kernel_open_vault");
    return env.Null();
  }

  session->handle = handle;
  return Napi::External<KernelSessionBox>::New(env, session, FinalizeKernelSession);
}

Napi::Value CloseVault(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  KernelSessionBox* session = RequireSessionBox(info, 0, "kernel_close");
  if (session == nullptr) {
    return env.Undefined();
  }

  kernel_handle* handle = session->handle;
  session->handle = nullptr;
  const kernel_status status = kernel_close(handle);
  if (status.code != KERNEL_OK) {
    ThrowKernelStatusError(env, status, "kernel_close", "kernel_close failed");
    return env.Undefined();
  }

  return env.Undefined();
}

Napi::Value GetState(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  KernelSessionBox* session = RequireSessionBox(info, 0, "kernel_get_state");
  if (session == nullptr) {
    return env.Null();
  }

  kernel_state_snapshot snapshot{};
  const kernel_status status = kernel_get_state(session->handle, &snapshot);
  if (status.code != KERNEL_OK) {
    ThrowKernelStatusError(
        env,
        status,
        "kernel_get_state",
        "kernel_get_state failed");
    return env.Null();
  }

  return MakeStateSnapshotObject(env, snapshot);
}

Napi::Value GetRebuildStatus(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  KernelSessionBox* session = RequireSessionBox(info, 0, "kernel_get_rebuild_status");
  if (session == nullptr) {
    return env.Null();
  }

  kernel_rebuild_status_snapshot snapshot{};
  const kernel_status status = kernel_get_rebuild_status(session->handle, &snapshot);
  if (status.code != KERNEL_OK) {
    ThrowKernelStatusError(
        env,
        status,
        "kernel_get_rebuild_status",
        "kernel_get_rebuild_status failed");
    return env.Null();
  }

  return MakeRebuildStatusObject(env, snapshot);
}

Napi::Value QuerySearch(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  KernelSessionBox* session = RequireSessionBox(info, 0, "kernel_query_search");
  if (session == nullptr) {
    return env.Null();
  }

  const Napi::Object request_object =
      RequireObjectArgument(info, 1, "kernel_query_search", "request");

  const std::string query =
      RequireStringField(env, request_object, "query", "kernel_query_search", false);
  const std::size_t limit =
      RequireSizeField(env, request_object, "limit", "kernel_query_search");
  const std::size_t offset =
      RequireSizeField(env, request_object, "offset", "kernel_query_search");
  const std::string kind_name =
      ReadOptionalStringField(request_object, "kind", false).empty()
          ? "all"
          : ReadOptionalStringField(request_object, "kind", false);
  const std::string sort_mode_name =
      ReadOptionalStringField(request_object, "sortMode", false).empty()
          ? "rel_path_asc"
          : ReadOptionalStringField(request_object, "sortMode", false);
  const std::string tag_filter =
      ReadOptionalStringField(request_object, "tagFilter", false);
  const std::string path_prefix =
      ReadOptionalStringField(request_object, "pathPrefix", true);

  kernel_search_query request{};
  request.query = query.c_str();
  request.limit = limit;
  request.offset = offset;
  request.kind = ParseSearchKind(env, kind_name, "kernel_query_search");
  request.tag_filter = tag_filter.empty() ? nullptr : tag_filter.c_str();
  request.path_prefix = path_prefix.empty() ? nullptr : path_prefix.c_str();
  request.include_deleted = ReadOptionalBoolField(request_object, "includeDeleted") ? 1 : 0;
  request.sort_mode =
      ParseSearchSortMode(env, sort_mode_name, "kernel_query_search");

  OwnedKernelResult<kernel_search_page, kernel_free_search_page> page;
  const kernel_status status = kernel_query_search(session->handle, &request, page.out());
  if (status.code != KERNEL_OK) {
    ThrowKernelStatusError(
        env,
        status,
        "kernel_query_search",
        "kernel_query_search failed");
    return env.Null();
  }

  return MakeSearchPageObject(env, page.value);
}

Napi::Value QueryAttachments(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  KernelSessionBox* session = RequireSessionBox(info, 0, "kernel_query_attachments");
  if (session == nullptr) {
    return env.Null();
  }

  if (info.Length() < 2 || !info[1].IsNumber()) {
    ThrowBindingError(
        env,
        "BINDING_INVALID_ARGUMENT",
        "kernel_query_attachments expects a numeric limit.",
        "kernel_query_attachments");
    return env.Null();
  }

  const double limit_value = info[1].As<Napi::Number>().DoubleValue();
  const std::size_t limit = limit_value == -1
      ? static_cast<std::size_t>(-1)
      : static_cast<std::size_t>(limit_value);

  OwnedKernelResult<kernel_attachment_list, kernel_free_attachment_list> attachments;
  const kernel_status status =
      kernel_query_attachments(session->handle, limit, attachments.out());
  if (status.code != KERNEL_OK) {
    ThrowKernelStatusError(
        env,
        status,
        "kernel_query_attachments",
        "kernel_query_attachments failed");
    return env.Null();
  }

  return MakeAttachmentListObject(env, attachments.value);
}

Napi::Value GetAttachment(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  KernelSessionBox* session = RequireSessionBox(info, 0, "kernel_get_attachment");
  if (session == nullptr) {
    return env.Null();
  }

  if (info.Length() < 2 || !info[1].IsString()) {
    ThrowBindingError(
        env,
        "BINDING_INVALID_ARGUMENT",
        "kernel_get_attachment expects an attachment rel_path string.",
        "kernel_get_attachment");
    return env.Null();
  }

  const std::string attachment_rel_path = ExtractHostPathArgument(info[1]);
  OwnedKernelResult<kernel_attachment_record, kernel_free_attachment_record> attachment;
  const kernel_status status =
      kernel_get_attachment(
          session->handle,
          attachment_rel_path.c_str(),
          attachment.out());
  if (status.code != KERNEL_OK) {
    ThrowKernelStatusError(
        env,
        status,
        "kernel_get_attachment",
        "kernel_get_attachment failed");
    return env.Null();
  }

  return MakeAttachmentRecordObject(env, attachment.value);
}

Napi::Value QueryNoteAttachmentRefs(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  KernelSessionBox* session =
      RequireSessionBox(info, 0, "kernel_query_note_attachment_refs");
  if (session == nullptr) {
    return env.Null();
  }

  if (info.Length() < 3 || !info[1].IsString() || !info[2].IsNumber()) {
    ThrowBindingError(
        env,
        "BINDING_INVALID_ARGUMENT",
        "kernel_query_note_attachment_refs expects note_rel_path and limit.",
        "kernel_query_note_attachment_refs");
    return env.Null();
  }

  const std::string note_rel_path = ExtractHostPathArgument(info[1]);
  const double limit_value = info[2].As<Napi::Number>().DoubleValue();
  const std::size_t limit = limit_value == -1
      ? static_cast<std::size_t>(-1)
      : static_cast<std::size_t>(limit_value);

  OwnedKernelResult<kernel_attachment_list, kernel_free_attachment_list> attachments;
  const kernel_status status =
      kernel_query_note_attachment_refs(
          session->handle,
          note_rel_path.c_str(),
          limit,
          attachments.out());
  if (status.code != KERNEL_OK) {
    ThrowKernelStatusError(
        env,
        status,
        "kernel_query_note_attachment_refs",
        "kernel_query_note_attachment_refs failed");
    return env.Null();
  }

  return MakeAttachmentListObject(env, attachments.value);
}

Napi::Value QueryAttachmentReferrers(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  KernelSessionBox* session =
      RequireSessionBox(info, 0, "kernel_query_attachment_referrers");
  if (session == nullptr) {
    return env.Null();
  }

  if (info.Length() < 3 || !info[1].IsString() || !info[2].IsNumber()) {
    ThrowBindingError(
        env,
        "BINDING_INVALID_ARGUMENT",
        "kernel_query_attachment_referrers expects attachment_rel_path and limit.",
        "kernel_query_attachment_referrers");
    return env.Null();
  }

  const std::string attachment_rel_path = ExtractHostPathArgument(info[1]);
  const double limit_value = info[2].As<Napi::Number>().DoubleValue();
  const std::size_t limit = limit_value == -1
      ? static_cast<std::size_t>(-1)
      : static_cast<std::size_t>(limit_value);

  OwnedKernelResult<kernel_attachment_referrers, kernel_free_attachment_referrers> referrers;
  const kernel_status status =
      kernel_query_attachment_referrers(
          session->handle,
          attachment_rel_path.c_str(),
          limit,
          referrers.out());
  if (status.code != KERNEL_OK) {
    ThrowKernelStatusError(
        env,
        status,
        "kernel_query_attachment_referrers",
        "kernel_query_attachment_referrers failed");
    return env.Null();
  }

  return MakeAttachmentReferrersObject(env, referrers.value);
}

Napi::Value GetPdfMetadata(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  KernelSessionBox* session = RequireSessionBox(info, 0, "kernel_get_pdf_metadata");
  if (session == nullptr) {
    return env.Null();
  }

  if (info.Length() < 2 || !info[1].IsString()) {
    ThrowBindingError(
        env,
        "BINDING_INVALID_ARGUMENT",
        "kernel_get_pdf_metadata expects an attachment rel_path string.",
        "kernel_get_pdf_metadata");
    return env.Null();
  }

  const std::string attachment_rel_path = ExtractHostPathArgument(info[1]);
  OwnedKernelResult<kernel_pdf_metadata_record, kernel_free_pdf_metadata_record> metadata;
  const kernel_status status =
      kernel_get_pdf_metadata(
          session->handle,
          attachment_rel_path.c_str(),
          metadata.out());
  if (status.code != KERNEL_OK) {
    ThrowKernelStatusError(
        env,
        status,
        "kernel_get_pdf_metadata",
        "kernel_get_pdf_metadata failed");
    return env.Null();
  }

  return MakePdfMetadataObject(env, metadata.value);
}

Napi::Value QueryNotePdfSourceRefs(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  KernelSessionBox* session =
      RequireSessionBox(info, 0, "kernel_query_note_pdf_source_refs");
  if (session == nullptr) {
    return env.Null();
  }

  if (info.Length() < 3 || !info[1].IsString() || !info[2].IsNumber()) {
    ThrowBindingError(
        env,
        "BINDING_INVALID_ARGUMENT",
        "kernel_query_note_pdf_source_refs expects note_rel_path and limit.",
        "kernel_query_note_pdf_source_refs");
    return env.Null();
  }

  const std::string note_rel_path = ExtractHostPathArgument(info[1]);
  const double limit_value = info[2].As<Napi::Number>().DoubleValue();
  const std::size_t limit = limit_value == -1
      ? static_cast<std::size_t>(-1)
      : static_cast<std::size_t>(limit_value);

  OwnedKernelResult<kernel_pdf_source_refs, kernel_free_pdf_source_refs> refs;
  const kernel_status status =
      kernel_query_note_pdf_source_refs(
          session->handle,
          note_rel_path.c_str(),
          limit,
          refs.out());
  if (status.code != KERNEL_OK) {
    ThrowKernelStatusError(
        env,
        status,
        "kernel_query_note_pdf_source_refs",
        "kernel_query_note_pdf_source_refs failed");
    return env.Null();
  }

  return MakePdfSourceRefsObject(env, refs.value);
}

Napi::Value QueryPdfReferrers(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  KernelSessionBox* session = RequireSessionBox(info, 0, "kernel_query_pdf_referrers");
  if (session == nullptr) {
    return env.Null();
  }

  if (info.Length() < 3 || !info[1].IsString() || !info[2].IsNumber()) {
    ThrowBindingError(
        env,
        "BINDING_INVALID_ARGUMENT",
        "kernel_query_pdf_referrers expects attachment_rel_path and limit.",
        "kernel_query_pdf_referrers");
    return env.Null();
  }

  const std::string attachment_rel_path = ExtractHostPathArgument(info[1]);
  const double limit_value = info[2].As<Napi::Number>().DoubleValue();
  const std::size_t limit = limit_value == -1
      ? static_cast<std::size_t>(-1)
      : static_cast<std::size_t>(limit_value);

  OwnedKernelResult<kernel_pdf_referrers, kernel_free_pdf_referrers> referrers;
  const kernel_status status =
      kernel_query_pdf_referrers(
          session->handle,
          attachment_rel_path.c_str(),
          limit,
          referrers.out());
  if (status.code != KERNEL_OK) {
    ThrowKernelStatusError(
        env,
        status,
        "kernel_query_pdf_referrers",
        "kernel_query_pdf_referrers failed");
    return env.Null();
  }

  return MakePdfReferrersObject(env, referrers.value);
}

Napi::Value QueryChemSpectrumMetadata(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  KernelSessionBox* session =
      RequireSessionBox(info, 0, "kernel_query_chem_spectrum_metadata");
  if (session == nullptr) {
    return env.Null();
  }

  if (info.Length() < 3 || !info[1].IsString() || !info[2].IsNumber()) {
    ThrowBindingError(
        env,
        "BINDING_INVALID_ARGUMENT",
        "kernel_query_chem_spectrum_metadata expects attachment_rel_path and limit.",
        "kernel_query_chem_spectrum_metadata");
    return env.Null();
  }

  const std::string attachment_rel_path = ExtractHostPathArgument(info[1]);
  const double limit_value = info[2].As<Napi::Number>().DoubleValue();
  const std::size_t limit = limit_value == -1
      ? static_cast<std::size_t>(-1)
      : static_cast<std::size_t>(limit_value);

  OwnedKernelResult<kernel_domain_metadata_list, kernel_free_domain_metadata_list> entries;
  const kernel_status status =
      kernel_query_chem_spectrum_metadata(
          session->handle,
          attachment_rel_path.c_str(),
          limit,
          entries.out());
  if (status.code != KERNEL_OK) {
    ThrowKernelStatusError(
        env,
        status,
        "kernel_query_chem_spectrum_metadata",
        "kernel_query_chem_spectrum_metadata failed");
    return env.Null();
  }

  return MakeDomainMetadataListObject(env, entries.value);
}

Napi::Value QueryChemSpectra(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  KernelSessionBox* session = RequireSessionBox(info, 0, "kernel_query_chem_spectra");
  if (session == nullptr) {
    return env.Null();
  }

  if (info.Length() < 2 || !info[1].IsNumber()) {
    ThrowBindingError(
        env,
        "BINDING_INVALID_ARGUMENT",
        "kernel_query_chem_spectra expects a numeric limit.",
        "kernel_query_chem_spectra");
    return env.Null();
  }

  const double limit_value = info[1].As<Napi::Number>().DoubleValue();
  const std::size_t limit = limit_value == -1
      ? static_cast<std::size_t>(-1)
      : static_cast<std::size_t>(limit_value);

  OwnedKernelResult<kernel_chem_spectrum_list, kernel_free_chem_spectrum_list> spectra;
  const kernel_status status =
      kernel_query_chem_spectra(session->handle, limit, spectra.out());
  if (status.code != KERNEL_OK) {
    ThrowKernelStatusError(
        env,
        status,
        "kernel_query_chem_spectra",
        "kernel_query_chem_spectra failed");
    return env.Null();
  }

  return MakeChemSpectrumListObject(env, spectra.value);
}

Napi::Value GetChemSpectrum(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  KernelSessionBox* session = RequireSessionBox(info, 0, "kernel_get_chem_spectrum");
  if (session == nullptr) {
    return env.Null();
  }

  if (info.Length() < 2 || !info[1].IsString()) {
    ThrowBindingError(
        env,
        "BINDING_INVALID_ARGUMENT",
        "kernel_get_chem_spectrum expects an attachment rel_path string.",
        "kernel_get_chem_spectrum");
    return env.Null();
  }

  const std::string attachment_rel_path = ExtractHostPathArgument(info[1]);
  OwnedKernelResult<kernel_chem_spectrum_record, kernel_free_chem_spectrum_record> spectrum;
  const kernel_status status =
      kernel_get_chem_spectrum(
          session->handle,
          attachment_rel_path.c_str(),
          spectrum.out());
  if (status.code != KERNEL_OK) {
    ThrowKernelStatusError(
        env,
        status,
        "kernel_get_chem_spectrum",
        "kernel_get_chem_spectrum failed");
    return env.Null();
  }

  return MakeChemSpectrumRecordObject(env, spectrum.value);
}

Napi::Value QueryNoteChemSpectrumRefs(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  KernelSessionBox* session =
      RequireSessionBox(info, 0, "kernel_query_note_chem_spectrum_refs");
  if (session == nullptr) {
    return env.Null();
  }

  if (info.Length() < 3 || !info[1].IsString() || !info[2].IsNumber()) {
    ThrowBindingError(
        env,
        "BINDING_INVALID_ARGUMENT",
        "kernel_query_note_chem_spectrum_refs expects note_rel_path and limit.",
        "kernel_query_note_chem_spectrum_refs");
    return env.Null();
  }

  const std::string note_rel_path = ExtractHostPathArgument(info[1]);
  const double limit_value = info[2].As<Napi::Number>().DoubleValue();
  const std::size_t limit = limit_value == -1
      ? static_cast<std::size_t>(-1)
      : static_cast<std::size_t>(limit_value);

  OwnedKernelResult<kernel_chem_spectrum_source_refs, kernel_free_chem_spectrum_source_refs>
      refs;
  const kernel_status status =
      kernel_query_note_chem_spectrum_refs(
          session->handle,
          note_rel_path.c_str(),
          limit,
          refs.out());
  if (status.code != KERNEL_OK) {
    ThrowKernelStatusError(
        env,
        status,
        "kernel_query_note_chem_spectrum_refs",
        "kernel_query_note_chem_spectrum_refs failed");
    return env.Null();
  }

  return MakeChemSpectrumSourceRefsObject(env, refs.value);
}

Napi::Value QueryChemSpectrumReferrers(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  KernelSessionBox* session =
      RequireSessionBox(info, 0, "kernel_query_chem_spectrum_referrers");
  if (session == nullptr) {
    return env.Null();
  }

  if (info.Length() < 3 || !info[1].IsString() || !info[2].IsNumber()) {
    ThrowBindingError(
        env,
        "BINDING_INVALID_ARGUMENT",
        "kernel_query_chem_spectrum_referrers expects attachment_rel_path and limit.",
        "kernel_query_chem_spectrum_referrers");
    return env.Null();
  }

  const std::string attachment_rel_path = ExtractHostPathArgument(info[1]);
  const double limit_value = info[2].As<Napi::Number>().DoubleValue();
  const std::size_t limit = limit_value == -1
      ? static_cast<std::size_t>(-1)
      : static_cast<std::size_t>(limit_value);

  OwnedKernelResult<kernel_chem_spectrum_referrers, kernel_free_chem_spectrum_referrers>
      referrers;
  const kernel_status status =
      kernel_query_chem_spectrum_referrers(
          session->handle,
          attachment_rel_path.c_str(),
          limit,
          referrers.out());
  if (status.code != KERNEL_OK) {
    ThrowKernelStatusError(
        env,
        status,
        "kernel_query_chem_spectrum_referrers",
        "kernel_query_chem_spectrum_referrers failed");
    return env.Null();
  }

  return MakeChemSpectrumReferrersObject(env, referrers.value);
}

Napi::Value ExportDiagnostics(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  KernelSessionBox* session = RequireSessionBox(info, 0, "kernel_export_diagnostics");
  if (session == nullptr) {
    return env.Null();
  }

  if (info.Length() < 2 || !info[1].IsString()) {
    ThrowBindingError(
        env,
        "BINDING_INVALID_ARGUMENT",
        "kernel_export_diagnostics expects an output path string.",
        "kernel_export_diagnostics");
    return env.Null();
  }

  const std::string output_path = ExtractHostPathArgument(info[1]);
  const kernel_status status =
      kernel_export_diagnostics(session->handle, output_path.c_str());
  if (status.code != KERNEL_OK) {
    ThrowKernelStatusError(
        env,
        status,
        "kernel_export_diagnostics",
        "kernel_export_diagnostics failed");
    return env.Null();
  }

  Napi::Object result = Napi::Object::New(env);
  result.Set("result", "exported");
  return result;
}

Napi::Value StartRebuild(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  KernelSessionBox* session =
      RequireSessionBox(info, 0, "kernel_start_rebuild_index");
  if (session == nullptr) {
    return env.Null();
  }

  const kernel_status status = kernel_start_rebuild_index(session->handle);
  if (status.code != KERNEL_OK) {
    ThrowKernelStatusError(
        env,
        status,
        "kernel_start_rebuild_index",
        "kernel_start_rebuild_index failed");
    return env.Null();
  }

  Napi::Object result = Napi::Object::New(env);
  result.Set("result", "started");
  return result;
}

Napi::Value WaitForRebuild(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  KernelSessionBox* session =
      RequireSessionBox(info, 0, "kernel_wait_for_rebuild");
  if (session == nullptr) {
    return env.Null();
  }

  if (info.Length() < 2 || !info[1].IsNumber()) {
    ThrowBindingError(
        env,
        "BINDING_INVALID_ARGUMENT",
        "kernel_wait_for_rebuild expects a timeoutMs number.",
        "kernel_wait_for_rebuild");
    return env.Null();
  }

  const double timeout_value = info[1].As<Napi::Number>().DoubleValue();
  if (timeout_value < 0 || timeout_value != static_cast<uint32_t>(timeout_value)) {
    ThrowBindingError(
        env,
        "BINDING_INVALID_ARGUMENT",
        "kernel_wait_for_rebuild expects a non-negative integer timeoutMs.",
        "kernel_wait_for_rebuild");
    return env.Null();
  }

  const kernel_status status =
      kernel_wait_for_rebuild(session->handle, static_cast<uint32_t>(timeout_value));
  if (status.code != KERNEL_OK) {
    ThrowKernelStatusError(
        env,
        status,
        "kernel_wait_for_rebuild",
        "kernel_wait_for_rebuild failed");
    return env.Null();
  }

  Napi::Object result = Napi::Object::New(env);
  result.Set("result", "completed");
  return result;
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
  exports.Set("getBindingInfo", Napi::Function::New(env, GetBindingInfo));
  exports.Set("openVault", Napi::Function::New(env, OpenVault));
  exports.Set("closeVault", Napi::Function::New(env, CloseVault));
  exports.Set("getState", Napi::Function::New(env, GetState));
  exports.Set("getRebuildStatus", Napi::Function::New(env, GetRebuildStatus));
  exports.Set("querySearch", Napi::Function::New(env, QuerySearch));
  exports.Set("queryAttachments", Napi::Function::New(env, QueryAttachments));
  exports.Set("getAttachment", Napi::Function::New(env, GetAttachment));
  exports.Set(
      "queryNoteAttachmentRefs",
      Napi::Function::New(env, QueryNoteAttachmentRefs));
  exports.Set(
      "queryAttachmentReferrers",
      Napi::Function::New(env, QueryAttachmentReferrers));
  exports.Set("getPdfMetadata", Napi::Function::New(env, GetPdfMetadata));
  exports.Set(
      "queryNotePdfSourceRefs",
      Napi::Function::New(env, QueryNotePdfSourceRefs));
  exports.Set("queryPdfReferrers", Napi::Function::New(env, QueryPdfReferrers));
  exports.Set(
      "queryChemSpectrumMetadata",
      Napi::Function::New(env, QueryChemSpectrumMetadata));
  exports.Set("queryChemSpectra", Napi::Function::New(env, QueryChemSpectra));
  exports.Set("getChemSpectrum", Napi::Function::New(env, GetChemSpectrum));
  exports.Set(
      "queryNoteChemSpectrumRefs",
      Napi::Function::New(env, QueryNoteChemSpectrumRefs));
  exports.Set(
      "queryChemSpectrumReferrers",
      Napi::Function::New(env, QueryChemSpectrumReferrers));
  exports.Set(
      "exportDiagnostics",
      Napi::Function::New(env, ExportDiagnostics));
  exports.Set("startRebuild", Napi::Function::New(env, StartRebuild));
  exports.Set("waitForRebuild", Napi::Function::New(env, WaitForRebuild));
  return exports;
}

}  // namespace

NODE_API_MODULE(kernel_host_binding, Init)
