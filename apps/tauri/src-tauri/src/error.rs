use serde::Serialize;

#[derive(Debug, thiserror::Error)]
pub enum AppError {
    #[error("数据库错误: {0}")]
    Database(#[from] rusqlite::Error),

    #[error("IO 错误: {0}")]
    Io(#[from] std::io::Error),

    #[error("网络请求错误: {0}")]
    Network(#[from] reqwest::Error),

    #[error("获取锁失败")]
    Lock,

    #[error("时间错误: {0}")]
    Time(#[from] std::time::SystemTimeError),

    #[error("PDF 引擎错误: {0}")]
    PdfEngine(String),

    #[error("PDF 渲染错误: {0}")]
    PdfRender(String),

    #[error("PDF 批注错误: {0}")]
    PdfAnnotation(String),

    #[error("{0}")]
    Custom(String),
}

impl Serialize for AppError {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: serde::Serializer,
    {
        serializer.serialize_str(self.to_string().as_str())
    }
}

impl From<String> for AppError {
    fn from(s: String) -> Self {
        AppError::Custom(s)
    }
}

impl From<&str> for AppError {
    fn from(s: &str) -> Self {
        AppError::Custom(s.to_string())
    }
}

pub type AppResult<T> = Result<T, AppError>;
