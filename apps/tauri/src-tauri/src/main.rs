// 防止在 Windows 发布版本中弹出额外的控制台窗口
#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

fn main() {
    nexus_lib::run();
}
