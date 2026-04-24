//! 内存向量缓存：避免每次查询都从 SQLite 反序列化所有 embedding。
//!
//! 设计要点：
//! - 首次查询时从 DB 加载全部 embedding 到内存
//! - 后续查询直接走内存，不再访问 DB
//! - 笔记新增/更新时，同步更新缓存
//! - 笔记删除时，从缓存中移除
//! - 使用 top-k BinaryHeap 替代全量排序

use std::cmp::Ordering;
use std::collections::BinaryHeap;
use std::collections::HashMap;
use std::sync::{Arc, Mutex};

use rusqlite::Connection;

use crate::models::NoteInfo;
use crate::AppResult;

use super::similarity::cosine_similarity;

/// 缓存中每个条目
struct CacheEntry {
    note: NoteInfo,
    embedding: Vec<f32>,
}

/// 用于 top-k 的带分数条目（小顶堆：堆顶是最小值，方便淘汰）
struct ScoredEntry {
    score: f32,
    note: NoteInfo,
}

impl PartialEq for ScoredEntry {
    fn eq(&self, other: &Self) -> bool {
        self.score == other.score
    }
}

impl Eq for ScoredEntry {}

impl PartialOrd for ScoredEntry {
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        Some(self.cmp(other))
    }
}

impl Ord for ScoredEntry {
    fn cmp(&self, other: &Self) -> Ordering {
        // Reverse the default ordering so BinaryHeap behaves like a min-heap.
        // That keeps the worst item in the current top-k at the root.
        other
            .score
            .partial_cmp(&self.score)
            .unwrap_or(Ordering::Equal)
    }
}

/// 全局向量缓存状态，通过 Tauri State 管理
#[derive(Clone)]
pub struct VectorCacheState {
    inner: Arc<Mutex<VectorCacheInner>>,
}

struct VectorCacheInner {
    /// note_id -> CacheEntry
    entries: HashMap<String, CacheEntry>,
    /// 是否已从 DB 加载过
    loaded: bool,
}

impl Default for VectorCacheState {
    fn default() -> Self {
        Self {
            inner: Arc::new(Mutex::new(VectorCacheInner {
                entries: HashMap::new(),
                loaded: false,
            })),
        }
    }
}

impl VectorCacheState {
    /// 确保缓存已加载。如果尚未加载，从 DB 读取所有 embedding。
    pub fn ensure_loaded(&self, conn: &Connection) -> AppResult<()> {
        let mut inner = self.inner.lock().map_err(|_| crate::AppError::Lock)?;
        if inner.loaded {
            return Ok(());
        }

        let all = crate::db::get_all_embeddings(conn)?;
        for (note, embedding) in all {
            let id = note.id.clone();
            inner.entries.insert(id, CacheEntry { note, embedding });
        }
        inner.loaded = true;
        eprintln!(
            "[vector_cache] 已加载 {} 条 embedding 到内存",
            inner.entries.len()
        );
        Ok(())
    }

    /// 更新单条 embedding（笔记保存/索引后调用）
    pub fn upsert(&self, note: NoteInfo, embedding: Vec<f32>) {
        if let Ok(mut inner) = self.inner.lock() {
            let id = note.id.clone();
            inner.entries.insert(id, CacheEntry { note, embedding });
        }
    }

    /// 移除单条（笔记删除时调用）
    pub fn remove(&self, note_id: &str) {
        if let Ok(mut inner) = self.inner.lock() {
            inner.entries.remove(note_id);
        }
    }

    /// 清空缓存（重建向量索引前调用）
    pub fn clear(&self) {
        if let Ok(mut inner) = self.inner.lock() {
            inner.entries.clear();
            inner.loaded = false;
        }
    }

    /// top-k 相似度查询（用 BinaryHeap 代替全量排序）
    pub fn top_k(
        &self,
        query_embedding: &[f32],
        k: usize,
        exclude_id: Option<&str>,
        conn: &Connection,
    ) -> AppResult<Vec<NoteInfo>> {
        self.ensure_loaded(conn)?;

        let inner = self.inner.lock().map_err(|_| crate::AppError::Lock)?;

        // 小顶堆，容量 k
        let mut heap: BinaryHeap<ScoredEntry> = BinaryHeap::with_capacity(k + 1);

        for (id, entry) in &inner.entries {
            if let Some(exc) = exclude_id {
                if id == exc {
                    continue;
                }
            }

            let score = cosine_similarity(query_embedding, &entry.embedding);

            if heap.len() < k {
                heap.push(ScoredEntry {
                    score,
                    note: entry.note.clone(),
                });
            } else if let Some(top) = heap.peek() {
                if score > top.score {
                    heap.pop();
                    heap.push(ScoredEntry {
                        score,
                        note: entry.note.clone(),
                    });
                }
            }
        }

        // 从小顶堆中取出并按分数降序排列
        let mut results: Vec<(f32, NoteInfo)> =
            heap.into_iter().map(|e| (e.score, e.note)).collect();
        results.sort_by(|a, b| b.0.partial_cmp(&a.0).unwrap_or(Ordering::Equal));

        Ok(results.into_iter().map(|(_, note)| note).collect())
    }
}
