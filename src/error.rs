use std::fmt;

/// nftables 操作错误
#[derive(Debug)]
#[allow(dead_code)]
pub enum NftError {
    /// 权限不足
    PermissionDenied,
    /// 规则不存在
    RuleNotFound,
    /// 链不存在
    ChainNotFound,
    /// 表不存在
    TableNotFound,
    /// 无效参数
    InvalidArgument(String),
    /// 其他错误
    Other(String),
}

impl fmt::Display for NftError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            NftError::PermissionDenied => {
                write!(f, "permission denied, root required(CAP_NET_ADMIN)")
            }
            NftError::RuleNotFound => write!(f, "rule not found"),
            NftError::ChainNotFound => write!(f, "chain not found"),
            NftError::TableNotFound => write!(f, "table not found"),
            NftError::InvalidArgument(msg) => write!(f, "invalid argument: {}", msg),
            NftError::Other(msg) => write!(f, "{}", msg),
        }
    }
}

impl std::error::Error for NftError {}
