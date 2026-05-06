use std::ffi::CString;
use std::os::raw::c_char;

use crate::{AppError, AppResult};

use super::ffi::*;
use super::types::SealedKernelReadNoteResult;
use super::*;

pub(crate) fn relativize_vault_path(
    file_path: &str,
    allow_empty: bool,
    state: &SealedKernelState,
) -> AppResult<String> {
    let session = active_session(state)?;
    let mut raw_text: *mut c_char = std::ptr::null_mut();
    let mut error: *mut c_char = std::ptr::null_mut();
    let code = unsafe {
        sealed_kernel_bridge_relativize_vault_path_text(
            session,
            file_path.as_ptr() as *const c_char,
            file_path.len() as u64,
            u8::from(allow_empty),
            &mut raw_text,
            &mut error,
        )
    };
    if code != 0 {
        return Err(bridge_error(
            "sealed_kernel_relativize_vault_path",
            code,
            error,
        ));
    }
    Ok(take_bridge_string(raw_text))
}

pub(super) fn validate_rel_path(rel_path: &str, label: &str) -> AppResult<String> {
    normalize_vault_relative_path(rel_path)
        .map_err(|_| AppError::Custom(format!("非法{label}路径")))
}

pub(super) fn rel_path_from_file_path(
    file_path: &str,
    state: &SealedKernelState,
    label: &str,
) -> AppResult<String> {
    relativize_vault_path(file_path, false, state)
        .map_err(|_| AppError::Custom(format!("{label}不在当前 vault 内或路径非法: {file_path}")))
}

pub(super) fn rel_path_from_optional_folder_path(
    folder_path: &str,
    state: &SealedKernelState,
) -> AppResult<String> {
    relativize_vault_path(folder_path, true, state)
        .map_err(|_| AppError::Custom(format!("文件夹不在当前 vault 内或路径非法: {folder_path}")))
}

pub fn read_note_by_file_path(file_path: &str, state: &SealedKernelState) -> AppResult<String> {
    let rel_path = rel_path_from_file_path(file_path, state, "文件")?;
    read_note_by_rel_path(&rel_path, state)
}

pub fn read_note_by_rel_path(rel_path: &str, state: &SealedKernelState) -> AppResult<String> {
    let rel_path = validate_rel_path(rel_path, "笔记")?;
    let rel_path_c = cstring_arg(rel_path, "rel_path")?;
    let session = active_session(state)?;

    let mut raw_json: *mut c_char = std::ptr::null_mut();
    let mut error: *mut c_char = std::ptr::null_mut();
    let code = unsafe {
        sealed_kernel_bridge_read_note_json(session, rel_path_c.as_ptr(), &mut raw_json, &mut error)
    };
    if code != 0 {
        return Err(bridge_error("sealed_kernel_read_note", code, error));
    }

    let value = take_bridge_string(raw_json);
    let parsed: SealedKernelReadNoteResult = serde_json::from_str(&value).map_err(|err| {
        AppError::Custom(format!("sealed kernel note read JSON is invalid: {err}"))
    })?;
    Ok(parsed.content)
}

pub fn read_first_changed_markdown_note_content(
    note_ids: impl IntoIterator<Item = String>,
    state: &SealedKernelState,
) -> AppResult<String> {
    let session = active_session(state)?;
    let joined = note_ids.into_iter().collect::<Vec<_>>().join("\n");
    let changed_paths = cstring_arg(joined, "changed_paths")?;
    let mut raw_text: *mut c_char = std::ptr::null_mut();
    let mut error: *mut c_char = std::ptr::null_mut();
    let code = unsafe {
        sealed_kernel_bridge_read_first_changed_markdown_note_content_text(
            session,
            changed_paths.as_ptr(),
            &mut raw_text,
            &mut error,
        )
    };
    if code != 0 {
        return Err(bridge_error(
            "sealed_kernel_read_first_changed_markdown_note_content",
            code,
            error,
        ));
    }
    Ok(take_bridge_string(raw_text))
}

pub fn read_vault_file_bytes_for_session(session: usize, file_path: &str) -> AppResult<Vec<u8>> {
    let session = session_from_token(session)?;
    let mut raw_bytes: *mut u8 = std::ptr::null_mut();
    let mut byte_count = 0u64;
    let mut error: *mut c_char = std::ptr::null_mut();
    let code = unsafe {
        sealed_kernel_bridge_read_vault_file_bytes(
            session,
            file_path.as_ptr() as *const c_char,
            file_path.len() as u64,
            &mut raw_bytes,
            &mut byte_count,
            &mut error,
        )
    };
    if code != 0 {
        return Err(bridge_error("sealed_kernel_read_vault_file", code, error));
    }

    let Ok(byte_count) = usize::try_from(byte_count) else {
        unsafe {
            sealed_kernel_bridge_free_bytes(raw_bytes);
        }
        return Err(AppError::Custom(
            "kernel file read result is too large to copy.".to_string(),
        ));
    };
    if raw_bytes.is_null() || byte_count == 0 {
        return Ok(Vec::new());
    }
    let bytes = unsafe { std::slice::from_raw_parts(raw_bytes, byte_count).to_vec() };
    unsafe {
        sealed_kernel_bridge_free_bytes(raw_bytes);
    }
    Ok(bytes)
}

pub fn write_note_by_file_path(
    vault_path: &str,
    file_path: &str,
    content: &str,
    state: &SealedKernelState,
) -> AppResult<()> {
    ensure_vault_open(vault_path, state)?;
    let rel_path =
        normalize_vault_relative_path(&rel_path_from_file_path(file_path, state, "文件")?)
            .map_err(|_| AppError::Custom("非法笔记路径".to_string()))?;
    let rel_path_c = cstring_arg(rel_path, "rel_path")?;
    let content_c = CString::new(content)
        .map_err(|_| AppError::Custom("content must not contain NUL bytes.".to_string()))?;
    let session = active_session(state)?;

    let mut raw_json: *mut c_char = std::ptr::null_mut();
    let mut error: *mut c_char = std::ptr::null_mut();
    let code = unsafe {
        sealed_kernel_bridge_write_note_json(
            session,
            rel_path_c.as_ptr(),
            content_c.as_ptr(),
            content.len() as u64,
            &mut raw_json,
            &mut error,
        )
    };
    if code != 0 {
        return Err(bridge_error("sealed_kernel_write_note", code, error));
    }

    let _ = take_bridge_string(raw_json);
    Ok(())
}

pub fn create_folder_by_path(
    vault_path: &str,
    folder_path: &str,
    state: &SealedKernelState,
) -> AppResult<()> {
    ensure_vault_open(vault_path, state)?;
    let rel_path = rel_path_from_file_path(folder_path, state, "文件夹")?;
    let rel_path_c = cstring_arg(rel_path, "folder_rel_path")?;
    let session = active_session(state)?;
    call_status_operation("sealed_kernel_create_folder", |error| unsafe {
        sealed_kernel_bridge_create_folder(session, rel_path_c.as_ptr(), error)
    })
}

pub fn delete_entry_by_path(
    vault_path: &str,
    target_path: &str,
    state: &SealedKernelState,
) -> AppResult<()> {
    ensure_vault_open(vault_path, state)?;
    let rel_path = rel_path_from_file_path(target_path, state, "条目")?;
    let rel_path_c = cstring_arg(rel_path, "target_rel_path")?;
    let session = active_session(state)?;
    call_status_operation("sealed_kernel_delete_entry", |error| unsafe {
        sealed_kernel_bridge_delete_entry(session, rel_path_c.as_ptr(), error)
    })
}

pub fn rename_entry_by_path(
    vault_path: &str,
    source_path: &str,
    new_name: &str,
    state: &SealedKernelState,
) -> AppResult<()> {
    ensure_vault_open(vault_path, state)?;
    let rel_path = rel_path_from_file_path(source_path, state, "条目")?;
    let rel_path_c = cstring_arg(rel_path, "source_rel_path")?;
    let new_name_c = cstring_arg(new_name.to_string(), "new_name")?;
    let session = active_session(state)?;
    call_status_operation("sealed_kernel_rename_entry", |error| unsafe {
        sealed_kernel_bridge_rename_entry(session, rel_path_c.as_ptr(), new_name_c.as_ptr(), error)
    })
}

pub fn move_entry_by_path(
    vault_path: &str,
    source_path: &str,
    dest_folder: &str,
    state: &SealedKernelState,
) -> AppResult<()> {
    ensure_vault_open(vault_path, state)?;
    let source_rel_path = rel_path_from_file_path(source_path, state, "条目")?;
    let dest_rel_path = rel_path_from_optional_folder_path(dest_folder, state)?;
    let source_rel_path_c = cstring_arg(source_rel_path, "source_rel_path")?;
    let dest_rel_path_c = if dest_rel_path.is_empty() {
        None
    } else {
        Some(cstring_arg(dest_rel_path, "dest_folder_rel_path")?)
    };
    let dest_ptr = dest_rel_path_c
        .as_ref()
        .map_or(std::ptr::null(), |value| value.as_ptr());
    let session = active_session(state)?;
    call_status_operation("sealed_kernel_move_entry", |error| unsafe {
        sealed_kernel_bridge_move_entry(session, source_rel_path_c.as_ptr(), dest_ptr, error)
    })
}
