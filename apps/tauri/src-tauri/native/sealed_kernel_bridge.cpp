#include "sealed_kernel_bridge_internal.h"

using namespace sealed_kernel_bridge_internal;

char* sealed_kernel_bridge_info_json(void) {
  return CopyString(
      "{\"adapter\":\"sealed-kernel-cpp-bridge\","
      "\"kernel\":\"chem_kernel\","
      "\"link_mode\":\"static-lib\","
      "\"path_encoding\":\"utf8-to-active-code-page\"}");
}

void sealed_kernel_bridge_free_string(char* value) {
  std::free(value);
}

void sealed_kernel_bridge_free_bytes(uint8_t* value) {
  std::free(value);
}

void sealed_kernel_bridge_free_float_array(float* value) {
  std::free(value);
}

int32_t sealed_kernel_bridge_open_vault_utf8(
    const char* vault_path_utf8,
    sealed_kernel_bridge_session** out_session,
    char** out_error) {
  if (out_session == nullptr) {
    SetError(out_error, "sealed_kernel_bridge_open_vault_utf8 missing out_session.");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  *out_session = nullptr;

  const std::string vault_path = Utf8ToActiveCodePage(vault_path_utf8);
  if (vault_path.empty()) {
    SetError(out_error, "vault_path must be a non-empty UTF-8 string.");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  kernel_handle* handle = nullptr;
  const kernel_status status = kernel_open_vault(vault_path.c_str(), &handle);
  if (status.code != KERNEL_OK) {
    return ReturnKernelError(status, "kernel_open_vault", out_error);
  }

  auto* session = new sealed_kernel_bridge_session();
  session->handle = handle;
  *out_session = session;
  return static_cast<int32_t>(KERNEL_OK);
}

int32_t sealed_kernel_bridge_validate_vault_root_utf8(
    const char* vault_path_utf8,
    char** out_error) {
  const std::string vault_path = Utf8ToActiveCodePage(vault_path_utf8);
  if (vault_path.empty()) {
    SetError(out_error, "vault_path must be a non-empty UTF-8 string.");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  const kernel_status status = kernel_validate_vault_root(vault_path.c_str());
  if (status.code != KERNEL_OK) {
    return ReturnKernelError(status, "kernel_validate_vault_root", out_error);
  }

  return static_cast<int32_t>(KERNEL_OK);
}

void sealed_kernel_bridge_close(sealed_kernel_bridge_session* session) {
  if (session == nullptr) {
    return;
  }

  if (session->handle != nullptr) {
    kernel_close(session->handle);
    session->handle = nullptr;
  }
  delete session;
}

int32_t sealed_kernel_bridge_get_state(
    sealed_kernel_bridge_session* session,
    sealed_kernel_bridge_state_snapshot* out_state,
    char** out_error) {
  if (session == nullptr || session->handle == nullptr || out_state == nullptr) {
    SetError(out_error, "sealed kernel session is not open.");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  kernel_state_snapshot snapshot{};
  const kernel_status status = kernel_get_state(session->handle, &snapshot);
  if (status.code != KERNEL_OK) {
    return ReturnKernelError(status, "kernel_get_state", out_error);
  }

  out_state->session_state = static_cast<int32_t>(snapshot.session_state);
  out_state->index_state = static_cast<int32_t>(snapshot.index_state);
  out_state->indexed_note_count = snapshot.indexed_note_count;
  out_state->pending_recovery_ops = snapshot.pending_recovery_ops;
  return static_cast<int32_t>(KERNEL_OK);
}
