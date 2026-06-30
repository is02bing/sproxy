use std::fmt;
use std::io::{stderr, stdout, Write};
use std::time::SystemTime;

/// 日志级别
#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord)]
pub enum Level {
    Trace,
    Debug,
    Info,
    Warn,
    Error,
}

impl fmt::Display for Level {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Level::Trace => write!(f, "V"),
            Level::Debug => write!(f, "D"),
            Level::Info => write!(f, "I"),
            Level::Warn => write!(f, "W"),
            Level::Error => write!(f, "E"),
        }
    }
}

/// 全局日志级别（默认 Info）
static mut LOG_LEVEL: Level = Level::Info;

/// 设置全局日志级别
pub fn set_log_level(level: Level) {
    unsafe { LOG_LEVEL = level };
}

/// 获取当前日志级别
pub fn log_level() -> Level {
    unsafe { LOG_LEVEL }
}

/// 从字符串解析日志级别
pub fn level_from_str(s: &str) -> Option<Level> {
    match s.to_lowercase().as_str() {
        "trace" => Some(Level::Trace),
        "debug" => Some(Level::Debug),
        "info" => Some(Level::Info),
        "warn" => Some(Level::Warn),
        "error" => Some(Level::Error),
        _ => None,
    }
}

/// 格式化时间: MM-DD HH:MM:SS.us (与 C 端 trace 格式对齐)
fn format_timestamp() -> String {
    let duration = SystemTime::now()
        .duration_since(SystemTime::UNIX_EPOCH)
        .unwrap_or_default();
    let secs = duration.as_secs();
    let micros = duration.subsec_micros();

    let days = secs / 86400;
    let time_of_day = secs % 86400;
    let hours = time_of_day / 3600;
    let minutes = (time_of_day % 3600) / 60;
    let seconds = time_of_day % 60;

    let (_year, month, day) = days_to_date(days);

    format!(
        "{:02}-{:02} {:02}:{:02}:{:02}.{:06}",
        month, day, hours, minutes, seconds, micros
    )
}

/// 将自 1970-01-01 以来的天数转换为 (年, 月, 日)
fn days_to_date(days_since_epoch: u64) -> (u64, u64, u64) {
    let z = days_since_epoch + 719468;
    let era = z / 146097;
    let doe = z - era * 146097;
    let yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    let y = yoe + era * 400;
    let doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    let mp = (5 * doy + 2) / 153;
    let d = doy - (153 * mp + 2) / 5 + 1;
    let m = if mp < 10 { mp + 3 } else { mp - 9 };
    let y = if m <= 2 { y + 1 } else { y };
    (y, m, d)
}

/// 核心日志函数
/// 格式与 C 端 trace_write 对齐:
///  - debug/trace 级别（log_level <= Debug）使用双行模板：
///    （空行）
///    [I] MM-DD HH:MM:SS.us file:line:
///    message
///  - info 及以上使用单行模板：
///    [I][MM-DD HH:MM:SS.us] file:line message
pub fn log(level: Level, file: &str, line: u32, _module_path: &str, args: fmt::Arguments<'_>) {
    if level < log_level() {
        return;
    }

    let timestamp = format_timestamp();
    let short_file = file.rsplit('/').next().unwrap_or(file);

    let output = if log_level() <= Level::Debug {
        // 双行模板（debug/trace）
        format!(
            "\n[{}] {} {}:{}:\n{}\n",
            level, timestamp, short_file, line, args
        )
    } else {
        // 单行模板（info 及以上）
        format!(
            "[{}][{}] {}:{} {}\n",
            level, timestamp, short_file, line, args
        )
    };

    if level >= Level::Warn {
        let mut writer = stderr().lock();
        let _ = writer.write_all(output.as_bytes());
        let _ = writer.flush();
    } else {
        let mut writer = stdout().lock();
        let _ = writer.write_all(output.as_bytes());
        let _ = writer.flush();
    }
}

/// 日志宏：自动捕获文件名、行号、模块路径
#[macro_export]
macro_rules! log_trace {
    ($($arg:tt)*) => {
        $crate::logger::log($crate::logger::Level::Trace, file!(), line!(), module_path!(), format_args!($($arg)*))
    };
}

#[macro_export]
macro_rules! log_debug {
    ($($arg:tt)*) => {
        $crate::logger::log($crate::logger::Level::Debug, file!(), line!(), module_path!(), format_args!($($arg)*))
    };
}

#[macro_export]
macro_rules! log_info {
    ($($arg:tt)*) => {
        $crate::logger::log($crate::logger::Level::Info, file!(), line!(), module_path!(), format_args!($($arg)*))
    };
}

#[macro_export]
macro_rules! log_warn {
    ($($arg:tt)*) => {
        $crate::logger::log($crate::logger::Level::Warn, file!(), line!(), module_path!(), format_args!($($arg)*))
    };
}

#[macro_export]
macro_rules! log_error {
    ($($arg:tt)*) => {
        $crate::logger::log($crate::logger::Level::Error, file!(), line!(), module_path!(), format_args!($($arg)*))
    };
}
