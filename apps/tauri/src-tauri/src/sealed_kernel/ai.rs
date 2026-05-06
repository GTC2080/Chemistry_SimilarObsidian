use std::os::raw::c_char;
#[cfg(test)]
use std::os::raw::c_float;

use crate::models::NoteInfo;
use crate::{AppError, AppResult};

use super::ffi::*;
use super::types::{AiEmbeddingRefreshJob, SealedKernelAiEmbeddingRefreshJobCatalog};
use super::*;

pub fn compute_ai_embedding_cache_key(
    base_url: &str,
    model: &str,
    text: &str,
) -> AppResult<String> {
    let mut raw_key: *mut c_char = std::ptr::null_mut();
    let mut error: *mut c_char = std::ptr::null_mut();
    let code = unsafe {
        sealed_kernel_bridge_compute_ai_embedding_cache_key(
            base_url.as_ptr() as *const c_char,
            base_url.len() as u64,
            model.as_ptr() as *const c_char,
            model.len() as u64,
            text.as_ptr() as *const c_char,
            text.len() as u64,
            &mut raw_key,
            &mut error,
        )
    };
    if code != 0 {
        return Err(product_text_bridge_error(
            "sealed_kernel_compute_ai_embedding_cache_key",
            code,
            error,
        ));
    }
    Ok(take_bridge_string(raw_key))
}
#[cfg(test)]
pub fn serialize_ai_embedding_blob(values: &[f32]) -> AppResult<Vec<u8>> {
    let mut raw_bytes: *mut u8 = std::ptr::null_mut();
    let mut byte_count = 0u64;
    let mut error: *mut c_char = std::ptr::null_mut();
    let code = unsafe {
        sealed_kernel_bridge_serialize_ai_embedding_blob(
            values.as_ptr() as *const c_float,
            values.len() as u64,
            &mut raw_bytes,
            &mut byte_count,
            &mut error,
        )
    };
    if code != 0 {
        return Err(bridge_error(
            "sealed_kernel_serialize_ai_embedding_blob",
            code,
            error,
        ));
    }
    let Ok(byte_count) = usize::try_from(byte_count) else {
        unsafe {
            sealed_kernel_bridge_free_bytes(raw_bytes);
        }
        return Err(AppError::Custom(
            "Embedding blob 内核结果过大，无法复制".to_string(),
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
#[cfg(test)]
pub fn parse_ai_embedding_blob(blob: &[u8]) -> AppResult<Vec<f32>> {
    let mut raw_values: *mut c_float = std::ptr::null_mut();
    let mut value_count = 0u64;
    let mut error: *mut c_char = std::ptr::null_mut();
    let code = unsafe {
        sealed_kernel_bridge_parse_ai_embedding_blob(
            blob.as_ptr(),
            blob.len() as u64,
            &mut raw_values,
            &mut value_count,
            &mut error,
        )
    };
    if code != 0 {
        return Err(bridge_error(
            "sealed_kernel_parse_ai_embedding_blob",
            code,
            error,
        ));
    }
    let Ok(value_count) = usize::try_from(value_count) else {
        unsafe {
            sealed_kernel_bridge_free_float_array(raw_values);
        }
        return Err(AppError::Custom(
            "Embedding blob 内核解析结果过大，无法复制".to_string(),
        ));
    };
    if raw_values.is_null() || value_count == 0 {
        return Ok(Vec::new());
    }
    let values =
        unsafe { std::slice::from_raw_parts(raw_values as *const f32, value_count).to_vec() };
    unsafe {
        sealed_kernel_bridge_free_float_array(raw_values);
    }
    Ok(values)
}

fn parse_ai_embedding_refresh_jobs_json(value: String) -> AppResult<Vec<AiEmbeddingRefreshJob>> {
    let catalog: SealedKernelAiEmbeddingRefreshJobCatalog =
        serde_json::from_str(&value).map_err(|err| {
            AppError::Custom(format!(
                "sealed kernel AI embedding refresh jobs JSON is invalid: {err}"
            ))
        })?;
    Ok(catalog
        .jobs
        .into_iter()
        .map(|job| AiEmbeddingRefreshJob {
            id: job.rel_path.replace('\\', "/"),
            content: job.content,
        })
        .collect())
}

pub fn prepare_ai_embedding_refresh_jobs(
    ignored_roots: &str,
    force_refresh: bool,
    state: &SealedKernelState,
) -> AppResult<Vec<AiEmbeddingRefreshJob>> {
    let session = active_session(state)?;
    let limit = note_catalog_default_limit()?;
    let ignored_roots = cstring_arg(ignored_roots.to_string(), "ignored_roots")?;
    let mut raw_json: *mut c_char = std::ptr::null_mut();
    let mut error: *mut c_char = std::ptr::null_mut();
    let code = unsafe {
        sealed_kernel_bridge_prepare_ai_embedding_refresh_jobs_json(
            session,
            ignored_roots.as_ptr(),
            limit,
            u8::from(force_refresh),
            &mut raw_json,
            &mut error,
        )
    };
    if code != 0 {
        return Err(bridge_error(
            "sealed_kernel_prepare_ai_embedding_refresh_jobs",
            code,
            error,
        ));
    }
    parse_ai_embedding_refresh_jobs_json(take_bridge_string(raw_json))
}

pub fn prepare_changed_ai_embedding_refresh_jobs(
    paths: &[String],
    state: &SealedKernelState,
) -> AppResult<Vec<AiEmbeddingRefreshJob>> {
    if paths.is_empty() {
        return Ok(Vec::new());
    }

    let session = active_session(state)?;
    let limit = note_catalog_default_limit()?;
    let changed_paths = cstring_arg(paths.join("\n"), "changed_paths")?;
    let mut raw_json: *mut c_char = std::ptr::null_mut();
    let mut error: *mut c_char = std::ptr::null_mut();
    let code = unsafe {
        sealed_kernel_bridge_prepare_changed_ai_embedding_refresh_jobs_json(
            session,
            changed_paths.as_ptr(),
            limit,
            &mut raw_json,
            &mut error,
        )
    };
    if code != 0 {
        return Err(bridge_error(
            "sealed_kernel_prepare_changed_ai_embedding_refresh_jobs",
            code,
            error,
        ));
    }
    parse_ai_embedding_refresh_jobs_json(take_bridge_string(raw_json))
}

pub fn update_ai_embedding_for_session(
    session: usize,
    note_id: &str,
    embedding: &[f32],
) -> AppResult<()> {
    if embedding.is_empty() {
        return Err(AppError::Custom(
            "embedding vector must not be empty.".to_string(),
        ));
    }
    let session = session_from_token(session)?;
    let note_id = cstring_arg(note_id.to_string(), "note_rel_path")?;
    call_status_operation("sealed_kernel_update_ai_embedding", |error| unsafe {
        sealed_kernel_bridge_update_ai_embedding(
            session,
            note_id.as_ptr(),
            embedding.as_ptr(),
            embedding.len() as u64,
            error,
        )
    })
}

pub fn clear_ai_embeddings(state: &SealedKernelState) -> AppResult<()> {
    let session = active_session(state)?;
    call_status_operation("sealed_kernel_clear_ai_embeddings", |error| unsafe {
        sealed_kernel_bridge_clear_ai_embeddings(session, error)
    })
}

pub fn delete_changed_ai_embedding_notes(
    paths: &[String],
    state: &SealedKernelState,
) -> AppResult<u64> {
    let session = active_session(state)?;
    let joined = paths.join("\n");
    let paths_c = cstring_arg(joined, "changed_paths")?;
    let mut deleted_count = 0u64;
    call_status_operation(
        "sealed_kernel_delete_changed_ai_embedding_notes",
        |error| unsafe {
            sealed_kernel_bridge_delete_changed_ai_embedding_notes(
                session,
                paths_c.as_ptr(),
                &mut deleted_count,
                error,
            )
        },
    )?;
    Ok(deleted_count)
}

pub fn query_ai_embedding_top_note_infos(
    query_embedding: &[f32],
    limit: u64,
    exclude_id: Option<&str>,
    state: &SealedKernelState,
) -> AppResult<Vec<NoteInfo>> {
    if query_embedding.is_empty() || limit == 0 {
        return Err(AppError::Custom(
            "embedding query and limit must be non-empty.".to_string(),
        ));
    }
    let vault_path = active_vault_path(state)?;
    let exclude_id_c = exclude_id
        .map(|value| cstring_arg(value.to_string(), "exclude_rel_path"))
        .transpose()?;
    let exclude_ptr = exclude_id_c
        .as_ref()
        .map_or(std::ptr::null(), |value| value.as_ptr());

    let catalog = query_note_records_with_json(
        "sealed_kernel_query_ai_embedding_top_notes",
        state,
        |session, raw_json, error| unsafe {
            sealed_kernel_bridge_query_ai_embedding_top_notes_json(
                session,
                query_embedding.as_ptr(),
                query_embedding.len() as u64,
                exclude_ptr,
                limit,
                raw_json,
                error,
            )
        },
    )?;

    Ok(catalog
        .notes
        .into_iter()
        .map(|record| note_info_from_record(&vault_path, record))
        .collect())
}

pub fn build_ai_rag_system_content(context: &str) -> AppResult<String> {
    let mut raw_text: *mut c_char = std::ptr::null_mut();
    let mut error: *mut c_char = std::ptr::null_mut();
    let code = unsafe {
        sealed_kernel_bridge_build_ai_rag_system_content_text(
            context.as_ptr() as *const c_char,
            context.len() as u64,
            &mut raw_text,
            &mut error,
        )
    };
    if code != 0 {
        return Err(product_text_bridge_error(
            "sealed_kernel_build_ai_rag_system_content",
            code,
            error,
        ));
    }
    Ok(take_bridge_string(raw_text))
}
#[cfg(test)]
pub fn build_ai_rag_context_from_note_paths(notes: &[(String, String)]) -> AppResult<String> {
    let note_path_ptrs: Vec<*const c_char> = notes
        .iter()
        .map(|(path, _)| path.as_ptr() as *const c_char)
        .collect();
    let note_path_sizes: Vec<u64> = notes.iter().map(|(path, _)| path.len() as u64).collect();
    let note_content_ptrs: Vec<*const c_char> = notes
        .iter()
        .map(|(_, content)| content.as_ptr() as *const c_char)
        .collect();
    let note_content_sizes: Vec<u64> = notes
        .iter()
        .map(|(_, content)| content.len() as u64)
        .collect();

    let mut raw_text: *mut c_char = std::ptr::null_mut();
    let mut error: *mut c_char = std::ptr::null_mut();
    let code = unsafe {
        sealed_kernel_bridge_build_ai_rag_context_from_note_paths_text(
            note_path_ptrs.as_ptr(),
            note_path_sizes.as_ptr(),
            note_content_ptrs.as_ptr(),
            note_content_sizes.as_ptr(),
            notes.len() as u64,
            &mut raw_text,
            &mut error,
        )
    };
    if code != 0 {
        return Err(product_text_bridge_error(
            "sealed_kernel_build_ai_rag_context_from_note_paths",
            code,
            error,
        ));
    }
    Ok(take_bridge_string(raw_text))
}

pub fn build_ai_rag_context_from_note_ids(
    note_ids: impl IntoIterator<Item = String>,
    state: &SealedKernelState,
) -> AppResult<String> {
    let session = active_session(state)?;
    let joined = note_ids.into_iter().collect::<Vec<_>>().join("\n");
    let note_paths = cstring_arg(joined, "note_paths")?;
    let mut raw_text: *mut c_char = std::ptr::null_mut();
    let mut error: *mut c_char = std::ptr::null_mut();
    let code = unsafe {
        sealed_kernel_bridge_build_ai_rag_context_from_changed_note_paths_text(
            session,
            note_paths.as_ptr(),
            &mut raw_text,
            &mut error,
        )
    };
    if code != 0 {
        return Err(product_text_bridge_error(
            "sealed_kernel_build_ai_rag_context_from_changed_note_paths",
            code,
            error,
        ));
    }
    Ok(take_bridge_string(raw_text))
}

pub fn ai_ponder_system_prompt() -> AppResult<String> {
    let mut raw_text: *mut c_char = std::ptr::null_mut();
    let mut error: *mut c_char = std::ptr::null_mut();
    let code =
        unsafe { sealed_kernel_bridge_get_ai_ponder_system_prompt_text(&mut raw_text, &mut error) };
    if code != 0 {
        return Err(product_text_bridge_error(
            "sealed_kernel_get_ai_ponder_system_prompt",
            code,
            error,
        ));
    }
    Ok(take_bridge_string(raw_text))
}

pub fn build_ai_ponder_user_prompt(topic: &str, context: &str) -> AppResult<String> {
    let mut raw_text: *mut c_char = std::ptr::null_mut();
    let mut error: *mut c_char = std::ptr::null_mut();
    let code = unsafe {
        sealed_kernel_bridge_build_ai_ponder_user_prompt_text(
            topic.as_ptr() as *const c_char,
            topic.len() as u64,
            context.as_ptr() as *const c_char,
            context.len() as u64,
            &mut raw_text,
            &mut error,
        )
    };
    if code != 0 {
        return Err(product_text_bridge_error(
            "sealed_kernel_build_ai_ponder_user_prompt",
            code,
            error,
        ));
    }
    Ok(take_bridge_string(raw_text))
}
